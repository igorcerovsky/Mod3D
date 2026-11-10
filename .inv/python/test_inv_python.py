"""Tests for the Python side of .inv. Run with `python test_inv_python.py` (or pytest)."""

import numpy as np
import torch

from dataset import BodyTerrainDataset, random_bodies, random_terrain, simulate_batch, terrain_channel
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
    torch.manual_seed(0)
    m = PotentialFieldTransformer3D(32, 32, 16, embed_dim=32, num_heads=2, depth=1).eval()
    x = torch.randn(2, 2, 32, 32)
    x[:, 1] = 0.2
    with torch.no_grad():
        y = m(x)
        y3 = m(torch.cat([3.0 * x[:, :1], x[:, 1:]], 1))
    assert y.shape == (2, 1, 16, 32, 32)
    assert not y[:, :, :3].any() and y[:, :, 4:].abs().sum() > 0       # top 0.2 of depth masked
    assert torch.allclose(y3, 3.0 * y, rtol=1e-4, atol=1e-6)            # linear in data amplitude


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


if __name__ == "__main__":
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok  {name}")
    print(f"{len(tests)} tests passed")
