"""Synthetic training data: ISOLATED bodies + cavities below a random terrain (anomaly only, no trend).

Each sample
-----------
* Random terrain (smooth Gaussian-filtered noise, optional tilt; 15 % flat), relief up to
  ``MAX_RELIEF_FRAC`` of the domain depth. ``t[i, j]`` = terrain depth below the grid top (z down,
  0 at the highest point). Stations are DRAPED on the terrain, ``clearance`` 0.30-0.40 m (30-40 cm,
  typical tripod/ground height for relative gravity meters) above it.
* 1 body (40 %) or 2-4 bodies (60 %), well separated (>= 2 empty cells), fully below the terrain
  and away from the lateral borders. Shapes: rotated ellipsoids or boxes.
    - rock bodies   +/-0.1 ... 0.8 g/cm^3 (random sign)
    - cavities      (25 % of bodies, small & shallow): air -2.0 ... -2.67, water -1.0 ... -1.67
* Background contrast is exactly zero (terrain-corrected anomaly: the relief carries no contrast).
* Field: exact Nagy-prism response on the draped stations (``TerrainGravityOperator``) plus
  Gaussian noise with sigma = U(0.5 %, 4 %) * max|clean field|. No regional trend.

The cheap part (geometry, numpy) runs in DataLoader workers via ``BodyTerrainDataset``; the forward
model runs batched on the training device via ``simulate_batch`` (~2 ms per sample on Apple MPS).
"""

import math

import numpy as np
import torch
from scipy.ndimage import binary_dilation, gaussian_filter
from torch.utils.data import Dataset

from forward import TerrainGravityOperator

# Training geometry (the C++ inference resamples any domain onto this grid; see neural_prior.hpp)
NX, NY, NZ, CELL = 64, 64, 32, 50.0
MAX_RELIEF_FRAC = 0.25          # max terrain relief / domain depth
CLEARANCE = (0.30, 0.40)        # ground gravimeter sensor height above terrain [m] (30-40 cm)
NOISE_FRAC = (0.005, 0.04)      # noise sigma / max|field|


def random_terrain(rng, nx, ny, depth_total):
    """Terrain depth below the grid top, (nx, ny) metres, min == 0."""
    if rng.random() < 0.15:
        return np.zeros((nx, ny), np.float32)
    corr = rng.uniform(4.0, 16.0) * nx / 64.0
    h = gaussian_filter(rng.normal(size=(nx, ny)), corr, mode="reflect")
    if rng.random() < 0.3:                                   # regional slope
        xg, yg = np.meshgrid(np.linspace(-1, 1, nx), np.linspace(-1, 1, ny), indexing="ij")
        h = h / (h.std() + 1e-12) + rng.uniform(-1.5, 1.5) * xg + rng.uniform(-1.5, 1.5) * yg
    h = (h - h.min()) / (h.max() - h.min() + 1e-12)
    relief = rng.uniform(0.03, MAX_RELIEF_FRAC) * depth_total
    t = ((1.0 - h) * relief).astype(np.float32)              # highest point -> depth 0
    return t - t.min()


def random_bodies(rng, t, nz, dz, dx):
    """Density model (nz, nx, ny) with isolated bodies strictly below terrain depth ``t`` (nx, ny)."""
    nx, ny = t.shape
    rho = np.zeros((nz, nx, ny), np.float32)
    blocked = np.zeros((nz, nx, ny), bool)
    t_cells = t / dz                                         # terrain depth in layer units
    hs = nx / 64.0                                           # horizontal size scale
    n_bodies = 1 if rng.random() < 0.4 else int(rng.integers(2, 5))
    placed, attempts = 0, 0
    while placed < n_bodies and attempts < 60:
        attempts += 1
        cavity = rng.random() < 0.25
        if cavity:
            rx, ry = rng.uniform(1.0, 3.5, 2) * hs
            rz = rng.uniform(0.8, 2.5) * nz / 32.0
            gap = rng.uniform(0.5, 6.0) * nz / 32.0          # top of cavity below terrain [cells]
            rho_val = -rng.uniform(2.0, 2.67) if rng.random() < 0.7 else -rng.uniform(1.0, 1.67)
        else:
            rx, ry = rng.uniform(1.5, 9.0, 2) * hs
            rz = rng.uniform(1.0, 6.0) * nz / 32.0
            gap = rng.uniform(0.5, 0.55 * nz)
            rho_val = rng.uniform(0.1, 0.8) * (1.0 if rng.random() < 0.5 else -1.0)
        rmax = max(rx, ry)
        margin = 3 + rmax
        if 2 * margin >= min(nx, ny):
            continue
        cx = rng.uniform(margin, nx - margin)
        cy = rng.uniform(margin, ny - margin)
        i0, i1 = int(max(0, cx - rmax - 1)), int(min(nx, cx + rmax + 2))
        j0, j1 = int(max(0, cy - rmax - 1)), int(min(ny, cy + rmax + 2))
        top_surface = float(t_cells[i0:i1, j0:j1].max())     # body must clear the highest relief above it
        cz = top_surface + gap + rz
        if cz + rz > nz - 0.5:
            continue
        k0, k1 = int(max(0, cz - rz - 1)), int(min(nz, cz + rz + 2))
        zz, xx, yy = np.meshgrid(np.arange(k0, k1) + 0.5, np.arange(i0, i1) + 0.5,
                                 np.arange(j0, j1) + 0.5, indexing="ij")
        th = rng.uniform(0, math.pi)
        u = (xx - cx) * math.cos(th) + (yy - cy) * math.sin(th)
        v = -(xx - cx) * math.sin(th) + (yy - cy) * math.cos(th)
        w = zz - cz
        if rng.random() < 0.6:
            sub = (u / rx) ** 2 + (v / ry) ** 2 + (w / rz) ** 2 <= 1.0
        else:
            sub = (np.abs(u) <= rx) & (np.abs(v) <= ry) & (np.abs(w) <= rz)
        if not sub.any():
            sub[sub.shape[0] // 2, sub.shape[1] // 2, sub.shape[2] // 2] = True
        mask = np.zeros_like(blocked)
        mask[k0:k1, i0:i1, j0:j1] = sub
        if (mask & blocked).any():
            continue
        rho[mask] = rho_val
        blocked |= binary_dilation(mask, iterations=2)
        placed += 1
    # safety: no density in cells whose top is above the terrain
    above = (np.arange(nz)[:, None, None] * dz) < t[None]
    rho[above] = 0.0
    return rho


def sample_geometry(rng, nx=NX, ny=NY, nz=NZ, dx=CELL, dy=CELL, dz=CELL):
    t = random_terrain(rng, nx, ny, nz * dz)
    rho = random_bodies(rng, t, nz, dz, dx)
    clearance = np.float32(rng.uniform(*CLEARANCE))
    noise_frac = np.float32(rng.uniform(*NOISE_FRAC))
    return rho, t, clearance, noise_frac


class BodyTerrainDataset(Dataset):
    """Yields geometry only: ``(rho (1,nz,nx,ny), t (nx,ny), clearance, noise_frac)``.

    ``seed=None`` -> fresh random sample on every access (infinite training data);
    ``seed=int``  -> sample ``idx`` is deterministic (validation).
    """

    def __init__(self, n_samples=2000, nx=NX, ny=NY, nz=NZ, dx=CELL, dy=CELL, dz=CELL, seed=None):
        self.n, self.seed = n_samples, seed
        self.g = (nx, ny, nz, dx, dy, dz)

    def __len__(self):
        return self.n

    def __getitem__(self, idx):
        rng = np.random.default_rng() if self.seed is None else np.random.default_rng([self.seed, idx])
        rho, t, c, nf = sample_geometry(rng, *self.g)
        noise_seed = int(rng.integers(0, 2**31 - 1))
        return (torch.from_numpy(rho)[None], torch.from_numpy(t), torch.tensor(c), torch.tensor(nf),
                torch.tensor(noise_seed))


@torch.no_grad()
def simulate_batch(op: TerrainGravityOperator, rho, t, clearance, noise_frac, noise_seed=None):
    """Batched forward on ``op``'s device. Returns ``(obs_noisy, obs_clean, z_obs)``.

    obs: (B, 1, nx, ny) mGal; z_obs: (B, nx, ny) station depth = t - clearance.
    """
    z_obs = t - clearance.view(-1, 1, 1)
    clean = op(rho, z_obs)
    peak = clean.abs().amax(dim=(1, 2, 3), keepdim=True)
    if noise_seed is None:
        noise = torch.randn_like(clean)
    else:   # deterministic noise (validation)
        noise = torch.stack([torch.randn(clean.shape[1:], generator=torch.Generator().manual_seed(int(s)))
                             for s in noise_seed.cpu()]).to(clean.device)
    noisy = clean + noise * noise_frac.view(-1, 1, 1, 1) * peak
    return noisy, clean, z_obs


def terrain_channel(t, depth_total):
    """Network terrain input: terrain depth / domain depth, (B, 1, nx, ny)."""
    return (t / depth_total).unsqueeze(1)


if __name__ == "__main__":
    import argparse
    import os

    ap = argparse.ArgumentParser(description="Preview synthetic isolated-body samples")
    ap.add_argument("--samples", type=int, default=4)
    ap.add_argument("--output", type=str, default="", help="Optional directory for a .npz preview")
    args = ap.parse_args()
    ds = BodyTerrainDataset(args.samples, seed=0)
    op = TerrainGravityOperator(NX, NY, NZ, CELL, CELL, CELL)
    for i in range(args.samples):
        rho, t, c, nf, ns = ds[i]
        obs, clean, _ = simulate_batch(op, rho[None], t[None], c[None], nf[None], ns[None])
        nb = len(np.unique(rho[rho != 0].numpy()))
        print(f"sample {i}: bodies~{nb} rho[{rho.min():.2f},{rho.max():.2f}] relief {t.max():.0f} m "
              f"field[{clean.min():.3f},{clean.max():.3f}] mGal")
        if args.output:
            os.makedirs(args.output, exist_ok=True)
            np.savez_compressed(os.path.join(args.output, f"sample_{i}.npz"), rho=rho.numpy(), t=t.numpy(),
                                obs=obs.numpy(), clean=clean.numpy())
