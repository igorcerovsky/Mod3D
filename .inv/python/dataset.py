"""Procedural 3D Geological Model & Potential Field Dataset Generator.

Synthetic subsurface density-contrast models (undulating strata, faults with throw, compact
intrusions) and their PHYSICAL gravity response, computed with the same Nagy-prism FFT operator
as the C++ engine (see ``forward.py``).

Design decisions (see README-train.md, "Training data"):

* **Anomaly models.** Gravity surveys are anomalies. The laterally uniform part of every depth
  layer (the horizontal-slab / regional component) is removed from the model, so the target
  contains only lateral density variations. This makes Minimum Support meaningful (the target
  is sparse-ish) and removes the edge-effect ambiguity of finite slabs.
* **Noise relative to signal.** Gaussian noise has sigma = noise_frac * max|clean field| with
  noise_frac ~ U(0.01, 0.05) per sample, so the signal-to-noise ratio is 20-100 by construction.
* **Reproducibility.** ``seed`` makes sample ``idx`` deterministic (validation sets); without it
  every access draws fresh entropy (also safe with multiple DataLoader workers).
* Flat observation plane at constant height; no topography is generated (the C++ engine handles
  topography at inversion time).
"""

import math

import numpy as np
import torch
from torch.utils.data import Dataset

from forward import GravityFFTOperator


class ProceduralGeologyGenerator:
    """Generates synthetic 3D geological volumes with lateral density contrasts (g/cm^3)."""

    def __init__(self, nx=64, ny=64, nz=32, dx=50.0, dy=50.0, dz=50.0):
        self.nx, self.ny, self.nz = nx, ny, nz
        self.dx, self.dy, self.dz = dx, dy, dz

    def generate_random_model(self, rng=None) -> np.ndarray:
        """Returns a (nz, nx, ny) float32 density-contrast model."""
        rng = rng if rng is not None else np.random.default_rng()
        nx, ny, nz = self.nx, self.ny, self.nz
        model = np.zeros((nz, nx, ny), dtype=np.float32)
        xg, yg = np.meshgrid(np.arange(nx), np.arange(ny), indexing="ij")
        zg = np.arange(nz)[:, None, None]
        scale = nx / 64.0

        # 1. Undulating strata
        for _ in range(int(rng.integers(2, 5))):
            fx = rng.uniform(0.02, 0.08) / scale
            fy = rng.uniform(0.02, 0.08) / scale
            ph_x, ph_y = rng.uniform(0, 2 * math.pi, 2)
            undulation = 2.0 * np.sin(fx * xg + ph_x) * np.cos(fy * yg + ph_y)
            z_contact = int(rng.integers(max(2, nz // 8), max(3, nz - nz // 8)))
            step = rng.uniform(-0.25, 0.25)
            model += (step * ((zg + undulation[None]) > z_contact)).astype(np.float32)

        # 2. Faults with throw (strike along x or y). Copy from the ORIGINAL model (no read-after-write).
        if rng.random() > 0.5:
            src = model.copy()
            axis_len = nx
            fault = int(rng.integers(axis_len // 3, 2 * axis_len // 3))
            dip = rng.uniform(math.radians(45), math.radians(75))
            throw = int(rng.integers(-max(1, nz // 8), max(1, nz // 8) + 1))
            along_x = rng.random() > 0.5
            for iz in range(nz):
                src_iz = iz + throw
                if not 0 <= src_iz < nz:
                    continue
                shift = iz / math.tan(dip)
                for i in range(axis_len):
                    if (i - fault) - shift > 0:
                        if along_x:
                            model[iz, i, :] = src[src_iz, i, :]
                        else:
                            model[iz, :, i] = src[src_iz, :, i]

        # 3. Compact intrusions / ore pockets (focusing targets)
        iz_g, ix_g, iy_g = np.ogrid[:nz, :nx, :ny]
        for _ in range(int(rng.integers(1, 4))):
            cx = int(rng.integers(nx // 4, 3 * nx // 4))
            cy = int(rng.integers(ny // 4, 3 * ny // 4))
            cz = int(rng.integers(max(2, nz // 10), max(3, nz - nz // 10)))
            rx = max(1.5, rng.uniform(3, 8) * scale)
            ry = max(1.5, rng.uniform(3, 8) * scale)
            rz = max(1.0, rng.uniform(2, 6) * nz / 32.0)
            density = float(rng.choice([-0.35, 0.35, 0.50]))
            d2 = ((ix_g - cx) / rx) ** 2 + ((iy_g - cy) / ry) ** 2 + ((iz_g - cz) / rz) ** 2
            model[d2 <= 1.0] = density

        # Anomaly model: remove the laterally uniform component of every depth layer.
        model -= model.mean(axis=(1, 2), keepdims=True)
        return np.clip(model, -0.6, 0.8)


class SyntheticPotentialFieldDataset(Dataset):
    """Paired (noisy gravity map, 3D density-contrast volume) samples.

    __getitem__ returns ``(obs, density)`` with shapes ``(1, nx, ny)`` and ``(1, nz, nx, ny)``.
    Set ``return_clean=True`` to also get the noise-free field (for diagnostics).
    """

    def __init__(self, n_samples=1000, nx=64, ny=64, nz=32, dx=50.0, dy=50.0, dz=50.0,
                 noise_frac=(0.01, 0.05), z_obs=-10.0, seed=None, return_clean=False, operator=None):
        self.n_samples = n_samples
        self.nx, self.ny, self.nz = nx, ny, nz
        self.generator = ProceduralGeologyGenerator(nx, ny, nz, dx, dy, dz)
        self.noise_frac = noise_frac if isinstance(noise_frac, (tuple, list)) else (noise_frac, noise_frac)
        self.seed = seed
        self.return_clean = return_clean
        self.operator = operator or GravityFFTOperator(nx, ny, nz, dx, dy, dz, z_obs=z_obs)

    def __len__(self):
        return self.n_samples

    def _rng(self, idx):
        if self.seed is None:
            return np.random.default_rng()
        return np.random.default_rng([self.seed, idx])

    def __getitem__(self, idx):
        rng = self._rng(idx)
        density = self.generator.generate_random_model(rng)             # (nz, nx, ny)
        clean = self.operator.forward_numpy(density)                    # (nx, ny) mGal
        peak = float(np.abs(clean).max())
        frac = rng.uniform(*self.noise_frac)
        noise = rng.normal(0.0, frac * peak, clean.shape).astype(np.float32)
        obs = torch.from_numpy(clean + noise).unsqueeze(0)
        dens = torch.from_numpy(density).unsqueeze(0)
        if self.return_clean:
            return obs, dens, torch.from_numpy(clean).unsqueeze(0)
        return obs, dens


if __name__ == "__main__":
    import argparse
    import os

    parser = argparse.ArgumentParser(description="Procedural 3D Geology & Potential Field Data Generator")
    parser.add_argument("--samples", type=int, default=100)
    parser.add_argument("--nx", type=int, default=64)
    parser.add_argument("--ny", type=int, default=64)
    parser.add_argument("--nz", type=int, default=32)
    parser.add_argument("--output", type=str, default="", help="Optional directory for a .npz preview")
    args = parser.parse_args()

    print(f"Generating {args.samples} synthetic models ({args.nx}x{args.ny}x{args.nz})...")
    ds = SyntheticPotentialFieldDataset(n_samples=args.samples, nx=args.nx, ny=args.ny, nz=args.nz, seed=0)
    obs, dens = ds[0]
    print(f"Observation tensor shape: {tuple(obs.shape)} | range: [{obs.min():.4f}, {obs.max():.4f}] mGal")
    print(f"Density tensor shape:     {tuple(dens.shape)} | range: [{dens.min():.4f}, {dens.max():.4f}] g/cm^3")
    if args.output:
        os.makedirs(args.output, exist_ok=True)
        path = os.path.join(args.output, "sample_preview.npz")
        np.savez_compressed(path, obs=obs.numpy(), density=dens.numpy())
        print(f"Saved sample preview to: {path}")
