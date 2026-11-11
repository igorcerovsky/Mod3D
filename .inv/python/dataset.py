"""Synthetic training data: ISOLATED bodies + cavities below a random terrain (anomaly only, no trend).

Each sample
-----------
* Random terrain (smooth Gaussian-filtered noise, optional tilt; 15 % flat), relief up to
  ``MAX_RELIEF_FRAC`` of the domain depth. ``t[i, j]`` = terrain depth below the grid top (z down,
  0 at the highest point). Stations are DRAPED on the terrain, ``clearance`` 0.30-0.40 m (30-40 cm,
  typical tripod/ground height for relative gravity meters) above it.
* 1 body (40 %) or 2-4 bodies (60 %), well separated (>= 2 empty cells), fully below the terrain
  and away from the lateral borders. Shapes cover compact pods/tunnels and flat/dipping tabular
  seams/sills across 6 physical non-background lithology classes (vs 2.67 g/cm^3 host rock):
    - air cavities               exact -2.67 g/cm^3 (shallow compact voids & tunnels)
    - water-filled cavities      exact -1.67 g/cm^3 (shallow flooded voids & conduits)
    - light rock / coal / clay   -1.55 ... -0.85 g/cm^3 (coal seams, soft porous clay, loose fill)
    - sediments / waste rock     -0.80 ... -0.10 g/cm^3 (sandstone, shale, compacted clay, evaporites)
    - mafic / ultramafic rock    +0.15 ... +0.75 g/cm^3 (basalt, gabbro, peridotite, skarn, weak ore)
    - high-grade mineral ore     +0.80 ... +1.85 g/cm^3 (massive sulfide, magnetite, chromite ore)
* Stratified shallow/mid/deep sampling plus an analytical SNR guard in multi-body scenes so weak
  bodies are not physically buried beneath the noise floor of a co-occurring cavity.
* Background contrast in the supervision target is strictly zero (terrain-corrected anomaly: the
  relief carries no contrast), while the forward gravity simulation adds 3D petrophysical voxel
  noise (internal rock heterogeneity + host-rock background density clutter below the terrain) so
  the network learns to reject geological voxel noise and recover clean, sharp bodies.
* Field: exact Nagy-prism response on the draped stations (``TerrainGravityOperator``) from the
  voxel-perturbed density volume plus station Gaussian noise with sigma = U(0.5 %, 4 %) * max|clean field|.
  No regional trend.

The cheap part (geometry, numpy) runs in DataLoader workers via ``BodyTerrainDataset``; the forward
model runs batched on the training device via ``simulate_batch``.
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
NOISE_FRAC = (0.005, 0.04)      # station noise sigma / max|field|
BODY_VOX_NOISE = 0.08           # 3D internal body voxel heterogeneity (std fraction of |rho|)
BG_VOX_NOISE_SCALE = 0.15       # 3D host-rock voxel clutter scale relative to noise_frac * max|rho|

# Calibrated physical density contrast bounds [g/cm^3 relative to 2.67 g/cm^3 host rock]
RHO_AIR = -2.67
RHO_WATER = -1.67
RHO_LIGHT_MIN, RHO_LIGHT_MAX = -1.55, -0.85
RHO_SEDIMENT_MIN, RHO_SEDIMENT_MAX = -0.80, -0.10
RHO_MAFIC_MIN, RHO_MAFIC_MAX = 0.15, 0.75
RHO_ORE_MIN, RHO_ORE_MAX = 0.80, 1.85
RHO_WASTE_MIN, RHO_WASTE_MAX = RHO_SEDIMENT_MIN, RHO_SEDIMENT_MAX


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


# (prob_among_rocks, rho_lo, rho_hi, tabular_prob, dip_range)
_ROCK_SPECS = (
    (0.21, RHO_LIGHT_MIN, RHO_LIGHT_MAX, 0.55, (-0.25, 0.25)),
    (0.53, RHO_SEDIMENT_MIN, RHO_SEDIMENT_MAX, 0.45, (-0.25, 0.25)),
    (0.85, RHO_MAFIC_MIN, RHO_MAFIC_MAX, 0.30, (-0.55, 0.55)),
    (1.00, RHO_ORE_MIN, RHO_ORE_MAX, 0.20, (-0.45, 0.45)),
)


def _sample_body_spec(rng, nz, hs, vs):
    """Draw (rx, ry, rz, gap, dip, rho_val, lo, hi, cavity, force_box) for one synthetic body."""
    if rng.random() < 0.24:
        gap = rng.uniform(0.5, max(1.2, 4.5 * vs))           # shallow cavity below terrain [cells]
        rho_val = RHO_AIR if rng.random() < 0.60 else RHO_WATER
        if rng.random() < 0.20:                              # horizontal tunnel / conduit
            rx, ry = rng.uniform(3.0, 6.2) * hs, rng.uniform(0.9, 1.8) * hs
            rz = rng.uniform(0.8, 1.8) * vs
            return rx, ry, rz, gap, 0.0, rho_val, rho_val, rho_val, True, True
        rx, ry = rng.uniform(1.2, 3.5, 2) * hs
        rz = rng.uniform(0.9, 2.5) * vs
        return rx, ry, rz, gap, 0.0, rho_val, rho_val, rho_val, True, False

    g_hi_sh = max(1.0, 4.0 * vs)
    g_hi_mid = max(g_hi_sh + 0.5, 9.0 * vs)
    g_hi_dp = max(g_hi_mid + 0.5, 0.50 * nz)
    u_depth = rng.random()
    if u_depth < 0.35:
        gap = rng.uniform(0.5, g_hi_sh)
    elif u_depth < 0.70:
        gap = rng.uniform(g_hi_sh, g_hi_mid)
    else:
        gap = rng.uniform(g_hi_mid, g_hi_dp)

    u_cls = rng.random()
    for cum_p, lo, hi, p_tab, dip_rng in _ROCK_SPECS:
        if u_cls <= cum_p:
            break
    rho_val = float(rng.uniform(lo, hi))
    if gap > 0.22 * nz and abs(rho_val) < 0.32:
        rho_val = float(np.clip(0.32 + 0.5 * rho_val if rho_val > 0 else -(0.32 + 0.35 * abs(rho_val)), lo, hi))
    if rng.random() < p_tab:                                 # tabular seam / bed / sill / vein
        rx = rng.uniform(4.0, 11.0) * hs
        ry = (rng.uniform(1.5, 5.5) if lo > 0 else rng.uniform(3.2, 9.5)) * hs
        rz = rng.uniform(0.7, 2.0) * vs
        dip = float(rng.uniform(*dip_rng)) if rng.random() < 0.50 else 0.0
    else:                                                    # compact / blocky body
        rx, ry = rng.uniform(1.6, 8.0, 2) * hs
        rz = rng.uniform(1.1, 5.2) * vs
        dip = float(rng.uniform(*dip_rng)) if rng.random() < 0.35 else 0.0
    rx, ry = max(rx, 0.20 * gap * hs), max(ry, 0.20 * gap * hs)
    rz = max(rz, 0.14 * gap * vs)
    return rx, ry, rz, gap, dip, rho_val, lo, hi, False, False


def random_bodies(rng, t, nz, dz, dx):
    """Density model (nz, nx, ny) with isolated bodies strictly below terrain depth ``t`` (nx, ny)."""
    nx, ny = t.shape
    rho = np.zeros((nz, nx, ny), np.float32)
    blocked = np.zeros((nz, nx, ny), bool)
    t_cells = t / dz                                         # terrain depth in layer units
    hs = nx / 64.0                                           # horizontal size scale
    vs = nz / 32.0                                           # vertical size scale
    n_bodies = 1 if rng.random() < 0.4 else int(rng.integers(2, 5))
    placed, attempts = 0, 0
    max_g_est = 0.0
    while placed < n_bodies and attempts < 60:
        attempts += 1
        rx, ry, rz, gap, dip, rho_val, lo, hi, cavity, force_box = _sample_body_spec(rng, nz, hs, vs)
        th = rng.uniform(0.0, math.pi)
        is_box = force_box or (rng.random() >= 0.55)
        cd, sd = math.cos(dip), math.sin(dip)
        ct, st = abs(math.cos(th)), abs(math.sin(th))
        rx_h = rx * abs(cd) + rz * abs(sd)
        if is_box:
            reach_x = rx_h * ct + ry * st
            reach_y = rx_h * st + ry * ct
        else:
            reach_x = math.hypot(rx_h * ct, ry * st)
            reach_y = math.hypot(rx_h * st, ry * ct)
        margin_x, margin_y = 3.0 + reach_x, 3.0 + reach_y
        if 2.0 * margin_x >= nx or 2.0 * margin_y >= ny:
            continue
        cx = rng.uniform(margin_x, nx - margin_x)
        cy = rng.uniform(margin_y, ny - margin_y)
        i0, i1 = int(max(3, cx - reach_x - 1)), int(min(nx - 3, cx + reach_x + 2))
        j0, j1 = int(max(3, cy - reach_y - 1)), int(min(ny - 3, cy + reach_y + 2))
        top_surface = float(t_cells[i0:i1, j0:j1].max())     # body must clear the highest relief above it
        z_extent = rz * abs(cd) + rx * abs(sd)
        cz = top_surface + gap + z_extent
        if cz + z_extent > nz - 0.5:
            continue
        k0, k1 = int(max(0, cz - z_extent - 1)), int(min(nz, cz + z_extent + 2))

        # Keep multi-body targets above the surface noise floor so strong cavities don't drown rocks
        est_vol = (4.0 / 3.0) * math.pi * rx * ry * rz
        z_eff = max(1.0, gap + z_extent)
        g_est = abs(rho_val) * est_vol / (z_eff * z_eff)
        if placed > 0 and max_g_est > 0.0 and g_est < 0.14 * max_g_est:
            if not cavity:
                rho_val = float(rng.uniform(0.75 * hi, hi) if rho_val > 0 else rng.uniform(lo, 0.75 * lo))
                g_est = abs(rho_val) * est_vol / (z_eff * z_eff)
            if g_est < 0.10 * max_g_est:
                continue

        zz, xx, yy = np.meshgrid(np.arange(k0, k1) + 0.5, np.arange(i0, i1) + 0.5,
                                 np.arange(j0, j1) + 0.5, indexing="ij")
        u0 = (xx - cx) * math.cos(th) + (yy - cy) * math.sin(th)
        v = -(xx - cx) * math.sin(th) + (yy - cy) * math.cos(th)
        w0 = zz - cz
        if dip != 0.0:
            u = u0 * cd + w0 * sd
            w = -u0 * sd + w0 * cd
        else:
            u, w = u0, w0
        if not is_box:
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
        g_actual = abs(rho_val) * float(sub.sum()) / (z_eff * z_eff)
        max_g_est = max(max_g_est, g_actual)
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
def simulate_batch(op: TerrainGravityOperator, rho, t, clearance, noise_frac, noise_seed=None,
                   body_vox_noise=BODY_VOX_NOISE, bg_vox_scale=BG_VOX_NOISE_SCALE):
    """Batched forward on ``op``'s device. Returns ``(obs_noisy, obs_clean, z_obs)``.

    ``obs_clean`` is the exact gravity field of the clean target ``rho``; ``obs_noisy`` is computed
    from ``rho + delta_rho`` (3D petrophysical voxel noise inside bodies and in the host rock below
    terrain) plus 2D station instrument noise, so the network learns to suppress both 3D subsurface
    density clutter and surface measurement noise while predicting the clean target ``rho``.
    """
    b, _, _, nx, ny = rho.shape
    z_obs = t - clearance.view(-1, 1, 1)
    if noise_seed is None:
        noise = torch.randn((b, 1, nx, ny), device=rho.device, dtype=rho.dtype)
        vox_n = torch.randn_like(rho)
    else:   # deterministic noise (validation)
        noise = torch.stack([torch.randn((1, nx, ny), generator=torch.Generator().manual_seed(int(s)))
                             for s in noise_seed.cpu()]).to(rho.device)
        vox_n = torch.stack([torch.randn(rho.shape[1:], generator=torch.Generator().manual_seed(int(s) + 1))
                             for s in noise_seed.cpu()]).to(rho.device)
    if body_vox_noise > 0.0 or bg_vox_scale > 0.0:
        z_top = torch.arange(op.nz, device=rho.device, dtype=rho.dtype).view(1, 1, op.nz, 1, 1) * op.dz
        below = (z_top >= t.unsqueeze(1).unsqueeze(2)).to(rho.dtype)
        amp = rho.abs().amax(dim=(1, 2, 3, 4), keepdim=True).clamp_min(0.1)
        sigma_vox = body_vox_noise * rho.abs() + bg_vox_scale * noise_frac.view(-1, 1, 1, 1, 1) * amp
        rho_vox = rho + sigma_vox * vox_n * below
        both = op(torch.cat([rho, rho_vox], dim=0), torch.cat([z_obs, z_obs], dim=0))
        clean, obs_vox = both[:b], both[b:]
    else:
        clean = op(rho, z_obs)
        obs_vox = clean
    peak = clean.abs().amax(dim=(1, 2, 3), keepdim=True)
    noisy = obs_vox + noise * noise_frac.view(-1, 1, 1, 1) * peak
    return noisy, clean, z_obs


def terrain_channel(t, depth_total):
    """Network terrain input: terrain depth / domain depth, (B, 1, nx, ny)."""
    return (t / depth_total).unsqueeze(1)


def augment_d4(rho, t, obs, clean, z_obs, k=None, flip=None):
    """Exact horizontal dihedral D4 symmetry augmentation (rotations + reflection) for gz inversion."""
    nx, ny = t.shape[-2], t.shape[-1]
    if k is None:
        k = int(torch.randint(0, 4 if nx == ny else 2, (1,)).item()) * (1 if nx == ny else 2)
    if flip is None:
        flip = bool(torch.randint(0, 2, (1,)).item())
    if k:
        rho = torch.rot90(rho, k, dims=(-2, -1))
        t = torch.rot90(t, k, dims=(-2, -1))
        obs = torch.rot90(obs, k, dims=(-2, -1))
        clean = torch.rot90(clean, k, dims=(-2, -1))
        z_obs = torch.rot90(z_obs, k, dims=(-2, -1))
    if flip:
        rho = torch.flip(rho, dims=(-1,))
        t = torch.flip(t, dims=(-1,))
        obs = torch.flip(obs, dims=(-1,))
        clean = torch.flip(clean, dims=(-1,))
        z_obs = torch.flip(z_obs, dims=(-1,))
    return rho, t, obs, clean, z_obs


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
