"""Tests for the Python side of .inv. Run with `python test_inv_python.py` (or pytest)."""

import numpy as np
import torch

from dataset import ProceduralGeologyGenerator, SyntheticPotentialFieldDataset
from forward import GravityFFTOperator, prism_gz
from infer_prior import scattered_to_columns
from losses import CompoundInversionLoss, ZhdanovFocusingLoss


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


def test_dataset_signal_to_noise_determinism_and_anomaly_models():
    ds = SyntheticPotentialFieldDataset(8, 32, 32, 16, seed=1, return_clean=True)
    snr = []
    for i in range(8):
        obs, dens, clean = ds[i]
        assert obs.shape == (1, 32, 32) and dens.shape == (1, 16, 32, 32)
        # anomaly model: laterally uniform component removed
        assert float(dens[0].mean(dim=(1, 2)).abs().max()) < 1e-5
        snr.append(float(clean.std() / (obs - clean).std()))
    assert np.median(snr) > 10.0                                   # was 0.08 with the old generator
    a1, d1 = ds[3][:2]
    a2, d2 = ds[3][:2]
    assert torch.equal(a1, a2) and torch.equal(d1, d2)             # seeded -> reproducible
    ds2 = SyntheticPotentialFieldDataset(2, 32, 32, 16)            # unseeded -> fresh samples
    assert not torch.equal(ds2[0][1], ds2[0][1])


def test_fault_copy_reads_original_model():
    g = ProceduralGeologyGenerator(32, 32, 16)
    rng = np.random.default_rng(5)
    for _ in range(5):
        m = g.generate_random_model(rng)
        assert np.isfinite(m).all() and m.min() >= -0.6 and m.max() <= 0.8


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
    op = GravityFFTOperator(16, 16, 8)
    ds = SyntheticPotentialFieldDataset(2, 16, 16, 8, seed=3, operator=op)
    obs = torch.stack([ds[0][0], ds[1][0]])
    dens = torch.stack([ds[0][1], ds[1][1]])
    crit = CompoundInversionLoss(forward_op=op)
    loss_perfect, m_perfect = crit(dens, dens, d_obs=obs, epoch=1, total_epochs=10)
    loss_zero, m_zero = crit(torch.zeros_like(dens), dens, d_obs=obs, epoch=1, total_epochs=10)
    assert m_perfect["loss_sup"] == 0.0 and m_perfect["loss_data"] < 0.1   # noise-limited
    assert m_zero["loss_data"] > 0.99                                      # zero model explains nothing
    assert loss_perfect.item() < loss_zero.item()
    assert crit.e_at(1, 10) > crit.e_at(10, 10)


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
