"""Unit, conditioning, loss, v6.1 warm-start, and CPU training smoke tests for V7."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F

from dataset import (
    ASPECT_MULTI,
    CLEARANCE_CLAMP_V6,
    DETREND_MODE_CODE,
    BodyTerrainDataset,
    build_v6_input,
    build_v7_input,
    simulate_batch,
)
from evaluate import _infer_eval_model_cfg, make_v6_val_buckets
from forward import SubvoxelCapOperator, TerrainGravityOperator
from losses import CompoundInversionLoss
from model_swin3d import (
    OddPyramidGravity,
    PotentialFieldTransformer3DV6,
    PotentialFieldTransformer3DV7,
    TierProlongation3D,
    V6_SIZE_PRESETS,
    V7_SIZE_PRESETS,
    create_model,
)
from train import _load_ckpt_weights_compat


def _make_probe_input(batch: int = 2, channels: int = 6, nx: int = 64, ny: int = 64) -> torch.Tensor:
    torch.manual_seed(17)
    x = torch.randn(batch, channels, nx, ny, dtype=torch.float32)
    x[:, 1] = 0.09375  # z = 3 / 32 -> top 3 layers are air
    x[:, 2] = 1.0
    x[:, 3] = 0.0
    x[:, 4] = 0.007
    if channels >= 6:
        for b in range(batch):
            x[b, 5] = float(b) / max(1, batch - 1)
    return x


def test_v7_presets_slabs_and_5ch_fallback():
    expected_ch_vox = {"slim": 24, "base": 32, "heavy": 40}
    assert set(V7_SIZE_PRESETS.keys()) == {"slim", "base", "heavy"}
    models = {}
    for sz, ch_v in expected_ch_vox.items():
        cfg = V7_SIZE_PRESETS[sz]
        assert cfg["variant"] == "v7"
        assert cfg["in_channels"] == 6
        assert cfg["ch_vox"] == ch_v
        m = create_model(64, 64, 32, **cfg).eval()
        assert isinstance(m, PotentialFieldTransformer3DV7)
        assert m.tier_depths == (8, 8, 4)
        assert m.tier_fine_depths == (8, 12, 12)
        assert m.tier_hw == ((64, 64), (32, 32), (16, 16))
        models[sz] = m

    x6 = _make_probe_input(batch=2, channels=6, nx=64, ny=64)
    required_keys = {
        "is_v7", "tier_preds", "log_var", "coarse_pred", "rho_init", "mask", "occ",
        "probs", "logits", "edt", "res_rms", "coarse_res_rms", "terr", "clr", "aspect", "g_init",
    }
    expected_slab_shapes = ((2, 1, 8, 64, 64), (2, 1, 8, 32, 32), (2, 1, 4, 16, 16))

    for sz in ("slim", "base", "heavy"):
        model = models[sz]
        with torch.no_grad():
            pred, aux = model(x6, return_aux=True)
            pred_u, lv_u = model.forward_uncertainty(x6)

        assert pred.shape == (2, 1, 32, 64, 64)
        assert pred_u.shape == (2, 1, 32, 64, 64)
        assert lv_u.shape == (2, 1, 32, 64, 64)
        assert torch.isfinite(pred).all() and torch.isfinite(lv_u).all()
        assert torch.allclose(pred, pred_u, atol=1e-6)
        assert torch.allclose(aux["log_var"], lv_u, atol=1e-6)
        assert float(pred[:, :, :3].abs().max()) == 0.0, "Air voxels above terrain must be masked to 0"
        assert float(lv_u.min()) >= -8.0 - 1e-5 and float(lv_u.max()) <= 4.0 + 1e-5

        assert required_keys.issubset(aux.keys())
        assert aux["is_v7"] is True
        tier_preds = aux["tier_preds"]
        assert isinstance(tier_preds, tuple) and len(tier_preds) == 3
        assert tuple(t.shape for t in tier_preds) == expected_slab_shapes
        total_slab_voxels = sum(t.shape[2] * t.shape[3] * t.shape[4] for t in tier_preds)
        assert total_slab_voxels == 41984
        assert float(tier_preds[0][:, :, :3].abs().max()) == 0.0
        assert aux["coarse_pred"].shape == (2, 1, 16, 32, 32)
        assert aux["rho_init"].shape == (2, 1, 32, 64, 64)

    # 5-channel fallback: matches 6-channel input with ch5 = 1.0 and responds to ch5 conditioning
    slim_m = models["slim"]
    x6_ones = x6.clone()
    x6_ones[:, 5] = 1.0
    x6_zeros = x6.clone()
    x6_zeros[:, 5] = 0.0
    x5 = x6_ones[:, :5]
    with torch.no_grad():
        pred5, aux5 = slim_m(x5, return_aux=True)
        pred6_ones, aux6_ones = slim_m(x6_ones, return_aux=True)
        pred6_zeros = slim_m(x6_zeros)

    assert pred5.shape == (2, 1, 32, 64, 64)
    assert aux5["log_var"].shape == (2, 1, 32, 64, 64)
    assert aux5["is_v7"] is True
    assert tuple(t.shape for t in aux5["tier_preds"]) == expected_slab_shapes
    assert torch.allclose(pred5, pred6_ones, atol=1e-6)
    assert torch.allclose(aux5["log_var"], aux6_ones["log_var"], atol=1e-6)
    assert not torch.allclose(pred6_zeros, pred6_ones, atol=1e-5)

    # Evaluate auto-detection metadata for V7
    is_v7, is_v6, in_ch, clr_clamp = _infer_eval_model_cfg(slim_m)
    assert is_v7 is True and is_v6 is True and in_ch == 6 and clr_clamp == CLEARANCE_CLAMP_V6


def test_v7_input_detrend_conditioning_and_buckets():
    assert DETREND_MODE_CODE["raw"] == 0.0
    assert DETREND_MODE_CODE["constant_shift"] == 0.33
    assert DETREND_MODE_CODE["planar_ramp"] == 0.67
    assert DETREND_MODE_CODE["border_planar"] == 1.0

    torch.manual_seed(23)
    obs = torch.randn(4, 1, 64, 64)
    terr = torch.rand(4, 64, 64) * 120.0
    vm = (torch.rand(4, 1, 64, 64) > 0.15).float()
    clr = torch.tensor([0.35, 20.0, 80.0, 150.0])
    modes = ["raw", "constant_shift", "planar_ramp", "border_planar"]

    x6 = build_v7_input(
        obs, terr, 1600.0, valid_mask=vm, aspect=1.0, clearance=clr,
        clearance_clamp=CLEARANCE_CLAMP_V6, detrend_modes=modes,
    )
    x5 = build_v6_input(obs, terr, 1600.0, valid_mask=vm, aspect=1.0, clearance=clr)
    assert x6.shape == (4, 6, 64, 64)
    assert torch.allclose(x6[:, :5], x5, atol=1e-6)

    expected_codes = [0.0, 0.33, 0.67, 1.0]
    for idx, code in enumerate(expected_codes):
        ch5 = x6[idx, 5]
        assert torch.allclose(ch5, torch.full((64, 64), code), atol=1e-6), (
            f"Mode {modes[idx]} expected ch5={code}, got [{float(ch5.min())}, {float(ch5.max())}]"
        )

    # None detrend_modes defaults to 1.0
    x6_def = build_v7_input(obs, terr, 1600.0, valid_mask=vm, aspect=1.0, clearance=clr, detrend_modes=None)
    assert torch.allclose(x6_def[:, 5], torch.ones((4, 64, 64)), atol=1e-6)

    # Integration with simulate_batch(return_detrend_modes=True) and make_v6_val_buckets(in_channels=6)
    grid = (32, 32, 16, 50.0, 50.0, 50.0)
    op = TerrainGravityOperator(*grid, h_min=-220.0)
    ds = BodyTerrainDataset(4, 32, 32, 16, 50.0, 50.0, 50.0, seed=5, v6_mode=True)
    rho, uid, t_b, c_b, nf_b, ns_b = (torch.stack([ds[i][k] for i in range(4)]) for k in range(6))
    noisy, _, _, vm_b, dmodes = simulate_batch(
        op, rho, t_b, c_b, nf_b, ns_b, return_mask=True, v6_physics=True, return_detrend_modes=True,
    )
    x_sim = build_v7_input(noisy, t_b, 800.0, valid_mask=vm_b, aspect=1.0, clearance=c_b, detrend_modes=dmodes)
    assert x_sim.shape == (4, 6, 32, 32)
    for idx, dm in enumerate(dmodes):
        assert abs(float(x_sim[idx, 5, 0, 0]) - DETREND_MODE_CODE[dm]) < 1e-6

    op_bank = {i: TerrainGravityOperator(32, 32, 16, 50.0, 50.0, 50.0 * a, h_min=-220.0) for i, a in enumerate(ASPECT_MULTI)}
    cap_bank = {i: SubvoxelCapOperator(32, 32, 50.0, 50.0, 50.0 * a) for i, a in enumerate(ASPECT_MULTI)}
    buckets = make_v6_val_buckets(4, 77, op, op_bank, "cpu", grid, subvox_cap_bank=cap_bank, chunk_size=4, in_channels=6)
    assert set(buckets.keys()) == {"ground_clean", "ground_hard_large_detrend", "airborne_relief_drape", "multi_aspect"}
    for chunks in buckets.values():
        assert chunks[0]["x"].shape[1] == 6


def test_v7_tier_odd_pyramid_adjoint_and_prolongation():
    torch.manual_seed(31)
    op_t1 = OddPyramidGravity(32, 32, 8, cell_xy=100.0, dz_ref=75.0, z0_ref=400.0).eval()
    op_t2 = OddPyramidGravity(16, 16, 4, cell_xy=200.0, dz_ref=150.0, z0_ref=1000.0).eval()
    for op, d, h in ((op_t1, 8, 32), (op_t2, 4, 16)):
        x = torch.randn(2, d, h, h)
        y = torch.randn(2, 1, h, h)
        z = torch.rand(2, 1, h, h) * 200.0 - 20.0
        a = torch.tensor([[[[0.75]]], [[[1.20]]]])
        lhs = float((op(x, z, a) * y).sum())
        rhs = float((x * op.adjoint(y, z, a)).sum())
        rel_err = abs(lhs - rhs) / (abs(lhs) + abs(rhs) + 1e-8)
        assert rel_err < 1e-4, f"V7 tier OddPyramidGravity adjoint mismatch {rel_err:.2e}"

    prolong = TierProlongation3D(64, 64, 32, d0=8, n1_fine=12, n2_fine=12).eval()
    slabs = (torch.randn(2, 1, 8, 64, 64), torch.randn(2, 1, 8, 32, 32), torch.randn(2, 1, 4, 16, 16))
    lvs = (torch.full((2, 1, 8, 64, 64), -3.0), torch.full((2, 1, 8, 32, 32), -2.5), torch.full((2, 1, 4, 16, 16), -2.0))
    mask = torch.ones(2, 1, 32, 64, 64)
    mask[:, :, :2] = 0.0
    with torch.no_grad():
        rho_out, lv_out = prolong(slabs, lvs, mask=mask)
    assert rho_out.shape == (2, 1, 32, 64, 64) and lv_out.shape == (2, 1, 32, 64, 64)
    assert float(rho_out[:, :, :2].abs().max()) == 0.0 and float(lv_out[:, :, :2].abs().max()) == 0.0


def test_v7_compound_loss_tier_supervision_zero_and_gradients():
    torch.manual_seed(41)
    B, D, H, W = 2, 32, 64, 64
    m_true = torch.zeros(B, 1, D, H, W)
    unit_id = torch.zeros(B, 1, D, H, W, dtype=torch.int32)
    # Place targets across Tier 0 (0..8), Tier 1 (8..20), and Tier 2 (20..32)
    m_true[0, 0, 2:6, 20:32, 20:32] = -0.85
    unit_id[0, 0, 2:6, 20:32, 20:32] = 1
    m_true[0, 0, 10:16, 24:40, 24:40] = 0.65
    unit_id[0, 0, 10:16, 24:40, 24:40] = 2
    m_true[1, 0, 22:28, 16:40, 16:40] = 1.15
    unit_id[1, 0, 22:28, 16:40, 16:40] = 1

    t0_exact = m_true[:, :, :8]
    t1_exact = F.adaptive_avg_pool3d(m_true[:, :, 8:20], (8, 32, 32))
    t2_exact = F.avg_pool3d(m_true[:, :, 20:], kernel_size=(3, 4, 4))

    op = TerrainGravityOperator(H, W, D, 50.0, 50.0, 50.0, h_min=-220.0)
    z_obs = torch.full((B, H, W), -0.35)
    d_clean = op(m_true, z_obs)

    # Zero-loss check at m_pred == m_true and exact pooled tier_preds
    crit_zero = CompoundInversionLoss(
        lambda_data=0.50, lambda_ms=0.05, lambda_cls=0.0, lambda_coarse=0.0,
        lambda_rho_init=0.0, lambda_nll=0.0, lambda_tier=0.20,
        forward_op=op, use_v6_losses=True,
    )
    loss_zero, stats_zero = crit_zero(
        m_true.clone(), m_true, d_clean=d_clean, z_obs=z_obs,
        aux={"is_v7": True, "tier_preds": (t0_exact, t1_exact, t2_exact), "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert float(loss_zero) < 1e-6, f"Expected near-zero total loss at exact match, got {float(loss_zero)}"
    assert stats_zero["loss_tier"] < 1e-6, f"Expected near-zero loss_tier, got {stats_zero['loss_tier']}"

    # Perturbed tier_preds produce positive loss_tier and finite gradients
    m_pred_req = (m_true * 0.80).detach().requires_grad_(True)
    tp0 = (t0_exact * 0.70).detach().requires_grad_(True)
    tp1 = (t1_exact * 0.75 + 0.05).detach().requires_grad_(True)
    tp2 = (t2_exact * 0.65).detach().requires_grad_(True)
    loss_pert, stats_pert = crit_zero(
        m_pred_req, m_true, d_clean=d_clean, z_obs=z_obs,
        aux={"is_v7": True, "tier_preds": (tp0, tp1, tp2), "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert stats_pert["loss_tier"] > 0.05
    loss_pert.backward()
    for tensor in (m_pred_req, tp0, tp1, tp2):
        assert tensor.grad is not None and torch.isfinite(tensor.grad).all()
        assert float(tensor.grad.abs().sum()) > 0.0

    # End-to-end V7 slim forward + CompoundInversionLoss backward on CPU
    model = create_model(64, 64, 32, **V7_SIZE_PRESETS["slim"]).train()
    x6 = _make_probe_input(batch=2, channels=6, nx=64, ny=64)
    crit_full = CompoundInversionLoss(
        lambda_data=0.50, lambda_ms=0.05, lambda_cls=0.25,
        forward_op=op, use_v6_losses=True,
    )
    pred, aux = model(x6, return_aux=True)
    loss_e2e, stats_e2e = crit_full(pred, m_true, d_clean=d_clean, z_obs=z_obs, aux=aux, unit_id=unit_id)
    assert torch.isfinite(loss_e2e)
    assert stats_e2e["loss_tier"] > 0.0
    loss_e2e.backward()

    grad_norms = [p.grad.abs().sum().item() for p in model.parameters() if p.requires_grad and p.grad is not None]
    assert len(grad_norms) > 50 and all(np.isfinite(g) for g in grad_norms)
    for mod_name in ("prolong.rho_pw.weight", "dec_vox1_tiers.0.spatial.weight", "dec_vox2_tiers.2.spatial.weight"):
        p = dict(model.named_parameters())[mod_name]
        assert p.grad is not None and torch.isfinite(p.grad).all() and float(p.grad.abs().sum()) > 0.0


def _corr_3d(a: torch.Tensor, b: torch.Tensor) -> float:
    af = a.double().reshape(-1)
    bf = b.double().reshape(-1)
    af = af - af.mean()
    bf = bf - bf.mean()
    return float((af * bf).sum() / (af.norm() * bf.norm() + 1e-12))


def test_v7_warm_start_from_v61_checkpoint():
    torch.manual_seed(53)
    x6 = _make_probe_input(batch=2, channels=6, nx=64, ny=64)

    # 1. V6.1 base (ch_vox=24) -> V7 base (ch_vox=32) direct strict=True migration
    m6_base = create_model(64, 64, 32, **V6_SIZE_PRESETS["base"]).eval()
    assert isinstance(m6_base, PotentialFieldTransformer3DV6)
    v61_sd = {k: v.clone() for k, v in m6_base.state_dict().items()}

    m7_base = create_model(64, 64, 32, **V7_SIZE_PRESETS["base"]).eval()
    m7_base.load_state_dict(v61_sd, strict=True)

    assert torch.equal(m7_base.blocks[0].n1.weight, m6_base.blocks[0].n1.weight)
    assert torch.equal(m7_base.enc0[0].weight[:, :19], m6_base.enc0[0].weight)
    assert float(m7_base.enc0[0].weight[:, 19:].detach().abs().max()) == 0.0
    for tier_i in range(3):
        assert torch.equal(m7_base.up_vox_tiers[tier_i].conv_hw.weight[:24], m6_base.up_vox.conv_hw.weight)
        assert torch.equal(m7_base.skip_vox_tiers[tier_i].proj_2d.weight[:24], m6_base.skip_vox.proj_2d.weight)
        assert torch.equal(m7_base.dec_vox1_tiers[tier_i].pw1.weight[:48, :24], m6_base.dec_vox1.pw1.weight)
        assert torch.equal(m7_base.dec_vox2_tiers[tier_i].pw1.weight[:48, :24], m6_base.dec_vox2.pw1.weight)
    assert float(m7_base.bridge_skip_0to1.proj_2d.weight.detach().abs().max()) == 0.0
    assert float(m7_base.skip_vox_t1.proj_2d.weight.detach().abs().max()) == 0.0

    with torch.no_grad():
        rho_m6_b = m6_base(x6[:, :5])
        rho_b, lv_b = m7_base.forward_uncertainty(x6)
    assert rho_b.shape == (2, 1, 32, 64, 64) and lv_b.shape == (2, 1, 32, 64, 64)
    assert torch.isfinite(rho_b).all() and torch.isfinite(lv_b).all()
    corr_b = _corr_3d(rho_m6_b, rho_b)
    assert corr_b > 0.90, f"Expected step-0 V6.1 base -> V7 base correlation > 0.90, got {corr_b:.4f}"

    # Verify 1 backward pass wakes up newly expanded ch_vox channels (24..32) and cross-tier bridges
    m7_base.train()
    rho_b_grad = m7_base(x6)
    (rho_b_grad ** 2).mean().backward()
    for pname, pgrad in (
        ("res_head.weight[:, 24:]", m7_base.res_head.weight.grad[:, 24:]),
        ("rock_head.weight[:, 24:]", m7_base.rock_head.weight.grad[:, 24:]),
        ("proto_head.weight[24:]", m7_base.proto_head.weight.grad[24:]),
        ("dec_vox2_tiers.0.pw2.weight[:24, 48:]", m7_base.dec_vox2_tiers[0].pw2.weight.grad[:24, 48:]),
        ("bridge_skip_0to1.proj_2d.weight", m7_base.bridge_skip_0to1.proj_2d.weight.grad),
        ("skip_vox_t1.proj_2d.weight", m7_base.skip_vox_t1.proj_2d.weight.grad),
    ):
        assert pgrad is not None and torch.isfinite(pgrad).all()
        assert float(pgrad.abs().sum()) > 0.0, f"Expected non-zero gradient on {pname}"

    # 2. V6.1 slim (ch_vox=16) -> V7 slim (ch_vox=24) preserves >0.90 correlation;
    #    V6.1 base -> V7 slim also loads cleanly via _load_ckpt_weights_compat(..., strict=False).
    m6_slim = create_model(64, 64, 32, **V6_SIZE_PRESETS["slim"]).eval()
    m7_slim_matched = create_model(64, 64, 32, **V7_SIZE_PRESETS["slim"]).eval()
    m7_slim_matched.load_state_dict({k: v.clone() for k, v in m6_slim.state_dict().items()}, strict=True)
    with torch.no_grad():
        rho_m6_s = m6_slim(x6[:, :5])
        rho_s_matched = m7_slim_matched(x6)
    corr_s = _corr_3d(rho_m6_s, rho_s_matched)
    assert corr_s > 0.90, f"Expected step-0 V6.1 slim -> V7 slim correlation > 0.90, got {corr_s:.4f}"

    m7_slim = create_model(64, 64, 32, **V7_SIZE_PRESETS["slim"]).eval()
    _load_ckpt_weights_compat(m7_slim, {k: v.clone() for k, v in v61_sd.items()}, strict=False)
    with torch.no_grad():
        rho_s, aux_s = m7_slim(x6, return_aux=True)
    assert rho_s.shape == (2, 1, 32, 64, 64) and aux_s["is_v7"] is True
    assert torch.isfinite(rho_s).all() and torch.isfinite(aux_s["log_var"]).all()


def test_v7_train_cli_cpu_smoke():
    inv_py_dir = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="v7_cpu_smoke_") as td:
        cmd = [
            sys.executable, "-u", str(inv_py_dir / "train.py"),
            "--variant", "v7",
            "--model-size", "slim",
            "--epochs", "1",
            "--steps", "2",
            "--batch-size", "2",
            "--n-val", "4",
            "--num-workers", "0",
            "--device", "cpu",
            "--save-dir", td,
        ]
        env = dict(os.environ)
        env["PYTHONPATH"] = str(inv_py_dir) + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
        proc = subprocess.run(cmd, env=env, capture_output=True, text=True, check=True)
        out = proc.stdout + proc.stderr
        assert "Architecture: v7 (slim)" in out
        assert "Best val loss" in out

        for fname in ("best_swin3d.pt", "last_swin3d.pt", "training_history.json", "training_monitor.png", "dashboard.html"):
            assert (Path(td) / fname).is_file(), f"Missing training artifact: {fname}"

        ck = torch.load(Path(td) / "best_swin3d.pt", map_location="cpu")
        assert ck["arch"]["variant"] == "v7"
        assert ck["arch"]["model_size"] == "slim"
        assert ck["arch"]["in_channels"] == 6
        assert ck["epoch"] == 1 and np.isfinite(ck["val_loss"])

        with open(Path(td) / "training_history.json", encoding="utf-8") as f:
            hist = json.load(f)
        assert len(hist["epochs"]) == 1
        ep0 = hist["epochs"][0]
        assert np.isfinite(ep0["train_loss"]) and np.isfinite(ep0["val_loss"]) and np.isfinite(ep0["val_gc_iou"])


def test_v7_onnx_export_coreml_single_partition():
    from export_onnx import export_checkpoint_to_onnx

    torch.manual_seed(61)
    model = create_model(64, 64, 32, **V7_SIZE_PRESETS["slim"]).eval()
    with tempfile.TemporaryDirectory(prefix="v7_onnx_") as td:
        ckpt_path = Path(td) / "v7_slim.pt"
        onnx_path = Path(td) / "v7_slim.onnx"
        torch.save({
            "model_state_dict": model.state_dict(),
            "arch": dict(V7_SIZE_PRESETS["slim"]),
            "grid": {"nx": 64, "ny": 64, "nz": 32, "cell": 50.0},
        }, ckpt_path)
        export_checkpoint_to_onnx(str(ckpt_path), str(onnx_path), opset=17, verify=True, verify_mps=False)
        assert onnx_path.is_file() and onnx_path.stat().st_size > 1_000_000


if __name__ == "__main__":
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok  {name}")
    print(f"{len(tests)} tests passed")
