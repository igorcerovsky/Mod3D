"""Tests for the Python side of .inv. Run with `python test_inv_python.py` (or pytest)."""

import numpy as np
import torch

from dataset import BodyTerrainDataset, augment_d4, random_bodies, random_terrain, simulate_batch, terrain_channel
from evaluate import compute_metrics, project_amplitude
from forward import GravityFFTOperator, TerrainGravityOperator, prism_gz
from infer_prior import column_surface_depth, scattered_to_columns
from losses import CompoundInversionLoss, ZhdanovFocusingLoss
from model_swin3d import PotentialFieldTransformer3D


def test_prism_matches_cpp_kernel():
    # Reference values produced by inv::ForwardKernel<double>::prism_gravity_gz (C++).
    a = prism_gz(0 - 50, 100 - 50, 0 - 50, 100 - 50, 100 - 0, 200 - 0)
    b = prism_gz(0 + 30, 100 + 30, 0 - 220, 100 - 220, 100 + 10, 200 + 10)
    assert abs(a - 0.292723604024) < 1e-9
    assert abs(b - 0.071125731841) < 1e-9


def test_prism_far_field_is_point_mass():
    d = 20000.0
    g = float(prism_gz(-50, 50, -50, 50, d, d + 100))
    approx = 6.6743e-3 * 100 * 100 * 100 / (d + 50) ** 2   # point mass at centre depth d+50, directly above
    assert abs(g - approx) / approx < 1e-3


def test_operator_is_a_true_3d_convolution_and_matches_brute_force():
    nx, ny, nz = 6, 5, 4
    op = GravityFFTOperator(nx, ny, nz, 50.0, 40.0, 30.0, z_obs=-10.0)
    rng = np.random.default_rng(0)
    rho = rng.normal(size=(nz, nx, ny)).astype(np.float32)
    d = op.forward_numpy(rho)
    brute = np.zeros((nx, ny))
    for i in range(nx):
        for j in range(ny):
            xo, yo = (i + 0.5) * 50.0, (j + 0.5) * 40.0
            for a in range(nx):
                for b in range(ny):
                    for c in range(nz):
                        x1, y1, z1 = a * 50.0 - xo, b * 40.0 - yo, c * 30.0 + 10.0
                        brute[i, j] += rho[c, a, b] * prism_gz(x1, x1 + 50, y1, y1 + 40, z1, z1 + 30)
    assert np.max(np.abs(d - brute)) < 1e-4 * np.abs(brute).max()

    # Regression for the old column-sum "forward": one buried cell must influence many stations.
    op64 = GravityFFTOperator(32, 32, 8)
    r = np.zeros((8, 32, 32), np.float32)
    r[3, 16, 16] = 1.0
    assert (np.abs(op64.forward_numpy(r)) > 1e-9).sum() > 100


def test_terrain_operator_matches_brute_force():
    nx, ny, nz, d = 12, 10, 8, 50.0
    op = TerrainGravityOperator(nx, ny, nz, d, d, d)
    rng = np.random.default_rng(0)
    t = rng.uniform(0, 100, (nx, ny))
    zs = t - 0.35
    rho = np.zeros((nz, nx, ny))
    active = (np.arange(nz)[:, None, None] * d) >= (t[None] + d * 0.15)
    rho[active] = rng.normal(size=active.sum())
    got = op.forward_numpy(rho, zs)
    r = rho.transpose(1, 2, 0)
    a, b, c = np.nonzero(r)
    brute = np.zeros((nx, ny))
    for i in range(nx):
        for j in range(ny):
            x1, y1, w1 = a * d - (i + .5) * d, b * d - (j + .5) * d, c * d - zs[i, j]
            brute[i, j] = np.sum(r[a, b, c] * prism_gz(x1, x1 + d, y1, y1 + d, w1, w1 + d))
    assert np.abs(got - brute).max() < 0.01 * np.abs(brute).max()


def test_flat_terrain_operator_equals_flat_fft_operator():
    nx, ny, nz = 16, 16, 8
    flat = GravityFFTOperator(nx, ny, nz, z_obs=-0.35)
    terr = TerrainGravityOperator(nx, ny, nz)
    rho = np.random.default_rng(1).normal(size=(nz, nx, ny))
    a = flat.forward_numpy(rho)
    b = terr.forward_numpy(rho, np.full((nx, ny), -0.35))
    assert np.abs(a - b).max() < 1e-5 * np.abs(a).max()


def test_bodies_are_isolated_below_terrain_with_cavities():
    rng = np.random.default_rng(7)
    nz, dz, saw_cavity, saw_multi = 32, 50.0, False, False
    for _ in range(40):
        t = random_terrain(rng, 64, 64, nz * dz)
        assert t.min() == 0.0 and t.max() <= 0.25 * nz * dz + 1e-3
        rho = random_bodies(rng, t, nz, dz, 50.0)
        assert np.isfinite(rho).all() and (rho != 0).any()
        above = (np.arange(nz)[:, None, None] * dz) < t[None]
        assert not rho[above].any()                                 # nothing above the relief
        assert not rho[:, :3].any() and not rho[:, -3:].any()        # away from lateral borders
        saw_cavity |= rho.min() <= -1.0
        saw_multi |= len(np.unique(rho[rho != 0])) > 1
    assert saw_cavity and saw_multi


def test_dataset_determinism_and_noise_level():
    op = TerrainGravityOperator(32, 32, 16)
    ds = BodyTerrainDataset(4, 32, 32, 16, seed=1)
    r1, r2 = ds[2], ds[2]
    assert all(torch.equal(x, y) for x, y in zip(r1, r2))
    rho, t, c, nf, ns = (torch.stack([ds[i][k] for i in range(4)]) for k in range(5))
    assert bool(((c >= 0.30) & (c <= 0.40)).all())
    noisy, clean, z = simulate_batch(op, rho, t, c, nf, ns)
    assert torch.allclose(z, t - c.view(-1, 1, 1))
    snr = clean.flatten(1).abs().amax(1) / (noisy - clean).flatten(1).std(1)
    assert float(snr.min()) > 20.0
    unseeded = BodyTerrainDataset(1, 32, 32, 16)
    assert not torch.equal(unseeded[0][0], unseeded[0][0])


def test_model_mask_and_amplitude_equivariance():
    from model_swin3d import PotentialFieldTransformer3DV1
    torch.manual_seed(0)
    m = PotentialFieldTransformer3DV1(32, 32, 16, embed_dim=32, num_heads=2, depth=1).eval()
    x = torch.randn(2, 2, 32, 32)
    x[:, 1] = 0.2
    with torch.no_grad():
        y = m(x)
        y3 = m(torch.cat([3.0 * x[:, :1], x[:, 1:]], 1))
    assert y.shape == (2, 1, 16, 32, 32)
    assert not y[:, :, :3].any() and y[:, :, 4:].abs().sum() > 0       # top 0.2 of depth masked
    assert torch.allclose(y3, 3.0 * y, rtol=1e-4, atol=1e-6)            # linear in data amplitude


def test_dataset_lithology_density_bounds():
    from dataset import (RHO_AIR, RHO_LIGHT_MAX, RHO_LIGHT_MIN, RHO_MAFIC_MAX, RHO_MAFIC_MIN,
                         RHO_ORE_MAX, RHO_ORE_MIN, RHO_SEDIMENT_MAX, RHO_SEDIMENT_MIN, RHO_WATER,
                         random_bodies)
    rng = np.random.default_rng(7)
    t = np.zeros((32, 32), dtype=np.float32)
    seen = {"air": False, "water": False, "light": False, "sediment": False, "mafic": False, "ore": False}
    for _ in range(80):
        rho = random_bodies(rng, t, 16, 50.0, 50.0)
        for v in np.unique(rho[rho != 0.0]):
            is_air = np.isclose(v, RHO_AIR, atol=1e-5)
            is_water = np.isclose(v, RHO_WATER, atol=1e-5)
            is_light = (RHO_LIGHT_MIN - 1e-5) <= v <= (RHO_LIGHT_MAX + 1e-5)
            is_sed = (RHO_SEDIMENT_MIN - 1e-5) <= v <= (RHO_SEDIMENT_MAX + 1e-5)
            is_mafic = (RHO_MAFIC_MIN - 1e-5) <= v <= (RHO_MAFIC_MAX + 1e-5)
            is_ore = (RHO_ORE_MIN - 1e-5) <= v <= (RHO_ORE_MAX + 1e-5)
            assert is_air or is_water or is_light or is_sed or is_mafic or is_ore, f"Out-of-range lithology {v}"
            seen["air"] |= bool(is_air)
            seen["water"] |= bool(is_water)
            seen["light"] |= bool(is_light)
            seen["sediment"] |= bool(is_sed)
            seen["mafic"] |= bool(is_mafic)
            seen["ore"] |= bool(is_ore)
    assert all(seen.values()), f"Missing lithology classes in sample: {seen}"


def test_v2_model_bounds_masking_and_aux():
    from dataset import RHO_AIR, RHO_LIGHT_MIN, RHO_LIGHT_MAX, RHO_MAFIC_MAX, RHO_MAFIC_MIN, RHO_ORE_MAX, RHO_WATER
    from losses import CompoundInversionLoss
    from model_swin3d import create_model
    torch.manual_seed(0)
    m = create_model(32, 32, 16, variant="v2", embed_dim=32, num_heads=2, depth=1,
                     ch_mid=8, ch_vox=8, n_sub=2, num_classes=7).eval()
    assert isinstance(m, PotentialFieldTransformer3D)
    x = torch.cat([5.0 * torch.randn(2, 1, 32, 32), torch.full((2, 1, 32, 32), 0.2)], dim=1)
    with torch.no_grad():
        y, aux = m(x, return_aux=True)
    assert y.shape == (2, 1, 16, 32, 32)
    assert not y[:, :, :3].any() and y[:, :, 4:].abs().sum() > 0
    assert float(y.min()) >= RHO_AIR - 1e-4 and float(y.max()) <= RHO_ORE_MAX + 1e-4
    assert aux["logits"].shape == (2, 7, 16, 32, 32)
    assert float(aux["rho_light"].min()) >= RHO_LIGHT_MIN - 1e-5
    assert float(aux["rho_mafic"].max()) <= RHO_MAFIC_MAX + 1e-5
    target = torch.zeros_like(y)
    target[0, 0, 5:7, 6:10, 6:10] = RHO_AIR
    target[0, 0, 8:10, 14:22, 14:20] = 0.5 * (RHO_LIGHT_MIN + RHO_LIGHT_MAX)
    target[0, 0, 11:13, 6:10, 16:22] = -0.45
    target[1, 0, 5:8, 8:12, 8:12] = RHO_WATER
    target[1, 0, 8:11, 16:22, 16:22] = 0.5 * (RHO_MAFIC_MIN + RHO_MAFIC_MAX)
    target[1, 0, 11:14, 6:10, 18:24] = 1.25
    crit = CompoundInversionLoss(lambda_data=0.0, lambda_ms=0.05, lambda_cls=0.25)
    loss, parts = crit(y, target, aux=aux)
    assert torch.isfinite(loss) and parts["loss_cls"] > 0.0


def test_v2_onnx_export_opset17_parity():
    import os
    import tempfile
    from model_swin3d import create_model
    torch.manual_seed(0)
    m = create_model(16, 16, 8, variant="v2", embed_dim=32, num_heads=2, depth=1,
                     ch_mid=8, ch_vox=8, n_sub=2).eval()
    x = torch.randn(1, 2, 16, 16)
    with torch.no_grad():
        y_pt = m(x).numpy()
    with tempfile.TemporaryDirectory() as td:
        onnx_path = os.path.join(td, "v2_test.onnx")
        torch.onnx.export(m, (x,), onnx_path, export_params=True, opset_version=17,
                          do_constant_folding=True, input_names=["observation_field"],
                          output_names=["density_volume"], dynamo=False)
        try:
            import onnxruntime as ort
        except ImportError:
            return
        sess = ort.InferenceSession(onnx_path, providers=["CPUExecutionProvider"])
        y_ort = sess.run(None, {"observation_field": x.numpy()})[0]
        assert np.allclose(y_pt, y_ort, rtol=1e-4, atol=1e-4)


def test_infer_prior_input_rescaling():
    import os
    import tempfile
    from dataset import RHO_AIR, RHO_ORE_MAX
    from infer_prior import amplitude_scale, predict_prior
    from model_swin3d import create_model

    g = {"nx": 16, "ny": 16, "nz": 8, "cell": 50.0}
    assert np.isclose(amplitude_scale(16, 16, 8, 50.0, 50.0, 50.0, g), 1.0)
    assert np.isclose(amplitude_scale(16, 16, 8, 100.0, 100.0, 100.0, g), 0.5)

    torch.manual_seed(1)
    arch = {"variant": "v2", "embed_dim": 32, "num_heads": 2, "depth": 1, "ch_mid": 8, "ch_vox": 8, "n_sub": 2}
    m = create_model(16, 16, 8, **arch).eval()
    xx, yy = np.meshgrid(np.arange(16), np.arange(16), indexing="ij")
    gz_base = -2.0 * np.exp(-((xx - 8.0) ** 2 + (yy - 8.0) ** 2) / 10.0)

    def _make_setup(cell):
        px = (xx.ravel() + 0.5) * cell
        py = (yy.ravel() + 0.5) * cell
        scale = cell / 50.0
        return {
            "voxel_domain": {"nx": 16, "ny": 16, "nz": 8, "dx": cell, "dy": cell, "dz": cell, "x0": 0.0, "y0": 0.0},
            "observations": {"x": px.tolist(), "y": py.tolist(), "gz": (scale * gz_base.ravel()).tolist()},
        }

    with tempfile.TemporaryDirectory() as td:
        ckpt = os.path.join(td, "v2.pt")
        torch.save({"model_state_dict": m.state_dict(), "grid": g, "arch": arch}, ckpt)
        p50 = predict_prior(_make_setup(50.0), ckpt)
        p100 = predict_prior(_make_setup(100.0), ckpt)
    assert np.allclose(p50, p100, rtol=1e-5, atol=1e-5)
    assert p50.min() >= RHO_AIR - 1e-4 and p50.max() <= RHO_ORE_MAX + 1e-4


def _block(n, h):
    m = torch.zeros(1, 1, n, n, n)
    m[:, :, n // 2 - h:n // 2 + h, n // 2 - h:n // 2 + h, n // 2 - h:n // 2 + h] = 0.4
    return m


def test_focusing_functionals_are_resolution_invariant_and_match_cpp_definitions():
    # Same physical 100 m cube in a 400 m domain at two resolutions.
    f50 = ZhdanovFocusingLoss((50.0, 50.0, 50.0))
    f25 = ZhdanovFocusingLoss((25.0, 25.0, 25.0))
    ms50, mgs50 = f50(_block(8, 1), 1e-3)
    ms25, mgs25 = f25(_block(16, 2), 1e-3)
    assert abs(float(ms50) - 1000000 / 400 ** 3) < 1e-6
    assert abs(float(ms50) - float(ms25)) < 1e-6
    # face-area fraction: 6*100^2 contact area / interior face area 3 axes * (n-1) planes * 400^2
    assert abs(float(mgs50) - 6 * 100 ** 2 / (3 * 7 * 400 ** 2)) < 1e-6
    assert abs(float(mgs25) - 6 * 100 ** 2 / (3 * 15 * 400 ** 2)) < 1e-6


def test_compound_loss_uses_physics_and_l1():
    op = TerrainGravityOperator(16, 16, 8)
    ds = BodyTerrainDataset(2, 16, 16, 8, seed=3)
    rho, t, c, nf, ns = (torch.stack([ds[i][k] for i in range(2)]) for k in range(5))
    _, clean, z = simulate_batch(op, rho, t, c, nf, ns)
    crit = CompoundInversionLoss(forward_op=op)
    lp, mp = crit(rho, rho, clean, z)
    lz, mz = crit(torch.zeros_like(rho), rho, clean, z)
    assert mp["loss_sup"] == 0.0 and mp["loss_data"] < 1e-6
    assert mz["loss_data"] > 0.99 and lp.item() < lz.item()
    # scale normalisation: a cavity-strength copy gives the same supervised loss
    _, m10 = crit(torch.zeros_like(rho), 10 * rho, 10 * clean, z)
    assert abs(m10["loss_sup"] - mz["loss_sup"]) < 1e-5


def test_distance_penalty_sharp_boundaries_and_voxel_noise():
    crit = CompoundInversionLoss()
    m_true = torch.zeros(1, 1, 16, 32, 32)
    m_true[:, :, 6:10, 6:10, 6:10] = -2.0

    # 1. Distance-weighted background penalty: identical ghost blob far vs near body
    near_ghost = m_true.clone()
    near_ghost[:, :, 6:10, 11:13, 6:10] = -0.5      # 2 voxels from body (r in 2..3)
    far_ghost = m_true.clone()
    far_ghost[:, :, 6:10, 24:26, 6:10] = -0.5       # >14 voxels from body (r > 7)
    l_near, m_near = crit(near_ghost, m_true)
    l_far, m_far = crit(far_ghost, m_true)
    assert m_far["loss_ms"] > 1.8 * m_near["loss_ms"]
    assert l_far.item() > l_near.item()

    # 2. Sharp boundary preservation: smoothed body vs crisp step-function body
    blurred = torch.nn.functional.avg_pool3d(m_true, kernel_size=3, stride=1, padding=1)
    blurred = blurred * (m_true.abs().max() / blurred.abs().max())
    l_blur, m_blur = crit(blurred, m_true)
    assert m_blur["loss_grad"] > 0.5 and l_blur.item() > 0.3

    # 3. 3D petrophysical voxel noise in simulate_batch
    op = TerrainGravityOperator(16, 16, 8)
    t = torch.zeros(1, 16, 16)
    c = torch.tensor([0.35])
    nf_zero = torch.tensor([0.0])
    ns = torch.tensor([42])
    noisy_vox, clean, z_obs = simulate_batch(op, m_true[:, :, :8, :16, :16], t, c, nf_zero, ns)
    assert not torch.allclose(noisy_vox, clean)     # internal body voxel heterogeneity perturbs field

    # 4. Opposite-sign compensating halo penalty (loss + evaluation metric)
    pos_body = torch.zeros(1, 1, 8, 16, 16)
    pos_body[:, :, 3:6, 6:10, 6:10] = 0.5
    same_sign_halo = pos_body.clone()
    same_sign_halo[:, :, 3:6, 5:6, 6:10] = 0.15     # +0.15 blur halo adjacent to +0.5 body
    opp_sign_halo = pos_body.clone()
    opp_sign_halo[:, :, 3:6, 5:6, 6:10] = -0.15     # -0.15 compensating halo adjacent to +0.5 body
    l_same, m_same = crit(same_sign_halo, pos_body)
    l_opp, m_opp = crit(opp_sign_halo, pos_body)
    assert m_same["loss_halo"] == 0.0 and m_opp["loss_halo"] > 0.15
    assert l_opp.item() > 2.5 * l_same.item()
    d_pos = op(pos_body, z_obs)
    met_same = compute_metrics(same_sign_halo, pos_body, d_pos, z_obs, op)
    met_opp = compute_metrics(opp_sign_halo, pos_body, d_pos, z_obs, op)
    assert met_same["halo_opp"] == 0.0 and met_opp["halo_opp"] > 0.03

    # 5. Depth-weighted sensitivity boost: deeper body error penalized more than shallow error
    two_bodies = torch.zeros(1, 1, 16, 16, 16)
    two_bodies[:, :, 2:4, 6:10, 6:10] = 0.5         # shallow body
    two_bodies[:, :, 12:14, 6:10, 6:10] = 0.5       # deep body
    err_shallow = two_bodies.clone()
    err_shallow[:, :, 2:4, 6:10, 6:10] = 0.25
    err_deep = two_bodies.clone()
    err_deep[:, :, 12:14, 6:10, 6:10] = 0.25
    l_sh, _ = crit(err_shallow, two_bodies)
    l_dp, _ = crit(err_deep, two_bodies)
    assert l_dp.item() > l_sh.item()

    # 6. Dihedral D4 symmetry augmentation preserves exact forward physics
    r_rot, t_rot, _, c_rot, z_rot = augment_d4(pos_body, t, d_pos, d_pos, z_obs, k=1, flip=True)
    assert torch.allclose(op(r_rot, z_rot), c_rot, rtol=1e-5, atol=1e-6)

    # 7. 1-step linear amplitude projection recovers exact scale
    proj = project_amplitude(0.4 * pos_body, d_pos, z_obs, op)
    assert torch.allclose(proj, pos_body, rtol=1e-5, atol=1e-6)

    # 8. Core overshoot penalty + Signed L1-IoU prevent spiky core overshoot on partial coverage
    partial_exact = torch.zeros_like(pos_body)
    partial_exact[:, :, 4:5, 7:9, 7:9] = 0.5        # partial coverage at true density (0.5)
    partial_spiky = torch.zeros_like(pos_body)
    partial_spiky[:, :, 4:5, 7:9, 7:9] = 1.25       # spiky 2.5x core overshoot on same subset
    l_ex, m_ex = crit(partial_exact, pos_body)
    l_sp, m_sp = crit(partial_spiky, pos_body)
    assert m_sp["loss_sup"] > m_ex["loss_sup"]
    assert m_sp["loss_ms"] > m_ex["loss_ms"]
    assert l_sp.item() > l_ex.item()

    # 9. Multi-body local contrast & per-body vertical column centroid_dz
    mixed = torch.zeros(1, 1, 16, 32, 32)
    mixed[:, :, 2:5, 6:10, 6:10] = -2.0             # strong shallow cavity
    mixed[:, :, 11:14, 20:24, 20:24] = 0.30         # weak deep rock body
    mixed_pred = mixed.clone()
    mixed_pred[:, :, 2:5, 6:10, 6:10] = -1.2        # cavity slightly under-predicted so rock > 0.2*max|p|
    op32 = TerrainGravityOperator(32, 32, 16)
    z32 = torch.full((1, 32, 32), -0.35)
    d_mixed = op32(mixed, z32)
    met_mixed = compute_metrics(mixed_pred, mixed, d_mixed, z32, op32)
    assert met_mixed["centroid_dz"] < 5.0           # both bodies are at exact true depths!
    assert met_mixed["iou"] > 0.95                  # both bodies are detected without contrast-cutoff erasure


def test_column_surface_depth_matches_cpp_layout():
    nx, ny = 3, 2
    nodes = np.arange((nx + 1) * (ny + 1), dtype=float)      # [iy*(nx+1)+ix]
    t = column_surface_depth(nodes, nx, ny)
    w = nx + 1
    assert t.shape == (nx, ny)
    assert t[2, 1] == 0.25 * (nodes[1 * w + 2] + nodes[1 * w + 3] + nodes[2 * w + 2] + nodes[2 * w + 3])


def test_scattered_gridding_and_voxel_order():
    x = [10.0, 60.0, 160.0]
    y = [10.0, 10.0, 160.0]
    v = [1.0, 3.0, 5.0]
    out = scattered_to_columns(x, y, v, 4, 4, 0.0, 0.0, 50.0, 50.0)
    assert out[0, 0] == 1.0 and out[1, 0] == 3.0 and out[3, 3] == 5.0
    assert np.isfinite(out).all() and out.min() >= 1.0 and out.max() <= 5.0
    # VoxelGrid order (ix*ny + iy)*nz + iz from a (nz, nx, ny) volume
    vol = np.arange(2 * 3 * 4).reshape(2, 3, 4)               # (nz, nx, ny)
    flat = np.transpose(vol, (1, 2, 0)).ravel(order="C")
    nx, ny, nz = 3, 4, 2
    assert flat[(2 * ny + 1) * nz + 1] == vol[1, 2, 1]


def test_hard_scene_archetypes_bounds_flanks_and_contacts():
    from scipy.ndimage import binary_dilation
    from dataset import (HARD_SCENE_TYPES, RHO_AIR, RHO_LIGHT_MAX, RHO_LIGHT_MIN, RHO_MAFIC_MAX,
                         RHO_MAFIC_MIN, RHO_ORE_MAX, RHO_ORE_MIN, RHO_SEDIMENT_MAX, RHO_SEDIMENT_MIN,
                         RHO_WATER)
    rng = np.random.default_rng(123)
    nz, nx, ny, dz = 32, 64, 64, 50.0
    max_fills = {}
    saw_contact = False
    for st in HARD_SCENE_TYPES:
        fills = []
        for _ in range(12):
            t = random_terrain(rng, nx, ny, nz * dz)
            rho = random_bodies(rng, t, nz, dz, 50.0, scene_type=st)
            assert np.isfinite(rho).all() and (rho != 0).any()
            above = (np.arange(nz)[:, None, None] * dz) < t[None]
            assert not rho[above].any()
            # Keep interior flanks within survey footprint (no full-domain boundary slabs)
            assert not rho[:, :3].any() and not rho[:, -3:].any()
            assert not rho[:, :, :3].any() and not rho[:, :, -3:].any()
            fills.append(float((rho != 0).mean()))
            vals = np.unique(rho[rho != 0])
            for v in vals:
                ok = (np.isclose(v, RHO_AIR, atol=1e-5) or np.isclose(v, RHO_WATER, atol=1e-5)
                      or (RHO_LIGHT_MIN - 1e-5 <= v <= RHO_LIGHT_MAX + 1e-5)
                      or (RHO_SEDIMENT_MIN - 1e-5 <= v <= RHO_SEDIMENT_MAX + 1e-5)
                      or (RHO_MAFIC_MIN - 1e-5 <= v <= RHO_MAFIC_MAX + 1e-5)
                      or (RHO_ORE_MIN - 1e-5 <= v <= RHO_ORE_MAX + 1e-5))
                assert ok, f"{st}: unexpected density {v}"
            if st in ("nested_halo", "abutting_contact") and len(vals) >= 2:
                v0, v1 = vals[0], vals[1]
                if (binary_dilation(rho == v0, iterations=1) & (rho == v1)).any():
                    saw_contact = True
        max_fills[st] = max(fills)
    assert max_fills["large_composite"] > 0.02 and max_fills["large_composite"] < 0.45
    assert saw_contact, "Expected zero-gap voxel face contacts in nested_halo / abutting_contact"


def test_evaluate_metrics_exact_on_nested_and_stacked_same_sign():
    from evaluate import make_fixed_set
    op = TerrainGravityOperator(32, 32, 16)
    z = torch.full((1, 32, 32), -0.35)
    t = torch.zeros(1, 1, 16, 32, 32)
    # 1. Nested same-sign (+1.4 ore inside +0.4 mafic shell) + vertically stacked lens directly above
    t[0, 0, 9:15, 10:22, 10:22] = 0.40
    t[0, 0, 11:13, 14:18, 14:18] = 1.40
    t[0, 0, 2:4, 13:19, 13:19] = 0.50
    t[0, 0, 3:6, 22:26, 22:26] = -1.67
    d = op(t, z)
    met = compute_metrics(t, t, d, z, op, dz=50.0)
    assert abs(met["iou"] - 1.0) < 1e-6, f"Expected exact IoU=1.0 on nested+stacked truth, got {met['iou']}"
    assert met["centroid_dz"] < 1e-5, f"Expected exact centroid_dz=0 on nested+stacked truth, got {met['centroid_dz']}"
    assert abs(met["amp_ratio"] - 1.0) < 1e-6 and met["halo_opp"] == 0.0
    x_h, truth_h, _, _ = make_fixed_set(4, 99, op, "cpu", (32, 32, 16, 50.0, 50.0, 50.0), hard_prob=1.0)
    assert x_h.shape == (4, 2, 32, 32) and (truth_h != 0).any()


def test_showcase_and_16_sample_dashboard():
    import json
    import os
    import tempfile
    from dataset import CELL, NX, NY, NZ, RHO_AIR, RHO_LIGHT_MAX, RHO_LIGHT_MIN, RHO_MAFIC_MIN, RHO_WATER
    from evaluate import make_category_showcase
    from dashboard import TrainingMonitor, build_dashboard_html

    grid = (NX, NY, NZ, CELL, CELL, CELL)
    op = TerrainGravityOperator(NX, NY, NZ, CELL, CELL, CELL)
    sx, strue, sclean, sz, labels = make_category_showcase(op, "cpu", grid, seed=4024)
    assert sx.shape == (8, 2, NX, NY) and strue.shape == (8, 1, NZ, NX, NY)
    assert labels == (
        "Cavities", "Light / Coal", "Tight Cluster", "Mafic / Ore",
        "Large Composite", "Nested Halo", "Abutting Contact", "Vertical Stack",
    )
    truth_np = strue[:, 0].numpy()
    slices = [truth_np[i, :, :, int(np.unravel_index(np.argmax(np.abs(truth_np[i])), truth_np[i].shape)[2])]
              for i in range(8)]
    assert (np.isclose(slices[0], RHO_AIR, atol=0.05) | np.isclose(slices[0], RHO_WATER, atol=0.05)).any()
    assert ((slices[1] <= RHO_LIGHT_MAX + 0.02) & (slices[1] >= RHO_LIGHT_MIN - 0.02)).any()
    assert np.count_nonzero(slices[2]) >= 12
    assert slices[3].max() >= RHO_MAFIC_MIN and slices[3].min() >= -0.05
    assert np.count_nonzero(slices[4]) >= 24
    for idx in (5, 6, 7):
        nz_vals = slices[idx][slices[idx] != 0]
        assert len(np.unique(np.round(nz_vals, 2))) >= 2

    html = build_dashboard_html()
    assert 'id="layoutPicker"' in html and 'id="inpLayoutRows"' in html and 'id="inpLayoutCols"' in html
    for preset in ("1x1", "2x2", "3x3", "4x4", "2x3", "3x2"):
        assert f'data-layout-preset="{preset}"' in html
    assert 'id="radView1"' not in html and 'id="radView8"' not in html and 'id="radView16"' not in html
    assert 'radViewSingle' not in html and 'radViewAll8' not in html
    assert 'data-theme="light"' in html and '[data-theme="dark"]' in html
    assert 'id="btnThemeLight"' in html and 'id="btnThemeDark"' in html
    assert 'function getThemePalette()' in html
    assert 'id="radRender2D"' in html and 'id="radRender3D"' in html and 'id="vol3dToolbar"' in html
    assert 'webgl2' in html and 'function drawVolume3D(' in html and 'function getCamBasis()' in html
    assert 'topoColor(' in html and 'shadeTerrainHit(' in html and 'buildTerrainContourSegments(' in html
    assert 'uField' in html and 'uSurfaceMode' in html and 'data-field-mode="inv"' in html and 'vol3dFieldScale' in html
    assert 'bouguerMod3dColor(' in html and 'diffMod3dColor(' in html
    assert 'data-vol3d-ep="best"' in html and 'data-vol3d-ep="latest"' in html and 'data-vol3d-ep="match"' in html and 'resolveVol3dEpoch' in html
    assert 'pitch: -0.22, dist: 4.0' in html and 'cam3d.pitch = -0.22;' in html
    assert 'cam3d.pitch >= -0.02' not in html
    assert 'float endDiff = uvwEnd.z - texture(uTerrain, uvwEnd.xy).r;' in html

    with tempfile.TemporaryDirectory() as tmp:
        mon = TrainingMonitor(tmp, gui=False, vol3d_stride=2, forward_op=op)
        rec = {
            "epoch": 1, "train_loss": 1.0, "val_loss": 0.9, "l1_rel": 0.2,
            "body_l1_rel": 0.3, "data_rel_misfit": 0.1, "iou": 0.5,
            "centroid_dz": 50.0, "amp_ratio": 0.9, "halo_opp": 0.01,
        }
        # First write 8 samples, then upgrade to 16 labeled samples (resume compatibility)
        mon.update(rec, sx, strue, strue * 0.8, sclean, sz, is_best=True)
        vx16 = torch.cat([sx, sx], dim=0)
        vt16 = torch.cat([strue, strue], dim=0)
        vc16 = torch.cat([sclean, sclean], dim=0)
        vz16 = torch.cat([sz, sz], dim=0)
        labels16 = ("Std",) * 4 + ("Hard",) * 4 + labels
        mon.update({**rec, "epoch": 2}, vx16, vt16, vt16 * 0.9, vc16, vz16, sample_labels=labels16, is_best=True)
        with open(f"{tmp}/training_history.json") as f:
            payload = json.load(f)
        assert len(payload["meta"]["samples"]) == 16
        assert [s["tag"] for s in payload["meta"]["samples"]] == list(labels16)
        assert len(payload["epochs"][-1]["pred_slices"]) == 16

        with open(f"{tmp}/vol3d_meta.json") as f:
            vmeta = json.load(f)
        assert len(vmeta["samples"]) == 16
        assert all("truth_b64" in s and "terrain_b64" in s and "obs_field_b64" in s and "obs_field_vmax" in s for s in vmeta["samples"])
        with open(f"{tmp}/vol3d_latest.json") as f:
            vlat = json.load(f)
        assert vlat["epoch"] == 2 and len(vlat["pred_b64"]) == 16
        assert len(vlat["pred_field_b64"]) == 16 and len(vlat["pred_field_vmax"]) == 16
        with open(f"{tmp}/vol3d_index.json") as f:
            vidx = json.load(f)
        assert vidx["best"] == 2 and vidx["latest"] == 2 and 1 in vidx["epochs"] and 2 in vidx["epochs"]
        assert os.path.exists(f"{tmp}/vol3d_best.js") and os.path.exists(f"{tmp}/vol3d_ep_0002.js")


def test_v3_model_bounds_masking_aux_and_onnx_opset17_parity():
    import os
    import tempfile
    from dataset import RHO_AIR, RHO_ORE_MAX
    from losses import CompoundInversionLoss
    from model_swin3d import PotentialFieldTransformer3DV3, create_model

    torch.manual_seed(0)
    arch = {"variant": "v3", "embed_dim": 32, "num_heads": 2, "depth": 2,
            "ch_mid": 12, "ch_vox": 8, "n_sub": 2, "num_classes": 7}
    m = create_model(16, 16, 8, **arch)
    assert isinstance(m, PotentialFieldTransformer3DV3)
    x = torch.cat([3.0 * torch.randn(2, 1, 16, 16), torch.full((2, 1, 16, 16), 0.25)], dim=1)
    y, aux = m(x, return_aux=True)
    assert y.shape == (2, 1, 8, 16, 16)
    assert not y[:, :, :2].any() and y[:, :, 3:].abs().sum() > 0
    assert float(y.min()) >= RHO_AIR - 1e-4 and float(y.max()) <= RHO_ORE_MAX + 1e-4
    assert aux["edt"].shape == (2, 1, 8, 16, 16) and float(aux["edt"].min()) >= 0.0
    assert not aux["edt"][:, :, :2].any()

    target = torch.zeros_like(y)
    target[0, 0, 3:6, 4:8, 4:8] = RHO_AIR
    target[0, 0, 4:7, 9:13, 4:8] = 0.55
    target[1, 0, 3:6, 5:9, 5:9] = -1.67
    target[1, 0, 4:7, 6:8, 10:13] = 1.30
    crit = CompoundInversionLoss(lambda_data=0.0, lambda_ms=0.05, lambda_cls=0.25)
    loss, parts = crit(y, target, aux=aux)
    assert torch.isfinite(loss) and parts["loss_edt"] > 0.0 and parts["loss_cls"] > 0.0
    loss.backward()
    for p in (m.edt_head.weight, m.up_vox.z.weight, m.skip_vox.d_gate[0].weight, m.s2p_attn.o.weight):
        assert p.grad is not None and torch.isfinite(p.grad).all()

    m.eval()
    x1 = x[:1]
    with torch.no_grad():
        y_pt = m(x1).numpy()
    with tempfile.TemporaryDirectory() as td:
        onnx_path = os.path.join(td, "v3_test.onnx")
        torch.onnx.export(m, (x1,), onnx_path, export_params=True, opset_version=17,
                          do_constant_folding=True, input_names=["observation_field"],
                          output_names=["density_volume"], dynamo=False)
        try:
            import onnxruntime as ort
            sess = ort.InferenceSession(onnx_path, providers=["CPUExecutionProvider"])
            y_ort = sess.run(None, {"observation_field": x1.numpy()})[0]
            assert np.allclose(y_pt, y_ort, rtol=1e-4, atol=1e-4)
        except ImportError:
            pass


def test_v4_model_physics_feedback_occupancy_and_onnx_parity():
    """Verify v4 physics/geology stem, coarse gravity residual feedback, occupancy head, and ONNX parity."""
    import os
    import tempfile
    from model_swin3d import RHO_AIR, RHO_ORE_MAX, create_model

    torch.manual_seed(17)
    arch = {
        "variant": "v4",
        "embed_dim": 64,
        "num_heads": 4,
        "depth": 2,
        "ch_mid": 24,
        "ch_vox": 16,
        "n_sub": 2,
        "num_classes": 7,
    }
    model = create_model(64, 64, 32, **arch)
    x = torch.randn(2, 2, 64, 64)
    x[:, 1] = torch.rand(2, 64, 64) * 0.18

    pred, aux = model(x, return_aux=True)
    assert pred.shape == (2, 1, 32, 64, 64)
    assert aux["logits"].shape == (2, 7, 32, 64, 64)
    assert aux["probs"].shape == (2, 7, 32, 64, 64)
    assert aux["edt"].shape == (2, 1, 32, 64, 64)
    assert aux["coarse_pred"].shape == (2, 1, 16, 32, 32)
    assert aux["occ"].shape == (2, 1, 32, 64, 64)
    assert float(pred.detach().min()) >= RHO_AIR - 1e-4
    assert float(pred.detach().max()) <= RHO_ORE_MAX + 1e-4

    # Air voxels above terrain must be strictly zero
    above_air = aux["mask"] < 0.5
    assert int(above_air.sum()) > 0
    assert torch.all(pred[above_air] == 0.0)

    # Verify gradients flow through coarse physics residual feedback and occupancy parameters
    loss = pred.square().mean() + aux["edt"].mean()
    loss.backward()
    assert model.res_gate.grad is not None and torch.isfinite(model.res_gate.grad)
    assert model.occ_sharpness.grad is not None and torch.isfinite(model.occ_sharpness.grad)
    assert model.cls_sharpness.grad is not None and torch.isfinite(model.cls_sharpness.grad)

    model.eval()
    x1 = x[:1]
    with torch.no_grad():
        y_pt = model(x1).numpy()
    with tempfile.TemporaryDirectory() as tmpdir:
        onnx_path = os.path.join(tmpdir, "v4.onnx")
        torch.onnx.export(model, (x1,), onnx_path, export_params=True, opset_version=17,
                          do_constant_folding=True, input_names=["observation_field"],
                          output_names=["density_volume"], dynamo=False)
        try:
            import onnxruntime as ort
            sess = ort.InferenceSession(onnx_path, providers=["CPUExecutionProvider"])
            y_ort = sess.run(None, {"observation_field": x1.numpy()})[0]
            assert np.allclose(y_pt, y_ort, rtol=1e-4, atol=1e-4)
        except ImportError:
            pass


def test_fast_forward_and_geological_deformations():
    """Verify multi-channel TerrainGravityOperator parity, level_stride=2, and geological body deformations."""
    from dataset import _rasterize_primitive

    op = TerrainGravityOperator(16, 16, 8, 50.0, 50.0, 50.0)
    torch.manual_seed(23)
    rho1 = torch.randn(2, 1, 8, 16, 16) * 400.0
    rho2 = torch.randn(2, 1, 8, 16, 16) * 400.0
    z_obs = torch.rand(2, 1, 16, 16) * 0.15

    g1 = op(rho1, z_obs)
    g2 = op(rho2, z_obs)
    g12 = op(torch.cat([rho1, rho2], dim=1), z_obs)
    assert g12.shape == (2, 2, 16, 16)
    assert torch.allclose(g12[:, 0:1], g1, rtol=1e-5, atol=1e-5)
    assert torch.allclose(g12[:, 1:2], g2, rtol=1e-5, atol=1e-5)

    g_s2 = op(rho1, z_obs, level_stride=2)
    assert g_s2.shape == (2, 1, 16, 16)
    assert torch.isfinite(g_s2).all()
    assert float((g_s2 - g1).abs().max()) < float(g1.abs().max()) * 0.15

    t_cells = np.zeros((32, 32), dtype=np.float32)
    m_base = _rasterize_primitive(t_cells, 16, 16.0, 16.0, 8.0, 7.0, 3.0, 3.5, 0.3, 0.15, False)
    m_deformed = _rasterize_primitive(
        t_cells, 16, 16.0, 16.0, 8.0, 7.0, 3.0, 3.5, 0.3, 0.15, False,
        taper=0.35, fold_amp=0.3, fold_freq=2.2, bend=0.4,
    )
    assert m_base is not None and m_deformed is not None
    assert m_base.shape == (16, 32, 32) and m_deformed.shape == (16, 32, 32)
    assert int(m_deformed.sum()) > 0
    assert not np.array_equal(m_base, m_deformed)


def test_v3_gap_saddle_and_class_balance_loss():
    from losses import CompoundInversionLoss, _gap_saddle_mask, _interior_edt_proxy
    m_true = torch.zeros(1, 1, 16, 32, 32)
    m_true[:, :, 6:10, 6:10, 6:10] = 0.5
    m_true[:, :, 6:10, 11:15, 6:10] = 0.5               # 1-cell inter-body gap at x=10

    fg = (m_true != 0).float()
    saddle = _gap_saddle_mask(fg, max_gap=2)
    assert saddle[:, :, 6:10, 10, 6:10].all()
    assert not saddle[:, :, 6:10, 5, 6:10].any()         # outer wall is NOT a gap saddle

    edt = _interior_edt_proxy(fg, steps=4)
    assert float(edt[:, :, 6, 6, 6]) == 0.25 and float(edt[:, :, 7, 7, 7]) == 0.5
    assert float(edt[:, :, 7, 10, 7]) == 0.0

    bridge = m_true.clone()
    bridge[:, :, 6:10, 10:11, 6:10] = 0.25               # bridges the 1-cell gap
    outer = m_true.clone()
    outer[:, :, 6:10, 5:6, 6:10] = 0.25                  # equal-volume blur outside outer flank
    crit = CompoundInversionLoss()
    l_bridge, _ = crit(bridge, m_true)
    l_outer, _ = crit(outer, m_true)
    assert l_bridge.item() > l_outer.item()


def test_vol3d_rle_roundtrip_and_axis_order():
    import base64
    from vol3d_io import (
        decode_field2d_i16_b64, decode_vol3d_rle_b64,
        encode_field2d_i16_b64, encode_terrain_u16_b64, encode_vol3d_rle_b64,
    )

    vol = np.zeros((32, 64, 64), dtype=np.float32)
    vol[5:10, 12:18, 40:48] = -2.67
    vol[18:24, 38:44, 10:16] = 1.65
    # Asymmetric single marker voxel to catch axis transpositions
    vol[3, 7, 53] = 0.95

    b64, scale = encode_vol3d_rle_b64(vol)
    assert abs(scale - 2.67) < 1e-3
    # Mostly-zero 131,072-voxel grid should compress well under ~15 KB raw
    assert len(base64.b64decode(b64)) < 15000

    rec = decode_vol3d_rle_b64(b64, vol.shape, scale)
    assert rec.shape == vol.shape
    max_err = float(np.abs(rec - vol).max())
    assert max_err <= scale / 127.0 + 1e-4, f"Quantization error too high: {max_err}"
    assert float(rec[3, 7, 53]) > 0.85 and float(rec[0, 0, 0]) == 0.0

    # Linear texture check: first (iz=0) and marker (iz=3, iy=53, ix=7) linear offset in (NZ, NY, NX)
    terr = np.full((64, 64), 200.0, dtype=np.float32)
    terr[7, 53] = 400.0
    t_b64 = encode_terrain_u16_b64(terr, 1600.0)
    u16 = np.frombuffer(base64.b64decode(t_b64), dtype="<u2").reshape(64, 64)  # (NY, NX)
    assert abs((float(u16[53, 7]) / 65535.0) * 1600.0 - 400.0) < 0.05

    fld = np.zeros((64, 64), dtype=np.float32)
    fld[7, 53] = -12.34
    fld[20, 10] = 8.50
    f_b64, f_vmax = encode_field2d_i16_b64(fld)
    f_rec = decode_field2d_i16_b64(f_b64, (64, 64), f_vmax)
    assert f_rec.shape == (64, 64)
    assert abs(float(f_rec[7, 53]) - (-12.34)) < 1e-3
    assert abs(float(f_rec[20, 10]) - 8.50) < 1e-3


def test_demo_export_and_html():
    import json
    import os
    import re
    import tempfile
    from dataset import CELL, NX, NY, NZ
    from evaluate import make_category_showcase
    from export_demo import export_demo
    from dashboard import TrainingMonitor, build_dashboard_html

    demo_html = build_dashboard_html(demo_mode=True)
    assert "const IS_DEMO_MODE = true;" in demo_html
    assert "let layoutRows = IS_DEMO_MODE ? 2 : 1;" in demo_html
    assert "let gridStartIndex = IS_DEMO_MODE ? 5 : 0;" in demo_html
    assert 'src="preview_2d.js"' in demo_html
    assert 'src="vol3d_meta.js"' in demo_html and 'src="vol3d_best.js"' in demo_html
    assert 'id="sampleSelect"' in demo_html and 'id="btnSamplePrev"' in demo_html and 'id="btnSampleNext"' in demo_html
    assert 'id="cvsBouguerBar"' in demo_html
    assert 'src="training_history.js"' not in demo_html
    assert 'id="mTrainLoss"' not in demo_html
    assert 'id="svgLoss"' not in demo_html
    assert 'id="epochSlider"' not in demo_html
    assert 'data-vol3d-ep="latest"' not in demo_html
    assert 'id="refreshIntervalSelect"' not in demo_html
    assert not re.findall(r"__[A-Z][A-Z0-9_]+__", demo_html)

    live_html = build_dashboard_html(demo_mode=False, refresh_sec=120)
    assert 'id="refreshIntervalSelect"' in live_html
    assert 'id="btnRefreshNow"' in live_html
    assert 'id="statusBadge"' in live_html
    assert '.status-badge.inactive' in live_html
    assert "const DEFAULT_REFRESH_SEC = 120;" in live_html
    assert not re.findall(r"__[A-Z][A-Z0-9_]+__", live_html)

    grid = (NX, NY, NZ, CELL, CELL, CELL)
    op = TerrainGravityOperator(NX, NY, NZ, CELL, CELL, CELL)
    sx, strue, sclean, sz, labels = make_category_showcase(op, "cpu", grid, seed=4024)

    with tempfile.TemporaryDirectory() as tmp_save, tempfile.TemporaryDirectory() as tmp_demo:
        mon = TrainingMonitor(tmp_save, gui=False, vol3d_stride=10, forward_op=op)
        base_rec = {
            "train_loss": 0.9, "val_loss": 0.8, "l1_rel": 0.2,
            "body_l1_rel": 0.3, "data_rel_misfit": 0.1, "iou": 0.55,
            "centroid_dz": 40.0, "amp_ratio": 0.92, "halo_opp": 0.01,
        }
        mon.update({**base_rec, "epoch": 1}, sx, strue, strue * 0.85, sclean, sz, sample_labels=labels, is_best=True)
        mon.update({**base_rec, "epoch": 2, "val_loss": 1.2}, sx, strue, strue * 0.5, sclean, sz, sample_labels=labels, is_best=False)

        summary = export_demo(tmp_save, tmp_demo)
        assert summary["best_epoch"] == 1 and summary["n_samples"] == 8
        assert sorted(os.listdir(tmp_demo)) == ["index.html", "preview_2d.js", "vol3d_best.js", "vol3d_meta.js"]

        with open(os.path.join(tmp_demo, "preview_2d.js")) as f:
            raw_preview = f.read()
        assert raw_preview.startswith("window.PREVIEW_2D = ")
        preview = json.loads(raw_preview[len("window.PREVIEW_2D = "):].rstrip(";\n "))
        assert preview["best_epoch"] == 1
        assert "epochs" not in preview
        for k in ("train_loss", "val_loss", "l1_rel", "iou", "centroid_dz"):
            assert k not in raw_preview
        samples = preview["meta"]["samples"]
        assert len(samples) == 8
        for s in samples:
            assert len(s["obs_profile"]) == NX
            assert len(s["pred_profile"]) == NX
            assert len(s["truth_slice"]) == NZ and len(s["truth_slice"][0]) == NX
            assert len(s["pred_slice"]) == NZ and len(s["pred_slice"][0]) == NX

        with open(os.path.join(tmp_demo, "vol3d_best.js")) as f:
            vbest_js = f.read()
        assert "window.VOL3D_BEST_EPOCH = 1;" in vbest_js


def test_v5_greens_bias_and_model_heads():
    from model_swin3d import GreensVerticalBias, PotentialFieldTransformer3DV5, create_model

    gb = GreensVerticalBias(heads=4, dp=6, wh=2, ww=2)
    b = gb()
    assert b.shape == (4, 6 * 4, 6 * 4)
    # Same-layer bias exceeds max-separation vertical decay bias
    assert float(b[0, 0, 0]) > float(b[0, 0, 5 * 4])
    b.sum().backward()
    assert gb.raw_decay.grad is not None and float(gb.raw_decay.grad.abs().sum()) > 0.0
    assert gb.gain.grad is not None and gb.asym.grad is not None

    m = create_model(16, 16, 8, variant="v5", embed_dim=32, num_heads=4, depth=2,
                     ch_mid=16, ch_vox=12, n_sub=2, num_classes=7, dropout=0.1)
    assert isinstance(m, PotentialFieldTransformer3DV5)
    m.eval()
    x5 = torch.randn(2, 5, 16, 16)
    x5[:, 1] = 0.125  # 1/8 depth = 1 layer of air at top
    x5[:, 2] = 1.0
    x5[:, 3] = 0.0
    x5[:, 4] = 0.25

    pred, aux = m(x5, return_aux=True)
    assert pred.shape == (2, 1, 8, 16, 16)
    assert aux["log_var"].shape == (2, 1, 8, 16, 16)
    assert aux["logits"].shape == (2, 7, 8, 16, 16)
    assert aux["coarse_pred"].shape == (2, 1, 4, 8, 8)
    assert float(aux["log_var"].min()) >= -8.0 and float(aux["log_var"].max()) <= 4.0
    # Strict terrain masking on both density and log_var above terrain (iz=0 < 0.125*8=1)
    assert float(pred[:, :, 0].abs().max()) == 0.0
    assert float(aux["log_var"][:, :, 0].abs().max()) == 0.0
    assert not m.coarse_fwd_conv.weight.requires_grad
    assert float(m.coarse_fwd_conv.weight.min()) >= 0.0
    assert abs(float(aux["coarse_pred"].mean())) < 0.10

    # Backward compatibility with 2-channel input + single-output forward
    pred2 = m(x5[:, :2])
    assert pred2.shape == (2, 1, 8, 16, 16)
    pred_u, lv_u = m.forward_uncertainty(x5)
    assert torch.allclose(pred, pred_u) and torch.allclose(aux["log_var"], lv_u)

    # MC Dropout stochasticity when enabled vs determinism in eval
    assert torch.allclose(m(x5), m(x5))
    m.enable_mc_dropout(True)
    assert not torch.allclose(m(x5), m(x5))
    m.enable_mc_dropout(False)


def test_v5_heteroscedastic_nll_and_coarse_losses():
    from model_swin3d import create_model

    torch.manual_seed(7)
    m = create_model(16, 16, 8, variant="v5", embed_dim=32, num_heads=4, depth=2,
                     ch_mid=16, ch_vox=12, n_sub=2, num_classes=7, dropout=0.05)
    x5 = torch.randn(2, 5, 16, 16)
    x5[:, 1] = 0.0
    x5[:, 2] = 1.0
    x5[:, 3] = 0.0
    x5[:, 4] = 0.007

    rho_t = torch.zeros(2, 1, 8, 16, 16)
    rho_t[:, :, 3:6, 5:10, 5:10] = 0.8
    pred, aux = m(x5, return_aux=True)

    crit = CompoundInversionLoss()
    loss, metrics = crit(pred, rho_t, aux=aux)
    assert torch.isfinite(loss)
    assert metrics["loss_nll"] >= 0.0
    assert metrics["loss_coarse"] >= 0.0
    assert metrics["loss_amp"] >= 0.0
    loss.backward()
    assert m.logvar_head.weight.grad is not None and torch.isfinite(m.logvar_head.weight.grad).all()

    # Exact identity gives zero supervised + amp + coarse errors while NLL remains >= 0
    aux_perfect = {
        "log_var": torch.full_like(rho_t, -8.0),
        "coarse_pred": torch.nn.functional.avg_pool3d(rho_t, 2),
        "mask": torch.ones_like(rho_t),
    }
    _, mp = crit(rho_t, rho_t, aux=aux_perfect)
    assert mp["loss_sup"] == 0.0 and mp["loss_coarse"] < 1e-6 and mp["loss_amp"] < 1e-6
    assert abs(mp["loss_nll"]) < 1e-6


def test_v5_realistic_augmentations_infill_and_clearance():
    from dataset import (
        ASPECT_MULTI,
        CLEARANCE_MULTI,
        RHO_AIR,
        RHO_LIGHT_MIN,
        RHO_ORE_MAX,
        RHO_WATER,
        apply_planar_fault,
        build_v5_input,
        infill_missing_obs,
        random_bodies,
        random_terrain,
    )

    rng = np.random.default_rng(123)
    t = random_terrain(rng, 24, 24, 12 * 50.0)
    rho = random_bodies(rng, t, 12, 50.0, 50.0, hard_prob=1.0, fault_prob=1.0)
    t_cells = t / 50.0
    rho_f = apply_planar_fault(rng, rho, t_cells)
    # No density allowed above terrain
    above = (np.arange(12)[:, None, None] * 50.0) < t[None]
    assert np.all(rho[above] == 0.0) and np.all(rho_f[above] == 0.0)
    # Lithology range preserved across fault cuts
    nz_vals = rho_f[rho_f != 0.0]
    assert nz_vals.size > 0
    for v in nz_vals:
        assert (
            abs(v - RHO_AIR) < 1e-4
            or abs(v - RHO_WATER) < 1e-4
            or (RHO_LIGHT_MIN - 1e-4 <= v <= RHO_ORE_MAX + 1e-4)
        ), f"Unexpected lithology density after fault: {v}"

    # Wide clearance operator (h_min=-85 m) + dropout/infill (no regolith/trend)
    op_hi = TerrainGravityOperator(24, 24, 12, 50.0, 50.0, 50.0, h_min=-85.0)
    assert op_hi.h0 <= -85.0
    ds = BodyTerrainDataset(4, 24, 24, 12, 50.0, 50.0, 50.0, seed=99,
                            fault_prob=0.5, clearance_range=CLEARANCE_MULTI,
                            aspect_ratios=ASPECT_MULTI)
    items = [ds[i] for i in range(4)]
    assert len(items[0]) == 6  # appends aspect_idx
    clear_max = float(max(float(it[2]) for it in items))
    assert clear_max >= 0.30

    rho_b, t_b, _, nf_b, ns_b = (torch.stack([it[k] for it in items]) for k in range(5))
    c_air = torch.full((4,), 55.0)
    obs_v5, clean_v5, z_obs_v5, vm_v5 = simulate_batch(
        op_hi, rho_b, t_b, c_air, nf_b, ns_b,
        dropout_prob=1.0, return_mask=True,
    )
    assert torch.isfinite(obs_v5).all() and vm_v5.shape == obs_v5.shape
    assert torch.allclose(z_obs_v5, t_b - 55.0)
    # Valid mask is non-trivial on at least one sample and missing stations were harmonically filled
    assert float(vm_v5.min()) == 0.0 and not torch.isnan(obs_v5).any()
    x5 = build_v5_input(obs_v5, t_b, 12 * 50.0, valid_mask=vm_v5, aspect=1.0, clearance=c_air)
    assert x5.shape == (4, 5, 24, 24)
    assert abs(float(x5[0, 4, 0, 0]) - 55.0 / 50.0) < 1e-4

    # Pure harmonic infill preserves exact observed stations
    raw = torch.ones((1, 1, 8, 8)) * 5.0
    raw[0, 0, 3:5, 3:5] = -999.0
    vm_test = torch.ones((1, 1, 8, 8))
    vm_test[0, 0, 3:5, 3:5] = 0.0
    filled = infill_missing_obs(raw, vm_test, iters=24)
    assert torch.allclose(filled[0, 0, 3:5, 3:5], torch.full((2, 2), 5.0), atol=1e-4)


def test_v5_onnx_export_and_parity():
    import os
    import tempfile
    from export_onnx import export_checkpoint_to_onnx
    from model_swin3d import create_model

    m = create_model(16, 16, 8, variant="v5", embed_dim=32, num_heads=4, depth=2,
                     ch_mid=16, ch_vox=12, n_sub=2, num_classes=7, dropout=0.05)
    ck = {
        "grid": {"nx": 16, "ny": 16, "nz": 8, "cell": 50.0},
        "arch": {"variant": "v5", "embed_dim": 32, "num_heads": 4, "depth": 2,
                 "ch_mid": 16, "ch_vox": 12, "n_sub": 2, "num_classes": 7, "dropout": 0.05},
        "model_state_dict": m.state_dict(),
    }
    with tempfile.TemporaryDirectory() as tmp:
        ckpt_p = os.path.join(tmp, "v5_test.pt")
        onnx_p = os.path.join(tmp, "v5_test.onnx")
        torch.save(ck, ckpt_p)
        export_checkpoint_to_onnx(ckpt_p, onnx_p, opset=17, verify=True)
        assert os.path.exists(onnx_p)


def test_v52_unit_and_instance_iou_loss():
    from losses import _interior_edt_proxy, _internal_unit_contact_mask

    crit = CompoundInversionLoss(lambda_data=0.0, lambda_cls=0.0, lambda_coarse=0.0, lambda_nll=0.0)

    # 1. Two-body scene: large strong air cavity (-2.67, 8x8x6) + small weak sediment body (-0.25, 4x4x4)
    m_true = torch.zeros(1, 1, 16, 32, 32)
    m_true[:, :, 2:8, 4:12, 4:12] = -2.67
    m_true[:, :, 6:10, 20:24, 20:24] = -0.25

    _, mp_exact = crit(m_true, m_true)
    assert mp_exact["loss_unit_iou"] < 0.05 and mp_exact["loss_sup"] == 0.0

    # Missing the weak sediment body must substantially penalize loss_unit_iou (not drowned by cavity)
    m_miss_weak = m_true.clone()
    m_miss_weak[:, :, 6:10, 20:24, 20:24] = 0.0
    _, mp_miss = crit(m_miss_weak, m_true)
    assert mp_miss["loss_unit_iou"] - mp_exact["loss_unit_iou"] > 0.20

    # 2. Abutting geological units (+1.50 ore directly touching +0.40 mafic): internal contact splits EDT
    m_abut = torch.zeros(1, 1, 16, 32, 32)
    m_abut[:, :, 4:12, 8:16, 8:16] = 1.50
    m_abut[:, :, 4:12, 16:24, 8:16] = 0.40
    fg = (m_abut != 0).float()
    c_mask = _internal_unit_contact_mask(m_abut, fg)
    assert float(c_mask[:, :, 4:12, 15:17, 8:16].min()) == 1.0
    edt = _interior_edt_proxy(fg, steps=4, contact_mask=c_mask)
    core_edt = float(edt[0, 0, 8, 12, 12])
    iface_edt = float(edt[0, 0, 8, 15, 12])
    assert iface_edt < core_edt

    _, mp_abut_exact = crit(m_abut, m_abut)
    assert mp_abut_exact["loss_sup"] == 0.0
    m_abut_miss_mafic = m_abut.clone()
    m_abut_miss_mafic[:, :, 4:12, 16:24, 8:16] = 0.0
    _, mp_abut_miss = crit(m_abut_miss_mafic, m_abut)
    assert mp_abut_miss["loss_unit_iou"] - mp_abut_exact["loss_unit_iou"] > 0.20


def test_v52_wavelength_clearance_and_curriculum():
    from dataset import (
        CLEARANCE_MULTI, RHO_AIR, RHO_WATER, BodyTerrainDataset,
        compute_max_resolvable_clearance,
    )

    t_cells = np.zeros((64, 64), dtype=np.float32)

    # 1. Cavity scenes (air / water) must cap clearance at <= 2.0 m
    for cav in (RHO_AIR, RHO_WATER):
        rho_cav = np.zeros((32, 64, 64), dtype=np.float32)
        rho_cav[2:8, 20:32, 20:32] = cav
        eff = compute_max_resolvable_clearance(rho_cav, t_cells, 50.0, 50.0,
                                               scene_type="isolated", fault_applied=False,
                                               clearance_range=CLEARANCE_MULTI)
        assert eff[1] <= 2.0, f"cavity clearance not capped: {eff}"

    # 2. High-frequency scene archetypes and faulted scenes must cap clearance at <= 2.0 m
    rho_broad = np.zeros((32, 64, 64), dtype=np.float32)
    rho_broad[2:10, 20:32, 20:32] = 1.10
    for st in ("tight_cluster", "nested_halo", "abutting_contact", "vertical_stack"):
        eff = compute_max_resolvable_clearance(rho_broad, t_cells, 50.0, 50.0,
                                               scene_type=st, fault_applied=False,
                                               clearance_range=CLEARANCE_MULTI)
        assert eff[1] <= 2.0, f"high-freq scene {st} not capped: {eff}"
    eff_fault = compute_max_resolvable_clearance(rho_broad, t_cells, 50.0, 50.0,
                                                 scene_type="isolated", fault_applied=True,
                                                 clearance_range=CLEARANCE_MULTI)
    assert eff_fault[1] <= 2.0, f"faulted scene not capped: {eff_fault}"

    # 3. Small/narrow isolated body (< 3.5 cells) must cap clearance at <= 2.0 m
    rho_small = np.zeros((32, 64, 64), dtype=np.float32)
    rho_small[2:6, 30:33, 30:33] = 1.10
    eff_small = compute_max_resolvable_clearance(rho_small, t_cells, 50.0, 50.0,
                                                 scene_type="isolated", fault_applied=False,
                                                 clearance_range=CLEARANCE_MULTI)
    assert eff_small[1] <= 2.0, f"small body not capped: {eff_small}"

    # 4. Broad, shallow, fault-free rock body permits elevated airborne clearance (> 15.0 m)
    eff_broad = compute_max_resolvable_clearance(rho_broad, t_cells, 50.0, 50.0,
                                                 scene_type="isolated", fault_applied=False,
                                                 clearance_range=CLEARANCE_MULTI)
    assert eff_broad[1] > 15.0 and eff_broad[1] <= CLEARANCE_MULTI[1], f"broad body capped wrongly: {eff_broad}"

    # 5. Dataset curriculum setter updates sampling bounds
    ds = BodyTerrainDataset(16, hard_prob=0.15, fault_prob=0.05, clearance_range=(0.30, 2.0))
    ds.set_curriculum(hard_prob=0.50, fault_prob=0.40, clearance_max=80.0)
    assert ds.hard_prob == 0.50 and ds.fault_prob == 0.40 and ds.clearance_range == (0.30, 80.0)


def test_v52_slim_base_heavy_architectures():
    from model_swin3d import V5_SIZE_PRESETS, PotentialFieldTransformer3DV5, create_model

    x = torch.randn(2, 5, 16, 16)
    counts = {}
    for sz in ("slim", "base", "heavy"):
        m = create_model(16, 16, 8, **V5_SIZE_PRESETS[sz]).eval()
        counts[sz] = sum(p.numel() for p in m.parameters() if p.requires_grad)
        with torch.no_grad():
            pred, aux = m(x, return_aux=True)
        assert pred.shape == (2, 1, 8, 16, 16), f"{sz} pred shape mismatch: {pred.shape}"
        assert aux["log_var"].shape == (2, 1, 8, 16, 16)

    assert counts["slim"] < counts["base"] < counts["heavy"], f"unexpected param counts: {counts}"

    # Base preset keys must match default PotentialFieldTransformer3DV5 identically
    m_base = create_model(16, 16, 8, **V5_SIZE_PRESETS["base"])
    m_def = PotentialFieldTransformer3DV5(16, 16, 8)
    assert list(m_base.state_dict().keys()) == list(m_def.state_dict().keys())


if __name__ == "__main__":
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok  {name}")
    print(f"{len(tests)} tests passed")
