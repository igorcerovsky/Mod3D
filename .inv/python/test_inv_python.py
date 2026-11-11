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
    import tempfile
    from dataset import CELL, NX, NY, NZ, RHO_AIR, RHO_LIGHT_MAX, RHO_LIGHT_MIN, RHO_MAFIC_MIN, RHO_WATER
    from evaluate import make_category_showcase
    from train import TrainingMonitor, build_dashboard_html

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
    assert 'id="radView1"' in html and 'id="radView8"' in html and 'id="radView16"' in html
    assert 'radViewSingle' not in html and 'radViewAll8' not in html

    with tempfile.TemporaryDirectory() as tmp:
        mon = TrainingMonitor(tmp, gui=False)
        rec = {
            "epoch": 1, "train_loss": 1.0, "val_loss": 0.9, "l1_rel": 0.2,
            "body_l1_rel": 0.3, "data_rel_misfit": 0.1, "iou": 0.5,
            "centroid_dz": 50.0, "amp_ratio": 0.9, "halo_opp": 0.01,
        }
        # First write 8 samples, then upgrade to 16 labeled samples (resume compatibility)
        mon.update(rec, sx, strue, strue * 0.8, sclean, sz)
        vx16 = torch.cat([sx, sx], dim=0)
        vt16 = torch.cat([strue, strue], dim=0)
        vc16 = torch.cat([sclean, sclean], dim=0)
        vz16 = torch.cat([sz, sz], dim=0)
        labels16 = ("Std",) * 4 + ("Hard",) * 4 + labels
        mon.update({**rec, "epoch": 2}, vx16, vt16, vt16 * 0.9, vc16, vz16, sample_labels=labels16)
        with open(f"{tmp}/training_history.json") as f:
            payload = json.load(f)
        assert len(payload["meta"]["samples"]) == 16
        assert [s["tag"] for s in payload["meta"]["samples"]] == list(labels16)
        assert len(payload["epochs"][-1]["pred_slices"]) == 16


if __name__ == "__main__":
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok  {name}")
    print(f"{len(tests)} tests passed")
