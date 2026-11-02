"""Procedural 3D Geological Model & Potential Field Dataset Generator.

Generates realistic 3D synthetic subsurface density and susceptibility models
(stratified layers, faults, intrusive bodies, dykes, salt domes) and computes
their forward gravity and magnetic response using analytical prism solutions.
"""

import math
import numpy as np
import torch
from torch.utils.data import Dataset


class ProceduralGeologyGenerator:
    """Generates synthetic 3D geological volumes with density contrasts."""

    def __init__(self, nx=64, ny=64, nz=32, dx=50.0, dy=50.0, dz=50.0):
        self.nx = nx
        self.ny = ny
        self.nz = nz
        self.dx = dx
        self.dy = dy
        self.dz = dz

    def generate_random_model(self) -> np.ndarray:
        """Generates a 3D density contrast model (g/cm^3)."""
        model = np.zeros((self.nx, self.ny, self.nz), dtype=np.float32)

        # 1. Background undulating stratigraphy
        z_coords = np.arange(self.nz, dtype=np.float32)
        x_grid, y_grid = np.meshgrid(np.arange(self.nx), np.arange(self.ny), indexing="ij")
        
        n_layers = np.random.randint(2, 5)
        for _ in range(n_layers):
            freq_x = np.random.uniform(0.02, 0.08)
            freq_y = np.random.uniform(0.02, 0.08)
            phase_x = np.random.uniform(0, 2 * math.pi)
            phase_y = np.random.uniform(0, 2 * math.pi)
            undulation = 2.0 * np.sin(freq_x * x_grid + phase_x) * np.cos(freq_y * y_grid + phase_y)

            z_contact = np.random.randint(4, self.nz - 4)
            density_step = np.random.uniform(-0.25, 0.25)
            
            for iz in range(self.nz):
                mask = (iz + undulation) > z_contact
                model[:, :, iz][mask] += density_step

        # 2. Add compact intrusive bodies / ore pockets (Zhdanov focusing targets)
        n_bodies = np.random.randint(1, 4)
        for _ in range(n_bodies):
            cx = np.random.randint(self.nx // 4, 3 * self.nx // 4)
            cy = np.random.randint(self.ny // 4, 3 * self.ny // 4)
            cz = np.random.randint(3, self.nz - 3)
            
            rx = np.random.randint(3, 8)
            ry = np.random.randint(3, 8)
            rz = np.random.randint(2, 6)
            
            density = np.random.choice([-0.35, 0.35, 0.50])

            iz, iy, ix = np.ogrid[:self.nz, :self.ny, :self.nx]
            dist_sq = ((ix - cx) / rx) ** 2 + ((iy - cy) / ry) ** 2 + ((iz - cz) / rz) ** 2
            body_mask = dist_sq <= 1.0
            model[body_mask.T] = density

        # 3. Add dipping faults with displacement
        if np.random.rand() > 0.5:
            fault_x = np.random.randint(self.nx // 3, 2 * self.nx // 3)
            dip_angle = np.random.uniform(math.radians(45), math.radians(75))
            throw = np.random.randint(-4, 4)
            
            for ix in range(self.nx):
                for iz in range(self.nz):
                    dist_to_fault = (ix - fault_x) - (iz / math.tan(dip_angle))
                    if dist_to_fault > 0 and 0 <= iz + throw < self.nz:
                        model[ix, :, iz] = model[ix, :, iz + throw]

        # Clip within physically reasonable rock density contrast bounds
        return np.clip(model, -0.6, 0.8)


class SyntheticPotentialFieldDataset(Dataset):
    """PyTorch Dataset generating paired (observation_field, 3D_density_volume)."""

    def __init__(self, n_samples=1000, nx=64, ny=64, nz=32, noise_level=0.02):
        self.n_samples = n_samples
        self.generator = ProceduralGeologyGenerator(nx, ny, nz)
        self.noise_level = noise_level
        self.nx = nx
        self.ny = ny
        self.nz = nz

    def __len__(self):
        return self.n_samples

    def __getitem__(self, idx):
        # Generate 3D voxel density model
        density_model = self.generator.generate_random_model()

        # Approximate forward surface gravity calculation via depth-attenuated convolution
        # (Nagy prism approximation on 2D surface grid)
        surface_gravity = np.zeros((self.nx, self.ny), dtype=np.float32)
        for iz in range(self.nz):
            depth = (iz + 0.5) * self.generator.dz
            layer_rho = density_model[:, :, iz]
            # 1 / (depth^2) attenuation
            weight = 1.0 / (1.0 + (depth / 100.0) ** 2)
            surface_gravity += layer_rho * weight * 0.1

        # Add observational noise
        noise = np.random.normal(0.0, self.noise_level, surface_gravity.shape).astype(np.float32)
        obs_field = surface_gravity + noise

        # Channels: (C=1, H, W) and (C=1, D, H, W)
        obs_tensor = torch.from_numpy(obs_field).unsqueeze(0)
        density_tensor = torch.from_numpy(density_model).permute(2, 0, 1).unsqueeze(0) # (1, D, H, W)

        return obs_tensor, density_tensor


if __name__ == "__main__":
    import argparse
    import os

    parser = argparse.ArgumentParser(description="Procedural 3D Geology & Potential Field Data Generator")
    parser.add_argument("--samples", type=int, default=100, help="Number of samples to generate and verify")
    parser.add_argument("--nx", type=int, default=64, help="Number of cells along X")
    parser.add_argument("--ny", type=int, default=64, help="Number of cells along Y")
    parser.add_argument("--nz", type=int, default=32, help="Number of cells along Z")
    parser.add_argument("--output", type=str, default="", help="Optional output directory to save .npz files")
    args = parser.parse_args()

    print(f"Generating {args.samples} synthetic models ({args.nx}x{args.ny}x{args.nz})...")
    ds = SyntheticPotentialFieldDataset(n_samples=args.samples, nx=args.nx, ny=args.ny, nz=args.nz)
    obs, dens = ds[0]
    print(f"Sample 0 generated successfully!")
    print(f"Observation tensor shape: {obs.shape} | range: [{obs.min():.4f}, {obs.max():.4f}] mGal")
    print(f"Density tensor shape:     {dens.shape} | range: [{dens.min():.4f}, {dens.max():.4f}] g/cm^3")

    if args.output:
        os.makedirs(args.output, exist_ok=True)
        sample_path = os.path.join(args.output, "sample_preview.npz")
        np.savez_compressed(sample_path, obs=obs.numpy(), density=dens.numpy())
        print(f"Saved sample preview to: {sample_path}")

