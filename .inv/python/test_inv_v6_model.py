"""Unit, physics, loss, CoreML single-partition, and FP16 tests for V6 architecture & inference."""

import contextlib
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import torch

from export_onnx import export_checkpoint_to_onnx
from forward import TerrainGravityOperator, prism_gz
from infer_prior import predict_prior_and_uncertainty
from losses import CompoundInversionLoss, _internal_unit_contact_mask
from model_swin3d import (
    _ROCK_BOUNDS,
    _ROCK_BOUNDS_V6,
    BorderProjectorConv,
    OddPyramidGravity,
    PotentialFieldTransformer3DV6,
    V6_SIZE_PRESETS,
    _prism_gz_exact_t,
    create_model,
)


def test_v6_presets_shapes_and_aux_keys():
    expected_ch_vox = {"slim": 16, "base": 24, "heavy": 36}
    for sz, ch_v in expected_ch_vox.items():
        cfg = V6_SIZE_PRESETS[sz]
        assert cfg["variant"] == "v6"
        assert cfg["ch_vox"] == ch_v
        m = create_model(64, 64, 32, **cfg).eval()
        assert isinstance(m, PotentialFieldTransformer3DV6)

    model = create_model(64, 64, 32, **V6_SIZE_PRESETS["base"]).eval()
    n_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    assert 2.0e6 <= n_params <= 3.5e6, f"Unexpected V6 base param count: {n_params}"

    x = torch.randn(1, 5, 64, 64)
    x[:, 1] = 0.09375  # exactly z = 3/32 -> top 3 layers are air
    x[:, 2] = 1.0
    x[:, 3] = 0.0
    x[:, 4] = 0.007
    with torch.no_grad():
        pred, aux = model(x, return_aux=True)
        pred_u, lv_u = model.forward_uncertainty(x)

    assert pred.shape == (1, 1, 32, 64, 64)
    assert pred_u.shape == (1, 1, 32, 64, 64)
    assert lv_u.shape == (1, 1, 32, 64, 64)
    assert torch.allclose(pred, pred_u, atol=1e-6)
    assert torch.allclose(aux["log_var"], lv_u, atol=1e-6)
    assert float(pred[:, :, :3].abs().max()) == 0.0, "Air voxels above terrain must be masked to 0"
    assert float(lv_u.min()) >= -8.0 - 1e-5 and float(lv_u.max()) <= 4.0 + 1e-5

    required_keys = {
        "is_v6", "log_var", "coarse_pred", "rho_init", "mask", "occ", "probs",
        "logits", "edt", "res_rms", "coarse_res_rms", "terr", "clr", "aspect", "g_init",
    }
    assert required_keys.issubset(aux.keys())
    assert aux["coarse_pred"].shape == (1, 1, 16, 32, 32)
    assert aux["rho_init"].shape == (1, 1, 32, 64, 64)


def test_v6_odd_pyramid_gravity_accuracy_and_adjoint():
    # Verify exact prism agreement with forward.prism_gz
    u1 = torch.tensor([-25.0, 25.0])
    u2 = torch.tensor([25.0, 75.0])
    w1 = torch.tensor([10.0, 50.0])
    w2 = torch.tensor([60.0, 100.0])
    g_np = prism_gz(u1.numpy(), u2.numpy(), u1.numpy(), u2.numpy(), w1.numpy(), w2.numpy())
    g_t = _prism_gz_exact_t(u1, u2, u1, u2, w1, w2).numpy()
    assert np.allclose(g_t, g_np, rtol=1e-6, atol=1e-7)

    # Steep relief ground + airborne + border-crossing accuracy <= 2.0%
    op2 = OddPyramidGravity(64, 64, 32, cell_xy=50.0, dz_ref=50.0).eval()
    op1 = OddPyramidGravity(32, 32, 16, cell_xy=100.0, dz_ref=100.0).eval()

    yy, xx = torch.meshgrid(torch.linspace(-1, 1, 64), torch.linspace(-1, 1, 64), indexing="ij")
    topo_m = (180.0 * torch.sin(3.14 * xx) * torch.cos(2.5 * yy) + 250.0).unsqueeze(0).unsqueeze(0)
    for asp in (0.55, 1.00, 1.40):
        dz = 50.0 * asp
        ref = TerrainGravityOperator(64, 64, 32, dx=50.0, dy=50.0, dz=dz, h_min=-450.0, h_max=500.0, step=25.0)
        z_g = torch.arange(32, dtype=torch.float32).view(1, 32, 1, 1) * dz
        rho = (
            1.1 * (((xx - 0.2) ** 2 + (yy + 0.1) ** 2 < 0.08) & (z_g >= topo_m + 50.0) & (z_g <= topo_m + 350.0)).float()
            - 0.7 * (((xx + 0.35).abs() < 0.35) & ((yy - 0.3).abs() < 0.35) & (z_g >= topo_m) & (z_g <= topo_m + 180.0)).float()
            + 0.4 * ((z_g > topo_m + 450.0) & (xx > 0.0)).float()
        ) * (z_g >= topo_m).float()
        for clr in (0.35, 80.0, 215.0):
            z_obs = topo_m - clr
            g_ref = ref(rho.unsqueeze(1), z_obs[:, 0])
            g_hat = op2(rho, z_obs, torch.tensor([[[[asp]]]]))
            rel_l2 = float((g_hat - g_ref).pow(2).sum().sqrt() / g_ref.pow(2).sum().sqrt())
            assert rel_l2 <= 0.020, f"Stage2 error {rel_l2:.4f} > 2.0% at asp={asp}, clr={clr}"

    yy1, xx1 = torch.meshgrid(torch.linspace(-1, 1, 32), torch.linspace(-1, 1, 32), indexing="ij")
    topo1 = (180.0 * torch.sin(3.14 * xx1) * torch.cos(2.5 * yy1) + 250.0).unsqueeze(0).unsqueeze(0)
    for asp in (0.55, 1.00, 1.40):
        dz = 100.0 * asp
        ref1 = TerrainGravityOperator(32, 32, 16, dx=100.0, dy=100.0, dz=dz, h_min=-450.0, h_max=500.0, step=25.0)
        z_g1 = torch.arange(16, dtype=torch.float32).view(1, 16, 1, 1) * dz
        rho1 = (
            1.1 * ((xx1 ** 2 + yy1 ** 2 < 0.12) & (z_g1 >= topo1) & (z_g1 <= topo1 + 400.0)).float()
            - 0.6 * ((xx1 > 0.1) & (z_g1 >= topo1) & (z_g1 <= topo1 + 250.0)).float()
        ) * (z_g1 >= topo1).float()
        for clr in (0.35, 80.0):
            z_obs1 = topo1 - clr
            g_ref1 = ref1(rho1.unsqueeze(1), z_obs1[:, 0])
            g_hat1 = op1(rho1, z_obs1, torch.tensor([[[[asp]]]]))
            rel_l2_1 = float((g_hat1 - g_ref1).pow(2).sum().sqrt() / g_ref1.pow(2).sum().sqrt())
            assert rel_l2_1 <= 0.020, f"Stage1 error {rel_l2_1:.4f} > 2.0% at asp={asp}, clr={clr}"

    # Exact transpose adjoint test across both operators
    torch.manual_seed(7)
    for op, d, h in [(op1, 16, 32), (op2, 32, 64)]:
        x = torch.randn(2, d, h, h)
        y = torch.randn(2, 1, h, h)
        z = torch.rand(2, 1, h, h) * 300.0 - 50.0
        a = torch.tensor([[[[0.65]]], [[[1.32]]]])
        lhs = float((op(x, z, a) * y).sum())
        rhs = float((x * op.adjoint(y, z, a)).sum())
        rel_err = abs(lhs - rhs) / (abs(lhs) + abs(rhs) + 1e-8)
        assert rel_err < 1e-4, f"Adjoint mismatch rel_err={rel_err:.2e}"

    # Verify [-500, +500] m clamping on out-of-range stations
    x_test = torch.randn(1, 32, 64, 64)
    z_clip = torch.full((1, 1, 64, 64), -500.0)
    z_far = torch.full((1, 1, 64, 64), -1200.0)
    assert torch.allclose(op2(x_test, z_clip, torch.ones(1, 1, 1, 1)), op2(x_test, z_far, torch.ones(1, 1, 1, 1)), atol=1e-6)


def test_v6_border_projector_invariance_and_bounds():
    for sz in (32, 64):
        bp = BorderProjectorConv(sz, sz).eval()
        yy, xx = torch.meshgrid(torch.linspace(-1, 1, sz), torch.linspace(-1, 1, sz), indexing="ij")
        g = torch.randn(2, 1, sz, sz)
        const_shift = g - 4.25
        planar = g - (1.7 + 2.1 * xx - 1.3 * yy)
        border_plane = g - (0.5 - 3.2 * xx + 0.9 * yy)
        ref_bp = bp(g)
        assert float((bp(const_shift) - ref_bp).abs().max()) < 1e-4
        assert float((bp(planar) - ref_bp).abs().max()) < 1e-4
        assert float((bp(border_plane) - ref_bp).abs().max()) < 1e-4

    # Closed V6 rock bounds (no blind [-0.90, -0.80] gap) while preserving V1..V5 _ROCK_BOUNDS
    assert len(_ROCK_BOUNDS) == 4 and _ROCK_BOUNDS[0][1] < -0.82
    assert _ROCK_BOUNDS_V6[0][1] >= _ROCK_BOUNDS_V6[1][0]
    assert _ROCK_BOUNDS_V6[2][1] >= _ROCK_BOUNDS_V6[3][0]
    assert abs(_ROCK_BOUNDS_V6[3][1] - 1.95) < 1e-6

    # Clearance Ch 4 clamp [0.0, 8.0] enforcement
    m = create_model(64, 64, 32, **V6_SIZE_PRESETS["slim"]).eval()
    x = torch.randn(1, 5, 64, 64)
    x[:, 1] = 0.08
    x[:, 2] = 1.0
    x[:, 3] = 0.0
    x_clamp = x.clone()
    x_clamp[:, 4] = 8.0
    x_over = x.clone()
    x_over[:, 4] = 25.0
    with torch.no_grad():
        assert torch.allclose(m(x_clamp), m(x_over), atol=1e-6)


def test_v6_losses_unit_id_tversky_nll_and_compat():
    torch.manual_seed(11)
    B, D, H, W = 1, 8, 16, 16
    m_true = torch.zeros(B, 1, D, H, W)
    unit_id = torch.zeros(B, 1, D, H, W, dtype=torch.int32)
    # Unit 1: adjacent to Unit 2 with equal density (-0.50), plus smooth interior grading in Unit 3
    m_true[:, :, 2:5, 4:8, 4:8] = -0.50
    unit_id[:, :, 2:5, 4:8, 4:8] = 1
    m_true[:, :, 2:5, 8:12, 4:8] = -0.50
    unit_id[:, :, 2:5, 8:12, 4:8] = 2
    m_true[:, :, 3:6, 4:8, 10:14] = -0.85  # landfill/sediment boundary test
    unit_id[:, :, 3:6, 4:8, 10:14] = 3

    fg = (m_true != 0).float()
    c_no_uid = _internal_unit_contact_mask(m_true, fg, unit_id=None)
    c_with_uid = _internal_unit_contact_mask(m_true, fg, unit_id=unit_id)
    assert float(c_no_uid[:, :, 2:5, 7:9, 4:8].sum()) == 0.0
    assert float(c_with_uid[:, :, 2:5, 7:9, 4:8].sum()) > 0.0

    # Backward compatibility with V5 (unit_id=None, use_v6_losses=False)
    crit = CompoundInversionLoss(lambda_data=0.0, use_v6_losses=False)
    m_pred = m_true.clone().requires_grad_(True)
    loss_v5, stats_v5 = crit(m_pred, m_true)
    assert torch.isfinite(loss_v5)
    assert stats_v5["loss_rho_init"] == 0.0

    # V6 losses with unit_id, rho_init, terr/clr, log_var
    aux_v6 = {
        "is_v6": True,
        "rho_init": m_true * 0.85,
        "log_var": torch.full_like(m_true, -2.0, requires_grad=True),
        "mask": torch.ones_like(m_true),
        "terr": torch.full((B, 1, 1, H, W), 0.125),
        "clr": torch.full((B, 1, 1, H, W), 1.6),
    }
    loss_exact, stats_exact = crit(m_true, m_true, aux=aux_v6, unit_id=unit_id)
    assert torch.isfinite(loss_exact)
    assert stats_exact["loss_rho_init"] > 0.0

    # Asymmetric recall weighting: under-prediction (FN) penalized harder than equal FP
    m_under = m_true * 0.70
    m_over = m_true * 1.30
    l_under, _ = crit(m_under, m_true, aux=aux_v6, unit_id=unit_id)
    l_over, _ = crit(m_over, m_true, aux=aux_v6, unit_id=unit_id)
    assert float(l_under) > float(l_over) * 0.85 and torch.isfinite(l_under) and torch.isfinite(l_over)

    # Gaussian NLL minimum calibration test: optimal log_var minimizes at log(err^2)
    lv_vals = [-6.0, -3.0, float(np.log(0.25 ** 2)), 0.0, 2.0]
    nll_losses = []
    crit_nll = CompoundInversionLoss(lambda_data=0.0, lambda_cls=0.0, beta_nll=0.0, use_v6_losses=True)
    m_err = m_true + 0.25
    for lv_v in lv_vals:
        _, st = crit_nll(m_err, m_true, aux={"is_v6": True, "log_var": torch.full_like(m_true, lv_v)}, unit_id=unit_id)
        nll_losses.append(st["loss_nll"])
    assert np.argmin(nll_losses) == 2, f"Expected NLL min at log(0.25^2), got losses {nll_losses}"

    # End-to-end gradient flow through V6 model + CompoundInversionLoss
    model = create_model(64, 64, 32, **V6_SIZE_PRESETS["slim"]).train()
    x = torch.randn(1, 5, 64, 64)
    x[:, 1] = 0.08
    x[:, 2] = 1.0
    x[:, 3] = 0.0
    x[:, 4] = 0.007
    y_true = torch.zeros(1, 1, 32, 64, 64)
    y_true[:, :, 6:12, 24:40, 24:40] = -0.85
    uid_full = torch.zeros(1, 1, 32, 64, 64, dtype=torch.int32)
    uid_full[:, :, 6:12, 24:40, 24:40] = 1
    pred, aux = model(x, return_aux=True)
    loss, _ = crit(pred, y_true, aux=aux, unit_id=uid_full)
    loss.backward()
    grad_norms = [p.grad.abs().sum().item() for p in model.parameters() if p.requires_grad and p.grad is not None]
    assert len(grad_norms) > 10 and all(np.isfinite(g) for g in grad_norms)


@contextlib.contextmanager
def _resolve_tmp_path(tmp_path):
    if tmp_path is not None:
        yield Path(tmp_path)
    else:
        with tempfile.TemporaryDirectory() as td:
            yield Path(td)


def test_v6_onnx_export_coreml_single_partition_and_fp16(tmp_path=None):
    import onnx
    import onnxruntime as ort

    with _resolve_tmp_path(tmp_path) as work_dir:
        model = create_model(64, 64, 32, **V6_SIZE_PRESETS["slim"]).eval()
        ckpt_path = work_dir / "v6_slim.pt"
        onnx_path = work_dir / "v6_slim.onnx"
        torch.save({
            "model_state_dict": model.state_dict(),
            "arch": dict(V6_SIZE_PRESETS["slim"]),
            "grid": {"nx": 64, "ny": 64, "nz": 32, "cell": 50.0},
        }, ckpt_path)

        export_checkpoint_to_onnx(str(ckpt_path), str(onnx_path), opset=17, verify=True, variant="v6")
        om = onnx.load(str(onnx_path))
        onnx.checker.check_model(om)
        ops = {n.op_type for n in om.graph.node}
        banned = {"Abs", "Neg", "Log", "GreaterOrEqual", "ReduceLogSumExp", "Expand"}
        assert not (ops & banned), f"Unexpected CoreML-hostile ops in ONNX graph: {ops & banned}"

        if "CoreMLExecutionProvider" in ort.get_available_providers():
            # Run in fresh subprocess to capture ORT GetCapability native C++ stdout/stderr reliably
            code = (
                "import onnxruntime as ort; ort.set_default_logger_severity(1); "
                f"ort.InferenceSession({repr(str(onnx_path))}, providers=[('CoreMLExecutionProvider', "
                "{'ModelFormat': 'MLProgram', 'MLComputeUnits': 'ALL', 'RequireStaticInputShapes': '1'}), 'CPUExecutionProvider'])"
            )
            proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=True)
            logs = proc.stdout + proc.stderr
            assert "number of partitions supported by CoreML: 1 " in logs, f"Expected 1 CoreML partition, got:\n{logs}"
            # Extract node counts: "number of nodes in the graph: N number of nodes supported by CoreML: M"
            for line in logs.splitlines():
                if "number of partitions supported by CoreML:" in line:
                    parts = line.split("number of nodes in the graph:")[-1].strip().split()
                    n_total = int(parts[0])
                    n_sup = int(line.split("number of nodes supported by CoreML:")[-1].strip().split()[0])
                    assert n_total == n_sup and n_total > 100, f"CoreML fallback nodes detected: {n_sup}/{n_total}"

    # Verify FP16 safety explicitly across low/high clearance on MPS (when available)
    if torch.backends.mps.is_available():
        m16 = create_model(64, 64, 32, **V6_SIZE_PRESETS["slim"]).to("mps").half().eval()
        m16.load_state_dict(model.state_dict())
        x_base = torch.randn(1, 5, 64, 64, device="mps", dtype=torch.float16)
        x_base[:, 1] = 0.09375
        x_base[:, 2] = 1.0
        x_base[:, 3] = 0.0
        with torch.no_grad():
            for clr_v in (0.007, 4.0, 8.0):
                x16 = x_base.clone()
                x16[:, 4] = clr_v
                d16, lv16 = m16.forward_uncertainty(x16)
                assert torch.isfinite(d16).all() and torch.isfinite(lv16).all()
                assert float(d16[:, :, :3].abs().max()) == 0.0


def test_v6_infer_prior_2d_clearance(tmp_path=None):
    with _resolve_tmp_path(tmp_path) as work_dir:
        model = create_model(64, 64, 32, **V6_SIZE_PRESETS["slim"]).eval()
        ckpt_path = work_dir / "v6_slim.pt"
        torch.save({
            "model_state_dict": model.state_dict(),
            "arch": dict(V6_SIZE_PRESETS["slim"]),
            "grid": {"nx": 64, "ny": 64, "nz": 32, "cell": 50.0},
        }, ckpt_path)

        nx, ny, nz = 32, 32, 16
        dx, dy, dz = 100.0, 100.0, 100.0
        xs, ys = np.meshgrid(np.arange(nx) * dx + 50.0, np.arange(ny) * dy + 50.0, indexing="ij")
        topo_val = 120.0
        topo_nodes = np.full((ny + 1) * (nx + 1), topo_val, dtype=float).tolist()
        # Spatially varying observation elevation (ground 0.35m on left, airborne 90m on right)
        zs = np.where(xs < 1600.0, topo_val - 0.35, topo_val - 90.0)
        gz = np.exp(-((xs - 1600.0) ** 2 + (ys - 1600.0) ** 2) / (400.0 ** 2))
        setup = {
            "voxel_domain": {"nx": nx, "ny": ny, "nz": nz, "dx": dx, "dy": dy, "dz": dz, "x0": 0.0, "y0": 0.0},
            "topography": topo_nodes,
            "observations": {
                "x": xs.ravel().tolist(),
                "y": ys.ravel().tolist(),
                "z": zs.ravel().tolist(),
                "gz": gz.ravel().tolist(),
            },
        }
        prior, sigma = predict_prior_and_uncertainty(setup, str(ckpt_path), device="cpu", variant="v6")
        assert prior.shape == (nx * ny * nz,) and np.isfinite(prior).all()
        assert sigma is not None and sigma.shape == (nx * ny * nz,) and np.isfinite(sigma).all() and (sigma > 0).all()


if __name__ == "__main__":
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok  {name}")
    print(f"{len(tests)} tests passed")
