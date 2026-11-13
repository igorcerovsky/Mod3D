"""Synthetic training data: standard isolated & harder composite/contact bodies below random terrain.

Each sample
-----------
* Random terrain (smooth Gaussian-filtered noise, optional tilt; 15 % flat), relief up to
  ``MAX_RELIEF_FRAC`` of the domain depth. ``t[i, j]`` = terrain depth below the grid top (z down,
  0 at the highest point). Stations are DRAPED on the terrain, ``clearance`` 0.30-0.40 m (30-40 cm,
  typical tripod/ground height for relative gravity meters) above it.
* By default (``hard_prob=0.50``), half of scenes are standard isolated 1-4 bodies (>= 2 empty cell
  clearance) and half draw from 5 harder well-posed geological archetypes while remaining bounded
  inside the observation flanks (no full-domain DC slabs) and balanced in surface SNR:
    - ``large_composite``: multi-lobe plutons, bent seams, L-shapes (~1-2.4 km)
    - ``nested_halo``: ore/cavity cores embedded in mafic/sediment/light host envelopes
    - ``abutting_contact``: zero-gap lithology contacts, fault throws & dyke-sill intersections
    - ``vertical_stack``: shallow lenses overlying deeper balanced dense targets
    - ``tight_cluster``: 3-5 closely spaced (1-cell gap) anomalies under rough terrain
  Across all 6 physical non-background lithology classes (vs 2.67 g/cm^3 host rock):
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
CLEARANCE_MULTI = (0.30, 80.0)  # mixed ground + drone/airborne clearance range for v5 [m]
ASPECT_MULTI = (0.55, 0.75, 1.0, 1.20, 1.40)  # vertical-to-horizontal cell aspect ratio dz/dx range for v5
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


# Hard scene archetypes mixed into training (see random_bodies / hard_prob)
HARD_SCENE_TYPES = ("large_composite", "nested_halo", "abutting_contact", "vertical_stack", "tight_cluster")
_HARD_SCENE_PROBS = (0.20, 0.20, 0.18, 0.21, 0.21)


# (prob_among_rocks, rho_lo, rho_hi, tabular_prob, dip_range)
_ROCK_SPECS = (
    (0.21, RHO_LIGHT_MIN, RHO_LIGHT_MAX, 0.55, (-0.25, 0.25)),
    (0.53, RHO_SEDIMENT_MIN, RHO_SEDIMENT_MAX, 0.45, (-0.25, 0.25)),
    (0.85, RHO_MAFIC_MIN, RHO_MAFIC_MAX, 0.30, (-0.55, 0.55)),
    (1.00, RHO_ORE_MIN, RHO_ORE_MAX, 0.20, (-0.45, 0.45)),
)


def _pick_rock_lithology(rng, nz, gap, class_probs=None):
    """Draw (rho_val, lo, hi, p_tab, dip_range) from the 4 continuous non-cavity lithology bands."""
    u_cls = rng.random()
    specs = _ROCK_SPECS if class_probs is None else tuple(
        (cp, lo, hi, p_tab, dip_r) for cp, (_, lo, hi, p_tab, dip_r) in zip(class_probs, _ROCK_SPECS)
    )
    for cum_p, lo, hi, p_tab, dip_rng in specs:
        if u_cls <= cum_p:
            break
    rho_val = float(rng.uniform(lo, hi))
    if gap > 0.22 * nz and abs(rho_val) < 0.32:
        rho_val = float(np.clip(0.32 + 0.5 * rho_val if rho_val > 0 else -(0.32 + 0.35 * abs(rho_val)), lo, hi))
    return rho_val, lo, hi, p_tab, dip_rng


def _sample_body_spec(rng, nz, hs, vs):
    """Draw (rx, ry, rz, gap, dip, rho_val, lo, hi, cavity, force_box) for one synthetic body."""
    if rng.random() < 0.24:
        gap = rng.uniform(0.5, max(1.2, 4.5 * vs))           # shallow cavity below terrain [cells]
        rho_val = RHO_AIR if rng.random() < 0.50 else RHO_WATER
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

    rho_val, lo, hi, p_tab, dip_rng = _pick_rock_lithology(rng, nz, gap)
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


def _primitive_reaches(rx, ry, rz, th, dip, is_box):
    cd, sd = math.cos(dip), math.sin(dip)
    ct, st = abs(math.cos(th)), abs(math.sin(th))
    rx_h = rx * abs(cd) + rz * abs(sd)
    if is_box:
        reach_x = rx_h * ct + ry * st
        reach_y = rx_h * st + ry * ct
    else:
        reach_x = math.hypot(rx_h * ct, ry * st)
        reach_y = math.hypot(rx_h * st, ry * ct)
    z_extent = rz * abs(cd) + rx * abs(sd)
    return reach_x, reach_y, z_extent


def _rasterize_primitive(t_cells, nz, cx, cy, cz, rx, ry, rz, th, dip, is_box, cavity=False, border_margin=3,
                         taper=0.0, fold_amp=0.0, fold_freq=2.2, bend=0.0):
    """Rasterize an ellipsoid or box (with optional geological taper, folding & arcuate bend) below ``t_cells``."""
    nx, ny = t_cells.shape
    rz_eff = rz * (1.0 + abs(float(fold_amp)) + 0.35 * abs(float(taper)))
    ry_eff = ry * (1.0 + 0.35 * abs(float(bend)))
    reach_x, reach_y, z_extent = _primitive_reaches(rx, ry_eff, rz_eff, th, dip, is_box)
    i0 = int(max(border_margin, math.floor(cx - reach_x - 1.0)))
    i1 = int(min(nx - border_margin, math.ceil(cx + reach_x + 2.0)))
    j0 = int(max(border_margin, math.floor(cy - reach_y - 1.0)))
    j1 = int(min(ny - border_margin, math.ceil(cy + reach_y + 2.0)))
    k0 = int(max(0, math.floor(cz - z_extent - 1.0)))
    k1 = int(min(nz, math.ceil(cz + z_extent + 2.0)))
    if i1 <= i0 or j1 <= j0 or k1 <= k0:
        return None
    zz, xx, yy = np.meshgrid(np.arange(k0, k1) + 0.5, np.arange(i0, i1) + 0.5,
                             np.arange(j0, j1) + 0.5, indexing="ij")
    ct, st = math.cos(th), math.sin(th)
    u0 = (xx - cx) * ct + (yy - cy) * st
    v = -(xx - cx) * st + (yy - cy) * ct
    w0 = zz - cz
    if dip != 0.0:
        cd, sd = math.cos(dip), math.sin(dip)
        u = u0 * cd + w0 * sd
        w = -u0 * sd + w0 * cd
    else:
        u, w = u0, w0
    u_norm = np.clip(u / max(rx, 0.5), -1.2, 1.2)
    if bend != 0.0:
        v = v - float(bend) * ry * (u_norm * u_norm - 0.30)
    if fold_amp != 0.0:
        w = w - float(fold_amp) * rz * np.sin(float(fold_freq) * u_norm)
    rz_loc = rz * np.clip(1.0 + float(taper) * u_norm, 0.45, 1.45) if taper != 0.0 else rz
    if not is_box:
        sub = (u / max(rx, 0.25)) ** 2 + (v / max(ry, 0.25)) ** 2 + (w / np.maximum(rz_loc, 0.25)) ** 2 <= 1.0
    else:
        sub = (np.abs(u) <= rx) & (np.abs(v) <= ry) & (np.abs(w) <= np.maximum(rz_loc, 0.25))
    min_top = t_cells[None, i0:i1, j0:j1] + (0.5 if cavity else 0.0)
    z_top = np.arange(k0, k1)[:, None, None]
    sub &= (z_top >= min_top)
    if not sub.any():
        return None
    mask = np.zeros((nz, nx, ny), dtype=bool)
    mask[k0:k1, i0:i1, j0:j1] = sub
    return mask


def _dilate_into_blocked(blocked, mask, iters: int):
    """Update ``blocked`` in-place by dilating ``mask`` only within its padded bounding box."""
    if iters <= 0:
        blocked |= mask
        return
    kz, kx, ky = np.nonzero(mask)
    if not kz.size:
        return
    nz, nx, ny = mask.shape
    z0, z1 = max(0, int(kz.min()) - iters), min(nz, int(kz.max()) + iters + 1)
    x0, x1 = max(0, int(kx.min()) - iters), min(nx, int(kx.max()) + iters + 1)
    y0, y1 = max(0, int(ky.min()) - iters), min(ny, int(ky.max()) + iters + 1)
    blocked[z0:z1, x0:x1, y0:y1] |= binary_dilation(mask[z0:z1, x0:x1, y0:y1], iterations=iters)


def _body_gravity_est(rho_val, mask, t_cells):
    """Analytical buried-voxel gravity proxy ``|rho| * V / z_eff^2``."""
    kz, kx, ky = np.nonzero(mask)
    if not kz.size:
        return 0.0
    z_eff = max(1.0, float((kz - t_cells[kx, ky]).mean()) + 0.5)
    return abs(float(rho_val)) * float(kz.size) / (z_eff * z_eff)


def _place_isolated_cluster(rng, rho, t_cells, nz, hs, vs, n_bodies, dilate_iters=2,
                            anchor=None, cluster_spread=None, min_g_ratio=0.14):
    """Place ``n_bodies`` separated bodies with >= ``dilate_iters`` cell clearance & SNR parity."""
    nx, ny = t_cells.shape
    blocked = np.zeros((nz, nx, ny), bool)
    if (rho != 0).any():
        _dilate_into_blocked(blocked, rho != 0, dilate_iters)
    max_g_est = 0.0
    if (rho != 0).any():
        for v in np.unique(rho[rho != 0]):
            max_g_est = max(max_g_est, _body_gravity_est(v, rho == v, t_cells))
    placed, attempts = 0, 0
    while placed < n_bodies and attempts < 80:
        attempts += 1
        rx, ry, rz, gap, dip, rho_val, lo, hi, cavity, force_box = _sample_body_spec(rng, nz, hs, vs)
        th = rng.uniform(0.0, math.pi)
        is_box = force_box or (rng.random() >= 0.55)
        reach_x, reach_y, z_extent = _primitive_reaches(rx, ry, rz, th, dip, is_box)
        margin_x, margin_y = 3.0 + reach_x, 3.0 + reach_y
        if 2.0 * margin_x >= nx or 2.0 * margin_y >= ny:
            continue
        if anchor is not None and cluster_spread is not None:
            lo_x = max(margin_x, anchor[0] - cluster_spread)
            hi_x = min(nx - margin_x, anchor[0] + cluster_spread)
            lo_y = max(margin_y, anchor[1] - cluster_spread)
            hi_y = min(ny - margin_y, anchor[1] + cluster_spread)
            cx = rng.uniform(lo_x, hi_x) if hi_x > lo_x else rng.uniform(margin_x, nx - margin_x)
            cy = rng.uniform(lo_y, hi_y) if hi_y > lo_y else rng.uniform(margin_y, ny - margin_y)
        else:
            cx = rng.uniform(margin_x, nx - margin_x)
            cy = rng.uniform(margin_y, ny - margin_y)
        i0, i1 = int(max(3, cx - reach_x - 1)), int(min(nx - 3, cx + reach_x + 2))
        j0, j1 = int(max(3, cy - reach_y - 1)), int(min(ny - 3, cy + reach_y + 2))
        top_surface = float(t_cells[i0:i1, j0:j1].max())
        cz = top_surface + gap + z_extent
        if cz + z_extent > nz - 0.5:
            continue

        est_vol = (4.0 / 3.0) * math.pi * rx * ry * rz
        z_eff = max(1.0, gap + z_extent)
        g_est = abs(rho_val) * est_vol / (z_eff * z_eff)
        if max_g_est > 0.0 and g_est < min_g_ratio * max_g_est:
            if not cavity:
                rho_val = float(rng.uniform(0.75 * hi, hi) if rho_val > 0 else rng.uniform(lo, 0.75 * lo))
                g_est = abs(rho_val) * est_vol / (z_eff * z_eff)
            if g_est < 0.75 * min_g_ratio * max_g_est:
                continue

        # Deterministic continuous geological shape modifiers (wedge taper, folding, arcuate bend)
        h1 = math.sin(cx * 12.9898 + cy * 78.233 + th * 37.719)
        h2 = math.cos(cx * 39.346 + cy * 11.135 - th * 53.19)
        taper = 0.28 * h1 if (not cavity and rx > 2.2 * rz and abs(h1) > 0.35) else 0.0
        fold_amp = 0.22 * h2 if (not cavity and rx > 2.5 * rz and abs(h2) > 0.45) else 0.0
        bend = 0.20 * (h1 * h2) if (rx > 1.8 * ry) else 0.0
        mask = _rasterize_primitive(
            t_cells, nz, cx, cy, cz, rx, ry, rz, th, dip, is_box, cavity=cavity,
            taper=taper, fold_amp=fold_amp, bend=bend,
        )
        if mask is None or (mask & blocked).any():
            continue
        rho[mask] = rho_val
        _dilate_into_blocked(blocked, mask, dilate_iters)
        max_g_est = max(max_g_est, _body_gravity_est(rho_val, mask, t_cells))
        placed += 1
    return placed


def _pick_parity_rock(rng, nz, gap, mask, t_cells, ref_g, min_ratio=0.20, max_ratio=5.0, class_probs=None):
    """Sample a continuous rock density whose surface gravity stays within ``[min_ratio, max_ratio]`` of ``ref_g``."""
    rho_val, lo, hi, _, _ = _pick_rock_lithology(rng, nz, gap, class_probs=class_probs)
    if ref_g <= 0.0:
        return True, rho_val
    g_raw = _body_gravity_est(rho_val, mask, t_cells)
    if min_ratio <= (g_raw / ref_g) <= max_ratio:
        return True, rho_val
    # Retarget within the same physical band if amplitude adjustment keeps physical bounds
    kz, kx, ky = np.nonzero(mask)
    if not kz.size:
        return False, rho_val
    z_eff = max(1.0, float((kz - t_cells[kx, ky]).mean()) + 0.5)
    geom = float(kz.size) / (z_eff * z_eff)
    need_abs = ref_g * (0.35 if g_raw < min_ratio * ref_g else 3.0) / max(geom, 1e-9)
    rho_try = math.copysign(need_abs, lo)
    if lo <= rho_try <= hi:
        return True, float(rho_try)
    rho_edge = float(hi if (lo > 0 and g_raw < min_ratio * ref_g) or (lo < 0 and g_raw > max_ratio * ref_g) else lo)
    g_edge = abs(rho_edge) * geom
    return (min_ratio <= (g_edge / ref_g) <= max_ratio), rho_edge


def _build_large_composite(rng, t_cells, nz, hs, vs):
    """Build a large multi-lobe non-convex body (pluton, bent seam, L-shape) well inside interior flanks."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), np.float32)
    max_reach = max(3.5, 0.34 * (nx - 6))
    gap = rng.uniform(0.5, max(1.5, 5.0 * vs))
    rho_val, lo, hi, _, dip_rng = _pick_rock_lithology(rng, nz, gap, class_probs=(0.20, 0.55, 0.90, 1.00))
    rx = min(max_reach * 0.85, rng.uniform(7.5, 15.5) * hs)
    ry = min(max_reach * 0.85, rng.uniform(5.0, 12.5) * hs)
    rz = min(0.28 * nz, rng.uniform(3.0, 6.8) * vs)
    th = rng.uniform(0.0, math.pi)
    dip = float(rng.uniform(*dip_rng)) if rng.random() < 0.45 else 0.0
    is_box = rng.random() >= 0.50
    reach_x, reach_y, z_ext = _primitive_reaches(rx, ry, rz, th, dip, is_box)
    mx = min(0.42 * nx, 3.0 + reach_x)
    my = min(0.42 * ny, 3.0 + reach_y)
    cx = rng.uniform(mx, nx - mx) if nx > 2.0 * mx else 0.5 * nx
    cy = rng.uniform(my, ny - my) if ny > 2.0 * my else 0.5 * ny
    i0, i1 = int(max(3, cx - reach_x)), int(min(nx - 3, cx + reach_x + 1))
    j0, j1 = int(max(3, cy - reach_y)), int(min(ny - 3, cy + reach_y + 1))
    cols = t_cells[max(0, i0):max(i0 + 1, i1), max(0, j0):max(j0 + 1, j1)]
    # Mix ridge-hosted bodies (truncated naturally by terrain) and valley-cleared deep roots
    top_ref = float(np.percentile(cols, rng.uniform(35.0, 95.0))) if cols.size else float(t_cells.max())
    cz = min(nz - z_ext - 0.5, top_ref + gap + z_ext)
    mask = _rasterize_primitive(t_cells, nz, cx, cy, cz, rx, ry, rz, th, dip, is_box)
    min_vox = max(12, int(35 * hs * hs * vs))
    if mask is None or mask.sum() < min_vox:
        return rho

    # Union 1-3 attached shifted lobes of identical lithology to create non-convex shapes (L, elbow, branched)
    n_lobes = int(rng.integers(1, 4))
    max_span = 0.78 * (nx - 6)
    for _ in range(n_lobes):
        ang = rng.uniform(0.0, 2.0 * math.pi)
        dx_s = math.cos(ang) * rng.uniform(0.40, 0.85) * rx
        dy_s = math.sin(ang) * rng.uniform(0.40, 0.85) * ry
        dz_s = rng.uniform(-0.45, 0.55) * rz
        rx2 = min(max_reach * 0.75, rx * rng.uniform(0.45, 0.90))
        ry2 = min(max_reach * 0.75, ry * rng.uniform(0.45, 0.90))
        rz2 = min(0.24 * nz, rz * rng.uniform(0.50, 0.95))
        th2 = th + rng.uniform(-0.8, 0.8)
        dip2 = float(np.clip(dip + rng.uniform(-0.3, 0.3), -0.65, 0.65))
        cx2 = float(np.clip(cx + dx_s, 3.0 + 0.5 * rx2, nx - 3.0 - 0.5 * rx2))
        cy2 = float(np.clip(cy + dy_s, 3.0 + 0.5 * ry2, ny - 3.0 - 0.5 * ry2))
        cz2 = float(np.clip(cz + dz_s, 1.0, nz - rz2 - 0.5))
        lobe = _rasterize_primitive(t_cells, nz, cx2, cy2, cz2, rx2, ry2, rz2, th2, dip2, rng.random() >= 0.45)
        if lobe is not None:
            cand = mask | lobe
            _, kx_c, ky_c = np.nonzero(cand)
            if (kx_c.max() - kx_c.min() + 1) <= max_span and (ky_c.max() - ky_c.min() + 1) <= max_span:
                mask = cand

    rho[mask] = rho_val
    if rng.random() < 0.35:
        _place_isolated_cluster(rng, rho, t_cells, nz, hs, vs, 1, dilate_iters=2, min_g_ratio=0.25)
    return rho


def _build_nested_halo(rng, t_cells, nz, hs, vs):
    """Nested core-in-host zones (ore-in-mafic, cavity-in-host, coal-in-sediment) with SNR parity."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), np.float32)
    max_reach = max(3.5, 0.32 * (nx - 6))
    for _ in range(16):
        gap = rng.uniform(0.6, max(1.5, 4.5 * vs))
        motif = rng.random()
        if motif < 0.45:                              # high-grade ore core inside mafic/ultramafic halo
            host_probs = (0.0, 0.0, 1.0, 1.0)
            core_kind = "ore"
        elif motif < 0.80:                            # air/water cavity inside sediment/light/ore host
            host_probs = (0.25, 0.65, 0.85, 1.0)
            core_kind = "cavity"
        else:                                         # contrasting core inside sediment/light host
            host_probs = (0.35, 1.0, 1.0, 1.0)
            core_kind = "mafic_or_light"
        rho_host, _, _, _, dip_rng = _pick_rock_lithology(rng, nz, gap, class_probs=host_probs)
        rx = min(max_reach, rng.uniform(5.5, 12.0) * hs)
        ry = min(max_reach, rng.uniform(4.5, 10.5) * hs)
        rz = min(0.24 * nz, rng.uniform(2.8, 5.5) * vs)
        th = rng.uniform(0.0, math.pi)
        dip = float(rng.uniform(*dip_rng)) if rng.random() < 0.35 else 0.0
        is_box = rng.random() >= 0.55
        reach_x, reach_y, z_ext = _primitive_reaches(rx, ry, rz, th, dip, is_box)
        if 2.0 * (3.0 + reach_x) >= nx or 2.0 * (3.0 + reach_y) >= ny:
            continue
        cx = rng.uniform(3.0 + reach_x, nx - 3.0 - reach_x)
        cy = rng.uniform(3.0 + reach_y, ny - 3.0 - reach_y)
        i0, i1 = int(max(3, cx - reach_x)), int(min(nx - 3, cx + reach_x + 1))
        j0, j1 = int(max(3, cy - reach_y)), int(min(ny - 3, cy + reach_y + 1))
        cols = t_cells[i0:i1, j0:j1]
        top_ref = float(np.percentile(cols, rng.uniform(45.0, 95.0))) if cols.size else float(t_cells.max())
        cz = top_ref + gap + z_ext
        if cz + z_ext > nz - 0.5:
            continue
        host_mask = _rasterize_primitive(t_cells, nz, cx, cy, cz, rx, ry, rz, th, dip, is_box)
        if host_mask is None or host_mask.sum() < max(16, int(36 * hs * hs * vs)):
            continue

        # Build eccentric inner core replacing ~14%-55% of host
        f_xy = rng.uniform(0.45, 0.72)
        f_z = rng.uniform(0.45, 0.72)
        core_cav = core_kind == "cavity"
        core_mask = _rasterize_primitive(
            t_cells, nz,
            cx + rng.uniform(-0.20, 0.20) * rx,
            cy + rng.uniform(-0.20, 0.20) * ry,
            cz + rng.uniform(-0.20, 0.20) * rz,
            rx * f_xy, ry * f_xy, rz * f_z, th, dip, is_box, cavity=core_cav,
        )
        if core_mask is None:
            continue
        core_in_host = core_mask & host_mask
        frac = float(core_in_host.sum()) / float(host_mask.sum())
        if frac < 0.12 or frac > 0.58:
            continue
        shell_mask = host_mask & (~core_mask)
        if shell_mask.sum() < max(10, int(18 * hs * hs * vs)):
            continue

        g_host = _body_gravity_est(rho_host, shell_mask, t_cells)
        if core_cav:
            rho_core = RHO_AIR if rng.random() < 0.48 else RHO_WATER
            g_core = _body_gravity_est(rho_core, core_mask, t_cells)
            ok = 0.20 <= (g_core / max(g_host, 1e-9)) <= 5.0
        else:
            probs_c = (0.0, 0.0, 0.0, 1.0) if core_kind == "ore" else ((1.0, 1.0, 1.0, 1.0) if rho_host > -0.8 else (0.0, 0.0, 1.0, 1.0))
            ok, rho_core = _pick_parity_rock(rng, nz, gap, core_mask, t_cells, g_host, class_probs=probs_c)
        if not ok:
            continue
        rho[host_mask] = rho_host
        rho[core_mask] = rho_core
        return rho
    return rho


def _build_abutting_contact(rng, t_cells, nz, hs, vs):
    """Zero-gap lithology contacts, faulted offsets, and dyke-meets-sill intersections."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), np.float32)
    max_reach = max(3.0, 0.28 * (nx - 6))
    for _ in range(18):
        gap1 = rng.uniform(0.8, max(2.0, 7.0 * vs))
        rho1, _, _, _, dip_rng1 = _pick_rock_lithology(rng, nz, gap1)
        is_dyke_sill = rng.random() < 0.42
        if is_dyke_sill:
            rx1, ry1, rz1 = rng.uniform(5.0, 10.0) * hs, rng.uniform(3.0, 7.0) * hs, rng.uniform(1.0, 2.2) * vs
            dip1 = float(rng.uniform(-0.25, 0.25))
            is_box1 = True
        else:
            rx1 = min(max_reach, rng.uniform(4.0, 9.5) * hs)
            ry1 = min(max_reach, rng.uniform(3.0, 7.5) * hs)
            rz1 = rng.uniform(1.8, 4.6) * vs
            dip1 = float(rng.uniform(*dip_rng1)) if rng.random() < 0.4 else 0.0
            is_box1 = rng.random() >= 0.45
        th1 = rng.uniform(0.0, math.pi)
        r1x, r1y, z1e = _primitive_reaches(rx1, ry1, rz1, th1, dip1, is_box1)
        if 2.0 * (3.0 + r1x) >= nx or 2.0 * (3.0 + r1y) >= ny:
            continue
        cx1 = rng.uniform(3.0 + r1x, nx - 3.0 - r1x)
        cy1 = rng.uniform(3.0 + r1y, ny - 3.0 - r1y)
        top1 = float(t_cells[int(max(3, cx1 - r1x)):int(min(nx - 3, cx1 + r1x + 1)),
                             int(max(3, cy1 - r1y)):int(min(ny - 3, cy1 + r1y + 1))].max())
        cz1 = top1 + gap1 + z1e
        if cz1 + z1e > nz - 0.5:
            continue
        m1 = _rasterize_primitive(t_cells, nz, cx1, cy1, cz1, rx1, ry1, rz1, th1, dip1, is_box1)
        if m1 is None or m1.sum() < max(10, int(18 * hs * hs * vs)):
            continue
        g1 = _body_gravity_est(rho1, m1, t_cells)

        if is_dyke_sill:
            # Steep feeder/dyke intersecting the underside/flank of sill m1
            rx2, ry2, rz2 = rng.uniform(2.0, 4.2) * hs, rng.uniform(1.6, 4.2) * hs, rng.uniform(3.2, 6.2) * vs
            dip2 = float(rng.choice([-1.0, 1.0]) * rng.uniform(0.55, 1.05))
            th2 = th1 + rng.uniform(-0.5, 0.5)
            cx2 = cx1 + rng.uniform(-0.35, 0.35) * rx1
            cy2 = cy1 + rng.uniform(-0.35, 0.35) * ry1
            cz2 = cz1 + rz1 * 0.4 + rz2 * 0.75
        else:
            # Side-by-side contact or fault throw along/across strike
            rx2 = min(max_reach, rx1 * rng.uniform(0.65, 1.25))
            ry2 = min(max_reach, ry1 * rng.uniform(0.65, 1.25))
            rz2 = rz1 * rng.uniform(0.75, 1.25)
            th2 = th1 + rng.uniform(-0.3, 0.3)
            dip2 = dip1
            along = rng.random() < 0.55
            sgn = float(rng.choice([-1.0, 1.0]))
            step = (rx1 + rx2) * rng.uniform(0.72, 0.96) if along else (ry1 + ry2) * rng.uniform(0.72, 0.96)
            ang = th1 if along else (th1 + 0.5 * math.pi)
            cx2 = cx1 + sgn * math.cos(ang) * step
            cy2 = cy1 + sgn * math.sin(ang) * step
            z_throw = rng.choice([-1.0, 1.0]) * rng.uniform(0.0, 3.5) * vs
            cz2 = cz1 + z_throw

        r2x, r2y, z2e = _primitive_reaches(rx2, ry2, rz2, th2, dip2, is_box1)
        if cx2 - r2x < 3.0 or cx2 + r2x > nx - 3.0 or cy2 - r2y < 3.0 or cy2 + r2y > ny - 3.0:
            continue
        if cz2 - z2e < 0.0 or cz2 + z2e > nz - 0.5:
            continue
        m2 = _rasterize_primitive(t_cells, nz, cx2, cy2, cz2, rx2, ry2, rz2, th2, dip2, is_box1)
        if m2 is None:
            continue
        m2_clean = m2 & (~m1)
        if m2_clean.sum() < max(10, int(16 * hs * hs * vs)):
            continue
        # Ensure bodies actually abut or sit within 1 voxel face
        if not (binary_dilation(m1, iterations=1) & m2_clean).any():
            continue
        same_fault = (not is_dyke_sill) and (rng.random() < 0.20)
        if same_fault:
            rho2 = rho1
            g2 = _body_gravity_est(rho2, m2_clean, t_cells)
            ok = 0.20 <= (g2 / max(g1, 1e-9)) <= 5.0
        else:
            ok, rho2 = _pick_parity_rock(rng, nz, max(0.5, cz2 - z2e - top1), m2_clean, t_cells, g1)
            if ok and abs(rho2 - rho1) < 0.25:
                continue
        if not ok:
            continue
        rho[m1] = rho1
        rho[m2_clean] = rho2
        return rho
    return rho


def _build_vertical_stack(rng, t_cells, nz, hs, vs):
    """Vertically stacked / depth-shielded bodies at contrasting depths with balanced surface mGal."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), np.float32)
    for _ in range(18):
        # 1. Shallow wide/moderate cap (or shallow water cavity)
        cav_sh = rng.random() < 0.25
        gap1 = rng.uniform(0.7, max(1.4, 3.2 * vs))
        rx1, ry1 = rng.uniform(3.2, 8.5, 2) * hs
        rz1 = rng.uniform(1.0, 2.5) * vs
        th1 = rng.uniform(0.0, math.pi)
        is_box1 = rng.random() >= 0.45
        r1x, r1y, z1e = _primitive_reaches(rx1, ry1, rz1, th1, 0.0, is_box1)
        if 2.0 * (3.0 + r1x) >= nx or 2.0 * (3.0 + r1y) >= ny:
            continue
        cx1 = rng.uniform(3.5 + r1x, nx - 3.5 - r1x)
        cy1 = rng.uniform(3.5 + r1y, ny - 3.5 - r1y)
        top_local = float(t_cells[int(max(3, cx1 - r1x)):int(min(nx - 3, cx1 + r1x + 1)),
                                  int(max(3, cy1 - r1y)):int(min(ny - 3, cy1 + r1y + 1))].max())
        cz1 = top_local + gap1 + z1e
        if cz1 + z1e > 0.45 * nz:
            continue
        m1 = _rasterize_primitive(t_cells, nz, cx1, cy1, cz1, rx1, ry1, rz1, th1, 0.0, is_box1, cavity=cav_sh)
        if m1 is None or m1.sum() < max(8, int(14 * hs * hs * vs)):
            continue
        if cav_sh:
            rho1 = RHO_WATER if rng.random() < 0.65 else RHO_AIR
        else:
            rho1, _, _, _, _ = _pick_rock_lithology(rng, nz, gap1, class_probs=(0.30, 0.70, 0.95, 1.00))
        g1 = _body_gravity_est(rho1, m1, t_cells)

        # 2. Deeper larger/stronger target beneath shallow cap (0-3 cells vertical gap)
        rx2, ry2 = rng.uniform(4.0, 10.5, 2) * hs
        rz2 = rng.uniform(2.2, 5.2) * vs
        th2 = rng.uniform(0.0, math.pi)
        dip2 = float(rng.uniform(-0.35, 0.35)) if rng.random() < 0.4 else 0.0
        r2x, r2y, z2e = _primitive_reaches(rx2, ry2, rz2, th2, dip2, False)
        cx2 = float(np.clip(cx1 + rng.uniform(-3.0, 3.0) * hs, 3.0 + r2x, nx - 3.0 - r2x))
        cy2 = float(np.clip(cy1 + rng.uniform(-3.0, 3.0) * hs, 3.0 + r2y, ny - 3.0 - r2y))
        v_gap = rng.uniform(0.0, 3.5 * vs)
        cz2 = cz1 + z1e + v_gap + z2e
        if cz2 + z2e > nz - 0.5 or cx2 - r2x < 3.0 or cy2 - r2y < 3.0:
            continue
        m2 = _rasterize_primitive(t_cells, nz, cx2, cy2, cz2, rx2, ry2, rz2, th2, dip2, rng.random() >= 0.55)
        if m2 is None:
            continue
        m2_clean = m2 & (~m1)
        if m2_clean.sum() < max(10, int(18 * hs * hs * vs)):
            continue
        ok, rho2 = _pick_parity_rock(rng, nz, cz2 - z2e - top_local, m2_clean, t_cells, g1,
                                      min_ratio=0.25, max_ratio=4.0, class_probs=(0.15, 0.30, 0.65, 1.00))
        if not ok:
            continue
        rho[m1] = rho1
        rho[m2_clean] = rho2
        return rho
    return rho


def apply_planar_fault(rng, rho, t_cells, border_margin=3, max_throw_cells=4):
    """Cut ``rho`` with a dipping 3D planar fault and shift one half-space by 1-``max_throw_cells``."""
    nz, nx, ny = rho.shape
    kz, kx, ky = np.nonzero(rho)
    if kz.size < 8:
        return rho

    # Pivot inside/near the active body centroid so the fault visibly bisects the structure
    pz = float(np.clip(np.median(kz) + rng.uniform(-1.5, 1.5), 1.0, nz - 2.0))
    px = float(np.clip(np.median(kx) + rng.uniform(-3.0, 3.0), border_margin + 1.0, nx - border_margin - 1.0))
    py = float(np.clip(np.median(ky) + rng.uniform(-3.0, 3.0), border_margin + 1.0, ny - border_margin - 1.0))

    phi = float(rng.uniform(0.0, 2.0 * math.pi))
    dip = float(rng.uniform(math.radians(35.0), math.radians(82.0)))
    nz_n = math.cos(dip)
    nx_n = math.sin(dip) * math.cos(phi)
    ny_n = math.sin(dip) * math.sin(phi)

    zz, xx, yy = np.meshgrid(np.arange(nz), np.arange(nx), np.arange(ny), indexing="ij")
    signed_dist = (zz - pz) * nz_n + (xx - px) * nx_n + (yy - py) * ny_n
    half = signed_dist > 0.0
    nz_pos = (rho != 0) & half
    nz_neg = (rho != 0) & (~half)
    if nz_pos.sum() < 4 or nz_neg.sum() < 4:
        return rho

    throw_z = int(rng.choice([-1, 1]) * rng.integers(1, max(2, int(max_throw_cells) + 1)))
    # Dip-slip horizontal heave along the horizontal component of fault normal (normal/reverse fault)
    heave_scale = rng.uniform(0.25, 0.85) * throw_z
    dx_i = int(round(heave_scale * math.cos(phi)))
    dy_i = int(round(heave_scale * math.sin(phi)))
    if throw_z == 0 and dx_i == 0 and dy_i == 0:
        return rho

    out = np.where(half, 0.0, rho).astype(np.float32)
    sz, sx, sy = np.nonzero(nz_pos)
    tz, tx, ty = sz + throw_z, sx + dx_i, sy + dy_i
    ok = (
        (tz >= 0) & (tz < nz)
        & (tx >= border_margin) & (tx < nx - border_margin)
        & (ty >= border_margin) & (ty < ny - border_margin)
    )
    if ok.any():
        tz, tx, ty, vals = tz[ok], tx[ok], ty[ok], rho[sz[ok], sx[ok], sy[ok]]
        below = tz >= t_cells[tx, ty]
        if below.any():
            out[tz[below], tx[below], ty[below]] = vals[below]

    if (out != 0).sum() < max(6, int(0.35 * (rho != 0).sum())):
        return rho
    return out


def random_bodies(rng, t, nz, dz, dx, hard_prob=0.50, scene_type=None, fault_prob=0.0):
    """Density model (nz, nx, ny) strictly below terrain depth ``t`` (nx, ny).

    Draws either standard isolated bodies (``dilate_iters=2``, ``1-4`` bodies) or, with
    probability ``hard_prob``, one of 5 physically well-posed harder geological archetypes
    (``large_composite``, ``nested_halo``, ``abutting_contact``, ``vertical_stack``,
    ``tight_cluster``). Never places full-domain DC slabs or sub-noise masked ghosts.
    When ``fault_prob > 0``, optionally cuts the scene with a true 3D dipping planar fault.
    """
    nx, ny = t.shape
    t_cells = t / dz                                         # terrain depth in layer units
    hs = nx / 64.0                                           # horizontal size scale
    vs = nz / 32.0                                           # vertical size scale
    rho = np.zeros((nz, nx, ny), np.float32)

    can_hard = min(nx, ny) >= 24 and nz >= 12
    if scene_type is None and can_hard and rng.random() < hard_prob:
        scene_type = str(rng.choice(HARD_SCENE_TYPES, p=_HARD_SCENE_PROBS))

    if scene_type == "large_composite":
        rho = _build_large_composite(rng, t_cells, nz, hs, vs)
    elif scene_type == "nested_halo":
        rho = _build_nested_halo(rng, t_cells, nz, hs, vs)
    elif scene_type == "abutting_contact":
        rho = _build_abutting_contact(rng, t_cells, nz, hs, vs)
    elif scene_type == "vertical_stack":
        rho = _build_vertical_stack(rng, t_cells, nz, hs, vs)
    elif scene_type == "tight_cluster":
        n_b = int(rng.integers(3, 6))
        anchor = (rng.uniform(0.28 * nx, 0.72 * nx), rng.uniform(0.28 * ny, 0.72 * ny))
        _place_isolated_cluster(rng, rho, t_cells, nz, hs, vs, n_b, dilate_iters=1,
                                anchor=anchor, cluster_spread=0.22 * nx, min_g_ratio=0.16)
    else:
        n_b = 1 if rng.random() < 0.4 else int(rng.integers(2, 5))
        _place_isolated_cluster(rng, rho, t_cells, nz, hs, vs, n_b, dilate_iters=2)

    if not (rho != 0).any():
        _place_isolated_cluster(rng, rho, t_cells, nz, hs, vs, 1 if rng.random() < 0.4 else int(rng.integers(2, 5)),
                                dilate_iters=2)
    if not (rho != 0).any():
        # Guaranteed center rock body on degenerate small/steep test grids
        rx_g, ry_g, rz_g = max(1.2, 3.0 * hs), max(1.2, 3.0 * hs), max(1.0, 2.0 * vs)
        top_c = float(t_cells[max(0, nx // 2 - 2):min(nx, nx // 2 + 3), max(0, ny // 2 - 2):min(ny, ny // 2 + 3)].max())
        cz_g = min(nz - rz_g - 0.5, top_c + 1.0 + rz_g)
        m_g = _rasterize_primitive(t_cells, nz, 0.5 * nx, 0.5 * ny, cz_g, rx_g, ry_g, rz_g, 0.0, 0.0, False)
        if m_g is not None:
            rho_g, *_ = _pick_rock_lithology(rng, nz, 2.0)
            rho[m_g] = rho_g

    if fault_prob > 0.0 and rng.random() < fault_prob:
        rho = apply_planar_fault(rng, rho, t_cells)

    # safety: no density in cells whose top is above the terrain
    above = (np.arange(nz)[:, None, None] * dz) < t[None]
    rho[above] = 0.0
    return rho


def _sample_clearance(rng, clearance_range):
    lo, hi = float(clearance_range[0]), float(clearance_range[1])
    if hi <= 1.0:
        return np.float32(rng.uniform(lo, hi))
    # Mix ground surveys (~50%) and log-uniform drone/airborne drape heights (~50%)
    if rng.random() < 0.50:
        return np.float32(rng.uniform(lo, min(1.5, hi)))
    log_lo = math.log(max(lo, 1.5))
    log_hi = math.log(max(hi, 1.51))
    return np.float32(math.exp(rng.uniform(log_lo, log_hi)))


def sample_geometry(rng, nx=NX, ny=NY, nz=NZ, dx=CELL, dy=CELL, dz=CELL,
                    hard_prob=0.50, fault_prob=0.0, clearance_range=CLEARANCE):
    t = random_terrain(rng, nx, ny, nz * dz)
    rho = random_bodies(rng, t, nz, dz, dx, hard_prob=hard_prob, fault_prob=fault_prob)
    clearance = _sample_clearance(rng, clearance_range)
    noise_frac = np.float32(rng.uniform(*NOISE_FRAC))
    return rho, t, clearance, noise_frac


class BodyTerrainDataset(Dataset):
    """Yields geometry only: ``(rho (1,nz,nx,ny), t (nx,ny), clearance, noise_frac, noise_seed)``
    (or appends ``aspect_idx`` when ``aspect_ratios`` is provided).

    ``seed=None`` -> fresh random sample on every access (infinite training data);
    ``seed=int``  -> sample ``idx`` is deterministic (validation).
    """

    def __init__(self, n_samples=2000, nx=NX, ny=NY, nz=NZ, dx=CELL, dy=CELL, dz=CELL,
                 seed=None, hard_prob=0.50, fault_prob=0.0, clearance_range=CLEARANCE,
                 aspect_ratios=None):
        self.n, self.seed, self.hard_prob = n_samples, seed, float(hard_prob)
        self.fault_prob = float(fault_prob)
        self.clearance_range = tuple(clearance_range)
        self.aspect_ratios = tuple(float(a) for a in aspect_ratios) if aspect_ratios else None
        self.g = (nx, ny, nz, dx, dy, dz)

    def __len__(self):
        return self.n

    def __getitem__(self, idx):
        rng = np.random.default_rng() if self.seed is None else np.random.default_rng([self.seed, idx])
        nx, ny, nz, dx, dy, dz = self.g
        aspect_idx = 0
        dz_cur = dz
        if self.aspect_ratios is not None:
            aspect_idx = int(rng.integers(0, len(self.aspect_ratios)))
            dz_cur = dx * self.aspect_ratios[aspect_idx]
        rho, t, c, nf = sample_geometry(
            rng, nx, ny, nz, dx, dy, dz_cur,
            hard_prob=self.hard_prob, fault_prob=self.fault_prob,
            clearance_range=self.clearance_range,
        )
        noise_seed = int(rng.integers(0, 2**31 - 1))
        items = (torch.from_numpy(rho)[None], torch.from_numpy(t), torch.tensor(c), torch.tensor(nf),
                 torch.tensor(noise_seed))
        if self.aspect_ratios is not None:
            return items + (torch.tensor(aspect_idx, dtype=torch.long),)
        return items


@torch.no_grad()
def infill_missing_obs(obs: torch.Tensor, valid_mask: torch.Tensor, iters: int = 24) -> torch.Tensor:
    """Fill missing stations (``valid_mask == 0``) via neighbor pull + Neumann harmonic relaxation.

    Eliminates sharp jumps to 0.0 that poison spatial Laplacian/gradient stems in 3D neural priors.
    """
    if bool((valid_mask > 0.5).all()):
        return obs
    b, _, nx, ny = obs.shape
    known = (valid_mask > 0.5).to(obs.dtype)
    k_mean = torch.ones((1, 1, 3, 3), device=obs.device, dtype=obs.dtype)
    k_lap = torch.tensor(
        [[[[0.0, 0.25, 0.0], [0.25, 0.0, 0.25], [0.0, 0.25, 0.0]]]],
        device=obs.device, dtype=obs.dtype,
    )
    u = obs * known
    cur_known = known
    max_steps = max(nx, ny) + 2
    for _ in range(max_steps):
        if bool((cur_known > 0.5).all()):
            break
        k_pad = torch.nn.functional.pad(cur_known, (1, 1, 1, 1), mode="replicate")
        u_pad = torch.nn.functional.pad(u, (1, 1, 1, 1), mode="replicate")
        w_sum = torch.nn.functional.conv2d(k_pad, k_mean)
        v_sum = torch.nn.functional.conv2d(u_pad, k_mean)
        can = (cur_known < 0.5) & (w_sum > 0.5)
        u = torch.where(can, v_sum / w_sum.clamp_min(1.0), u)
        cur_known = torch.where(can, torch.ones_like(cur_known), cur_known)

    missing = known < 0.5
    for _ in range(iters):
        u_pad = torch.nn.functional.pad(u, (1, 1, 1, 1), mode="replicate")
        u_avg = torch.nn.functional.conv2d(u_pad, k_lap)
        u = torch.where(missing, u_avg, u)
    return u


def _rand_uniform(rng, low, high, shape, device, dtype):
    if rng is None:
        return torch.empty(shape, device=device, dtype=dtype).uniform_(low, high)
    vals = rng.uniform(low, high, size=shape).astype(np.float32)
    return torch.from_numpy(vals).to(device=device, dtype=dtype)


@torch.no_grad()
def simulate_batch(op: TerrainGravityOperator, rho, t, clearance, noise_frac, noise_seed=None,
                   body_vox_noise=BODY_VOX_NOISE, bg_vox_scale=BG_VOX_NOISE_SCALE,
                   dropout_prob: float = 0.0, return_mask: bool = False):
    """Batched forward on ``op``'s device. Returns ``(obs_noisy, obs_clean, z_obs[, valid_mask])``.

    ``obs_clean`` is the exact gravity field of the clean target ``rho``; ``obs_noisy`` is computed
    from ``rho + delta_rho`` (3D petrophysical voxel noise below terrain) plus spatially correlated
    sensor noise and optional missing-station dropout + harmonic infill. Regional trends and
    regolith overburden are excluded (detrended prior to inversion).
    """
    b, _, nz, nx, ny = rho.shape
    z_obs = t - clearance.view(-1, 1, 1)
    rng_np = None if noise_seed is None else np.random.default_rng(int(noise_seed.flatten()[0].item()) + 17)
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
        # Mix white voxel heterogeneity with 3x3x3 spatially correlated petrophysical clutter
        vox_corr = torch.nn.functional.avg_pool3d(vox_n, kernel_size=3, stride=1, padding=1) * 3.0
        vox_mix = 0.72 * vox_n + 0.28 * vox_corr
        rho_vox = rho + sigma_vox * vox_mix * below

        try:
            both = op(torch.cat([rho, rho_vox], dim=1), z_obs)
            clean, obs_vox = both[:, 0:1], both[:, 1:2]
        except Exception:
            both = op(torch.cat([rho, rho_vox], dim=0), torch.cat([z_obs, z_obs], dim=0))
            clean, obs_vox = both[:b], both[b:]
    else:
        clean = op(rho, z_obs)
        obs_vox = clean

    peak = clean.abs().amax(dim=(1, 2, 3), keepdim=True).clamp_min(1e-6)
    noise_corr = torch.nn.functional.avg_pool2d(noise, kernel_size=3, stride=1, padding=1) * 2.1
    noise_mix = 0.75 * noise + 0.25 * noise_corr
    noisy = obs_vox + noise_mix * noise_frac.view(-1, 1, 1, 1) * peak

    valid_mask = torch.ones((b, 1, nx, ny), device=rho.device, dtype=rho.dtype)
    if dropout_prob > 0.0:
        for i in range(b):
            u_d = float(_rand_uniform(rng_np, 0.0, 1.0, (1,), rho.device, rho.dtype).item())
            if u_d >= dropout_prob:
                continue
            vm = valid_mask[i, 0]
            mode = int(math.floor(u_d / max(dropout_prob, 1e-6) * 3.0)) % 3
            if mode == 0:  # irregular cutout / survey corner/border missing patch
                x0 = int(_rand_uniform(rng_np, 0, max(1, nx // 2), (1,), rho.device, rho.dtype).item())
                y0 = int(_rand_uniform(rng_np, 0, max(1, ny // 2), (1,), rho.device, rho.dtype).item())
                w_x = int(_rand_uniform(rng_np, max(2, nx // 8), max(3, nx // 3), (1,), rho.device, rho.dtype).item())
                w_y = int(_rand_uniform(rng_np, max(2, ny // 8), max(3, ny // 3), (1,), rho.device, rho.dtype).item())
                vm[x0:min(nx, x0 + w_x), y0:min(ny, y0 + w_y)] = 0.0
            elif mode == 1:  # missing flight/survey traverse lines
                step = max(2, int(_rand_uniform(rng_np, 3, 6, (1,), rho.device, rho.dtype).item()))
                phase = int(_rand_uniform(rng_np, 0, step, (1,), rho.device, rho.dtype).item())
                if phase % 2 == 0:
                    vm[phase::step, :] = 0.0
                else:
                    vm[:, phase::step] = 0.0
            else:  # random sparse dummy holes
                keep = _rand_uniform(rng_np, 0.0, 1.0, (nx, ny), rho.device, rho.dtype) > 0.22
                vm.mul_(keep.to(rho.dtype))
            # Safety floor: keep >= 45% valid stations so infill remains physical
            if float(vm.mean().item()) < 0.45:
                vm.fill_(1.0)
        noisy = infill_missing_obs(noisy, valid_mask)

    if return_mask:
        return noisy, clean, z_obs, valid_mask
    return noisy, clean, z_obs


def terrain_channel(t, depth_total):
    """Network terrain input: terrain depth / domain depth, (B, 1, nx, ny)."""
    return (t / depth_total).unsqueeze(1)


def build_v5_input(obs, t, depth_total, valid_mask=None, aspect=1.0, clearance=0.35, dxy_canon=CELL):
    """Build 5-channel v5 input ``(B, 5, H, W)``:
    ``[gz_50m_eq, terr/depth, valid_mask, log(dz/dxy), clearance/dxy_canon]``.
    """
    b, _, nx, ny = obs.shape
    terr = terrain_channel(t, depth_total)
    if valid_mask is None:
        valid_mask = torch.ones_like(obs)
    if isinstance(aspect, torch.Tensor):
        asp_t = aspect.to(device=obs.device, dtype=obs.dtype).view(b, 1, 1, 1).expand(b, 1, nx, ny)
    else:
        asp_t = torch.full((b, 1, nx, ny), float(aspect), device=obs.device, dtype=obs.dtype)
    if isinstance(clearance, torch.Tensor):
        clr_t = clearance.to(device=obs.device, dtype=obs.dtype).view(b, 1, 1, 1).expand(b, 1, nx, ny)
    else:
        clr_t = torch.full((b, 1, nx, ny), float(clearance), device=obs.device, dtype=obs.dtype)
    log_asp = torch.log(asp_t.clamp(0.1, 10.0))
    clr_norm = (clr_t / float(dxy_canon)).clamp(0.0, 4.0)
    return torch.cat([obs, terr, valid_mask, log_asp, clr_norm], dim=1)


def augment_d4(rho, t, obs, clean, z_obs, valid_mask=None, k=None, flip=None):
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
        if valid_mask is not None:
            valid_mask = torch.rot90(valid_mask, k, dims=(-2, -1))
    if flip:
        rho = torch.flip(rho, dims=(-1,))
        t = torch.flip(t, dims=(-1,))
        obs = torch.flip(obs, dims=(-1,))
        clean = torch.flip(clean, dims=(-1,))
        z_obs = torch.flip(z_obs, dims=(-1,))
        if valid_mask is not None:
            valid_mask = torch.flip(valid_mask, dims=(-1,))
    if valid_mask is not None:
        return rho, t, obs, clean, z_obs, valid_mask
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
