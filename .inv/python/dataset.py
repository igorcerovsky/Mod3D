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
from scipy.ndimage import binary_dilation, gaussian_filter, label
from torch.utils.data import Dataset

from forward import G_MGAL, TerrainGravityOperator

# Training geometry (the C++ inference resamples any domain onto this grid; see neural_prior.hpp)
NX, NY, NZ, CELL = 64, 64, 32, 50.0
MAX_RELIEF_FRAC = 0.25          # max terrain relief / domain depth
CLEARANCE = (0.30, 0.40)        # ground gravimeter sensor height above terrain [m] (30-40 cm)
CLEARANCE_MULTI = (0.30, 80.0)  # mixed ground + drone/airborne clearance range for v5 [m]
CLEARANCE_AIRBORNE_V6 = (2.0, 160.0)  # secondary airborne relief-drape band for v6 [m]
CLEARANCE_HIGH_DRAPE_MAX_V6 = 215.0   # hard ceiling inside TerrainGravityOperator(h_min=-220)
CLEARANCE_CLAMP_V6 = 8.0        # canonical ch-4 ceiling: 8.0 * 50 m = 400 m (matches neural_prior.hpp)
GROUND_PROB_V6 = 0.67           # ~67% primary ground surveys [0.30, 2.0 m], ~33% airborne
LARGE_BODY_PROB_V6 = 0.22       # ~22% large-area landfill / quarry / plume / shallow basin / regional pluton
ASPECT_MULTI = (0.55, 0.75, 1.0, 1.20, 1.40)  # vertical-to-horizontal cell aspect ratio dz/dx range for v5/v6
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
RHO_LANDFILL_MIN, RHO_LANDFILL_MAX = -0.85, -0.05
RHO_WEAK_MIN, RHO_WEAK_MAX = 0.05, 0.80

V6_SCENE_TYPES = (
    "landfill_plume",
    "broad_basin_pluton",
    "folded_horizons",
    "listric_graben",
    "intrusive_plumbing",
    "basement_cover",
    "graded_halo",
    "large_composite",
    "nested_halo",
    "abutting_contact",
    "vertical_stack",
    "tight_cluster",
    "isolated",
)


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


def random_bodies(rng, t, nz, dz, dx, hard_prob=0.50, scene_type=None, fault_prob=0.0, return_meta=False):
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
    chosen_scene_type = scene_type or "isolated"

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
        chosen_scene_type = "isolated"
        _place_isolated_cluster(rng, rho, t_cells, nz, hs, vs, 1 if rng.random() < 0.4 else int(rng.integers(2, 5)),
                                dilate_iters=2)
    if not (rho != 0).any():
        # Guaranteed center rock body on degenerate small/steep test grids
        chosen_scene_type = "isolated"
        rx_g, ry_g, rz_g = max(1.2, 3.0 * hs), max(1.2, 3.0 * hs), max(1.0, 2.0 * vs)
        top_c = float(t_cells[max(0, nx // 2 - 2):min(nx, nx // 2 + 3), max(0, ny // 2 - 2):min(ny, ny // 2 + 3)].max())
        cz_g = min(nz - rz_g - 0.5, top_c + 1.0 + rz_g)
        m_g = _rasterize_primitive(t_cells, nz, 0.5 * nx, 0.5 * ny, cz_g, rx_g, ry_g, rz_g, 0.0, 0.0, False)
        if m_g is not None:
            rho_g, *_ = _pick_rock_lithology(rng, nz, 2.0)
            rho[m_g] = rho_g

    fault_applied = False
    if fault_prob > 0.0 and rng.random() < fault_prob:
        rho_before_fault = rho.copy()
        rho = apply_planar_fault(rng, rho, t_cells)
        fault_applied = not np.array_equal(rho, rho_before_fault)

    # safety: no density in cells whose top is above the terrain
    above = (np.arange(nz)[:, None, None] * dz) < t[None]
    rho[above] = 0.0
    if return_meta:
        return rho, {"scene_type": chosen_scene_type, "fault_applied": bool(fault_applied)}
    return rho


def _finalize_units_v6(rho, unit_id, t, dz, max_units=16):
    """Clip above terrain, synchronize nonzero voxels and unit_id, and compact IDs to 1..<=max_units."""
    nz = rho.shape[0]
    above = (np.arange(nz)[:, None, None] * dz) < t[None]
    rho[above] = 0.0
    unit_id[above] = 0
    nz_mask = np.abs(rho) >= 1e-4
    rho[~nz_mask] = 0.0
    unit_id[~nz_mask] = 0

    orphan = nz_mask & (unit_id <= 0)
    if orphan.any():
        unit_id[orphan] = int(unit_id.max()) + 1

    pos_uids = np.unique(unit_id[unit_id > 0])
    if pos_uids.size > max_units:
        counts = [(int((unit_id == u).sum()), int(u)) for u in pos_uids]
        counts.sort(reverse=True)
        keep = [u for _, u in counts[: max_units - 1]]
        remap = {u: i + 1 for i, u in enumerate(keep)}
        new_uid = np.where(unit_id > 0, max_units, 0).astype(np.int32)
        for u, dst in remap.items():
            new_uid[unit_id == u] = dst
        unit_id = new_uid
    elif pos_uids.size > 0 and (pos_uids[-1] != pos_uids.size or pos_uids[0] != 1):
        lut = np.zeros(int(pos_uids[-1]) + 1, dtype=np.int32)
        lut[pos_uids] = np.arange(1, pos_uids.size + 1, dtype=np.int32)
        unit_id = lut[unit_id]
    return rho.astype(np.float32, copy=False), unit_id.astype(np.int32, copy=False)


def _border_occupancy_2d(mask_2d):
    """Fraction of outer 1-cell grid perimeter pixels occupied by ``mask_2d``."""
    nx, ny = mask_2d.shape
    if nx < 2 or ny < 2:
        return 0.0
    perim = (
        int(mask_2d[0, :].sum())
        + int(mask_2d[-1, :].sum())
        + int(mask_2d[1:-1, 0].sum())
        + int(mask_2d[1:-1, -1].sum())
    )
    total = 2 * nx + 2 * (ny - 2)
    return float(perim) / float(max(1, total))


def _trim_border_for_detrend(rng, mask_2d, max_border_frac=0.72):
    """Ensure ``mask_2d`` never blankets >72% of outer perimeter so BorderPlanar remains well-posed."""
    if _border_occupancy_2d(mask_2d) <= max_border_frac:
        return mask_2d
    nx, ny = mask_2d.shape
    out = mask_2d.copy()
    strip = max(2, min(6, min(nx, ny) // 8))
    sides = [0, 1, 2, 3]
    rng.shuffle(sides)
    for s in sides:
        if _border_occupancy_2d(out) <= max_border_frac:
            break
        if s == 0:
            out[:strip, :] = False
        elif s == 1:
            out[-strip:, :] = False
        elif s == 2:
            out[:, :strip] = False
        else:
            out[:, -strip:] = False
    return out


def _pick_v6_contrast(rng, nz, gap, prefer_weak=False, allow_cavity=True):
    """Draw v6 density contrast covering weak (-0.85..-0.05, +0.05..+0.80) and standard rock bands."""
    if allow_cavity and rng.random() < 0.11:
        rho_val = RHO_AIR if rng.random() < 0.50 else RHO_WATER
        return float(rho_val), rho_val, rho_val, True
    if prefer_weak or rng.random() < 0.45:
        if rng.random() < 0.50:
            lo, hi = RHO_LANDFILL_MIN, RHO_LANDFILL_MAX
        else:
            lo, hi = RHO_WEAK_MIN, RHO_WEAK_MAX
        # Sample bias toward realistic moderate/weak exploration contrasts 0.08..0.55
        if rng.random() < 0.65:
            rho_val = float(rng.uniform(-0.55, -0.07) if lo < 0 else rng.uniform(0.07, 0.55))
        else:
            rho_val = float(rng.uniform(lo, hi))
        return rho_val, lo, hi, False
    rho_val, lo, hi, _, _ = _pick_rock_lithology(rng, nz, gap)
    return float(rho_val), float(lo), float(hi), False


def _apply_density_field_v6(rng, mask, base_rho, t_cells, lo, hi, mode="linear"):
    """Assign smooth linear Mod3D density_gradient, compaction, or internal clutter inside ``mask``."""
    kz, kx, ky = np.nonzero(mask)
    if kz.size == 0:
        return np.zeros(mask.shape, dtype=np.float32)
    out = np.zeros(mask.shape, dtype=np.float32)
    z0, z1 = int(kz.min()), int(kz.max()) + 1
    x0, x1 = int(kx.min()), int(kx.max()) + 1
    y0, y1 = int(ky.min()), int(ky.max()) + 1
    sub_m = mask[z0:z1, x0:x1, y0:y1]

    if mode == "compaction":
        # Depth below local terrain (shallowest magnitude at top -> denser compaction at depth)
        zz = np.arange(z0, z1, dtype=np.float32)[:, None, None]
        d_burial = np.maximum(0.0, zz - t_cells[None, x0:x1, y0:y1])
        d_span = max(2.0, float(d_burial[sub_m].max() - d_burial[sub_m].min()))
        frac = np.clip((d_burial - float(d_burial[sub_m].min())) / d_span, 0.0, 1.0)
        # Negative waste/sediment compacts from strong negative (-0.85..-0.40) toward weaker contrast
        if base_rho < 0:
            r_top = float(np.clip(base_rho - rng.uniform(0.10, 0.30), lo, min(hi, -0.15)))
            r_bot = float(np.clip(base_rho + rng.uniform(0.15, 0.45), r_top + 0.05, hi))
            vals = r_top + (r_bot - r_top) * (1.0 - np.exp(-2.2 * frac))
        else:
            r_top = float(np.clip(base_rho * rng.uniform(0.55, 0.85), lo, hi))
            r_bot = float(np.clip(base_rho * rng.uniform(1.05, 1.35), r_top + 0.04, hi))
            vals = r_top + (r_bot - r_top) * frac
    else:
        # Mod3D exact linear Body::density_gradient: rho_0 + g . (r - r_center)
        zc, xc, yc = float(kz.mean()), float(kx.mean()), float(ky.mean())
        zz, xx, yy = np.meshgrid(
            np.arange(z0, z1, dtype=np.float32) - zc,
            np.arange(x0, x1, dtype=np.float32) - xc,
            np.arange(y0, y1, dtype=np.float32) - yc,
            indexing="ij",
        )
        span_r = max(2.0, float(max(z1 - z0, x1 - x0, y1 - y0)))
        amp_g = rng.uniform(0.08, 0.28) * max(0.25, abs(base_rho)) / span_r
        gz_v = rng.uniform(-1.0, 1.0) * amp_g * 1.4
        gx_v = rng.uniform(-0.7, 0.7) * amp_g
        gy_v = rng.uniform(-0.7, 0.7) * amp_g
        vals = np.clip(base_rho + gz_v * zz + gx_v * xx + gy_v * yy, lo, hi)

    sgn = -1.0 if base_rho < 0 else 1.0
    if sgn < 0:
        vals = np.minimum(vals, -0.04)
    else:
        vals = np.maximum(vals, 0.04)
    out[z0:z1, x0:x1, y0:y1][sub_m] = vals[sub_m].astype(np.float32)
    return out


def apply_continuous_fault_v6(rng, rho, unit_id, t_cells, mode="random", max_throw_cells=4):
    """Apply continuous zero-tear coordinate-pullback displacement (planar, listric, or graben/horst)."""
    nz, nx, ny = rho.shape
    kz, kx, ky = np.nonzero(rho)
    if kz.size < 8:
        return rho, unit_id, False

    if mode == "random":
        mode = str(rng.choice(["planar", "listric", "graben"]))

    pz = float(np.clip(np.median(kz) + rng.uniform(-2.0, 2.0), 1.0, nz - 2.0))
    px = float(np.clip(np.median(kx) + rng.uniform(-4.0, 4.0), 2.0, nx - 2.0))
    py = float(np.clip(np.median(ky) + rng.uniform(-4.0, 4.0), 2.0, ny - 2.0))

    phi = float(rng.uniform(0.0, 2.0 * math.pi))
    c_phi, s_phi = math.cos(phi), math.sin(phi)
    zz, xx, yy = np.meshgrid(
        np.arange(nz, dtype=np.float32),
        np.arange(nx, dtype=np.float32),
        np.arange(ny, dtype=np.float32),
        indexing="ij",
    )
    h_proj = (xx - px) * c_phi + (yy - py) * s_phi
    z_rel = zz - pz

    if mode == "listric":
        dip0 = float(rng.uniform(math.radians(48.0), math.radians(78.0)))
        # Fault curves gently flatter with depth -> increasing horizontal offset component
        curve = rng.uniform(0.4, 1.1) / max(float(nz), 8.0)
        x_fault = -z_rel / math.tan(dip0) - curve * np.maximum(0.0, z_rel) ** 2
        signed_dist = h_proj - x_fault
        half = signed_dist > 0.0
        throw_z = float(rng.choice([-1.0, 1.0]) * rng.integers(1, max(2, int(max_throw_cells) + 1)))
        # Local dip direction controls heave/throw ratio (normal/reverse slip + gentle rollover drag)
        cot_loc = 1.0 / math.tan(dip0) + 2.0 * curve * np.maximum(0.0, z_rel)
        drag = np.exp(-np.abs(signed_dist) / 2.2) * (0.35 * throw_z)
        dz_off = np.where(half, throw_z, 0.0) - drag * np.where(half, -0.5, 0.5)
        dh_off = np.where(half, -throw_z * np.clip(cot_loc, 0.25, 1.2), 0.0)
    elif mode == "graben":
        dip = float(rng.uniform(math.radians(52.0), math.radians(76.0)))
        half_w = rng.uniform(0.09, 0.22) * nx
        cot_d = 1.0 / math.tan(dip)
        f1 = h_proj - (-half_w + z_rel * cot_d)
        f2 = h_proj - (half_w - z_rel * cot_d)
        if rng.random() < 0.50:
            # Asymmetric half-graben: single active listric bounding fault + gentle antithetic rollover
            side_sign = 1.0 if rng.random() < 0.5 else -1.0
            f_main = f1 if side_sign > 0 else -f2
            in_hg = f_main > 0.0
            throw_z = float(rng.choice([-1.0, 1.0]) * rng.integers(2, max(3, int(max_throw_cells) + 1)))
            taper = np.clip(1.0 - np.abs(f_main) / max(1.5 * half_w, 6.0), 0.15, 1.0)
            dz_off = np.where(in_hg, throw_z * taper, 0.0)
            dh_off = np.where(in_hg, -side_sign * throw_z * np.clip(cot_d, 0.25, 0.85) * 0.5 * taper, 0.0)
        else:
            # Symmetric two-wall graben/horst: vertical center throw + gentle outward footwall heave
            in_block = (f1 > 0.0) & (f2 < 0.0)
            throw_z = float(rng.choice([-1.0, 1.0]) * rng.integers(2, max(3, int(max_throw_cells) + 1)))
            dz_off = np.where(in_block, throw_z, 0.0)
            dh_off = np.where(
                f1 <= 0.0,
                -0.30 * throw_z * np.clip(cot_d, 0.2, 0.8),
                np.where(f2 >= 0.0, 0.30 * throw_z * np.clip(cot_d, 0.2, 0.8), 0.0),
            )
    else:
        dip = float(rng.uniform(math.radians(38.0), math.radians(82.0)))
        signed_dist = z_rel * math.cos(dip) + h_proj * math.sin(dip)
        half = signed_dist > 0.0
        throw_z = float(rng.choice([-1.0, 1.0]) * rng.integers(1, max(2, int(max_throw_cells) + 1)))
        heave = rng.uniform(0.25, 0.85) * throw_z
        dz_off = np.where(half, throw_z, 0.0)
        dh_off = np.where(half, heave, 0.0)

    # Pull-back coordinate lookup (inverse warp: out[p] = in[p - displacement])
    sz = np.rint(zz - dz_off).astype(np.int32)
    sx = np.rint(xx - dh_off * c_phi).astype(np.int32)
    sy = np.rint(yy - dh_off * s_phi).astype(np.int32)
    valid = (sz >= 0) & (sz < nz) & (sx >= 0) & (sx < nx) & (sy >= 0) & (sy < ny)
    sz_c = np.clip(sz, 0, nz - 1)
    sx_c = np.clip(sx, 0, nx - 1)
    sy_c = np.clip(sy, 0, ny - 1)

    out_rho = np.where(valid, rho[sz_c, sx_c, sy_c], 0.0).astype(np.float32)
    out_uid = np.where(valid, unit_id[sz_c, sx_c, sy_c], 0).astype(np.int32)
    above = zz < t_cells[None, :, :]
    out_rho[above] = 0.0
    out_uid[above] = 0

    if int((out_rho != 0).sum()) < max(6, int(0.35 * kz.size)):
        return rho, unit_id, False
    applied = not np.array_equal(out_rho, rho)
    return out_rho, out_uid, applied


def _place_secondary_anomaly_v6(rng, rho, unit_id, t_cells, nz, hs, vs, next_uid,
                                 min_g_ratio=0.05, border_margin=0):
    """Place 1-2 smaller secondary anomalies near/under a dominant primary target (SNR >= min_g_ratio)."""
    nx, ny = t_cells.shape
    nz_mask = rho != 0
    if not nz_mask.any():
        return next_uid
    blocked = np.zeros_like(nz_mask)
    _dilate_into_blocked(blocked, nz_mask, 1)
    main_g = 0.0
    for u in np.unique(unit_id[unit_id > 0]):
        um = unit_id == u
        main_g = max(main_g, _body_gravity_est(float(np.abs(rho[um]).mean()), um, t_cells))

    n_sec = 1 if rng.random() < 0.70 else 2
    placed, attempts = 0, 0
    while placed < n_sec and attempts < 40:
        attempts += 1
        rx = rng.uniform(1.6, 4.5) * hs
        ry = rng.uniform(1.5, 4.5) * hs
        rz = rng.uniform(1.0, 2.8) * vs
        th = rng.uniform(0.0, math.pi)
        dip = rng.uniform(-0.35, 0.35) if rng.random() < 0.35 else 0.0
        is_box = rng.random() >= 0.55
        reach_x, reach_y, z_ext = _primitive_reaches(rx, ry, rz, th, dip, is_box)
        mx = border_margin + (0.4 * reach_x if border_margin == 0 else reach_x)
        my = border_margin + (0.4 * reach_y if border_margin == 0 else reach_y)
        if 2.0 * mx >= nx or 2.0 * my >= ny:
            continue
        cx = rng.uniform(mx, nx - mx)
        cy = rng.uniform(my, ny - my)
        i0, i1 = int(max(0, cx - reach_x)), int(min(nx, cx + reach_x + 1))
        j0, j1 = int(max(0, cy - reach_y)), int(min(ny, cy + reach_y + 1))
        top_s = float(t_cells[i0:i1, j0:j1].max()) if i1 > i0 and j1 > j0 else float(t_cells.max())
        gap = rng.uniform(0.5, max(1.5, 7.5 * vs))
        cz = top_s + gap + z_ext
        if cz + z_ext > nz - 0.5:
            continue
        rho_val, _, _, is_cav = _pick_v6_contrast(rng, nz, gap, prefer_weak=False, allow_cavity=True)
        m = _rasterize_primitive(t_cells, nz, cx, cy, cz, rx, ry, rz, th, dip, is_box,
                                 cavity=is_cav, border_margin=border_margin)
        if m is None or m.sum() < max(6, int(10 * hs * hs * vs)) or (m & blocked).any():
            continue
        g_sec = _body_gravity_est(rho_val, m, t_cells)
        if main_g > 0.0 and g_sec < min_g_ratio * main_g:
            continue
        rho[m] = rho_val
        unit_id[m] = next_uid
        next_uid += 1
        _dilate_into_blocked(blocked, m, 1)
        placed += 1
    return next_uid


def _build_large_landfill_or_basin(rng, t_cells, nz, hs, vs, variant="landfill_plume"):
    """Build large-area & voluminous targets (25%-70%+ lateral footprint, 15%-50% volume, margin=0)."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), dtype=np.float32)
    unit_id = np.zeros((nz, nx, ny), dtype=np.int32)
    xx, yy = np.meshgrid(np.arange(nx, dtype=np.float32) + 0.5, np.arange(ny, dtype=np.float32) + 0.5, indexing="ij")
    zz = np.arange(nz, dtype=np.float32)[:, None, None] + 0.5

    subsurf_cells = np.maximum(0.0, nz - t_cells)
    total_sub_vox = float(subsurf_cells.sum())
    if variant == "landfill_plume":
        if rng.random() < 0.55:
            # Realistic shallow/mid-thickness extensive waste disposal sites
            target_area_frac = float(rng.uniform(0.42, 0.72))
            target_vol_frac = float(rng.uniform(0.135, 0.22))
            max_border_lim = 0.55
        else:
            target_area_frac = float(rng.uniform(0.27, 0.68))
            target_vol_frac = float(rng.uniform(0.22, 0.42))
            max_border_lim = 0.68 if rng.random() < 0.5 else 0.55
    else:
        target_area_frac = float(rng.uniform(0.27, 0.72))
        target_vol_frac = float(rng.uniform(0.16, 0.45))
        max_border_lim = 0.72

    # Build organic multi-lobe 2D lateral footprint with optional border contact (margin=0)
    fp_2d = np.zeros((nx, ny), dtype=bool)
    n_lobes = int(rng.integers(2, 5))
    scale_r = math.sqrt(target_area_frac / max(1, n_lobes)) * nx * rng.uniform(0.55, 0.85)
    for _ in range(n_lobes):
        cx = rng.uniform(0.10 * nx, 0.90 * nx)
        cy = rng.uniform(0.10 * ny, 0.90 * ny)
        rx = np.clip(scale_r * rng.uniform(0.65, 1.35), 4.0 * hs, 0.55 * nx)
        ry = np.clip(scale_r * rng.uniform(0.65, 1.35), 4.0 * hs, 0.55 * ny)
        th = rng.uniform(0.0, math.pi)
        ct, st = math.cos(th), math.sin(th)
        u = (xx - cx) * ct + (yy - cy) * st
        v = -(xx - cx) * st + (yy - cy) * ct
        if rng.random() < 0.45:
            lobe = (np.abs(u) <= rx) & (np.abs(v) <= ry)
        else:
            lobe = (u / rx) ** 2 + (v / ry) ** 2 <= 1.0
        fp_2d |= lobe

    # Organic boundary perturbation (quarry/plume margins & topographic trough preference)
    rough = gaussian_filter(rng.normal(size=(nx, ny)), max(1.5, 3.0 * hs), mode="reflect")
    rough = rough / (rough.std() + 1e-6)
    dist_map = gaussian_filter(fp_2d.astype(np.float32), max(1.2, 2.2 * hs), mode="nearest")
    score = dist_map + 0.14 * rough
    if variant == "landfill_plume" and t_cells.max() > t_cells.min() + 0.25 and rng.random() < 0.45:
        t_norm = (t_cells - t_cells.min()) / (t_cells.max() - t_cells.min() + 1e-6)
        score = score + 0.12 * t_norm  # valley/quarry-fill preference
    thresh = float(np.quantile(score, 1.0 - target_area_frac))
    fp_2d = score >= thresh
    fp_2d = _trim_border_for_detrend(rng, fp_2d, max_border_frac=max_border_lim)

    area_cnt = int(fp_2d.sum())
    if area_cnt < max(16, int(0.18 * nx * ny)):
        return rho, unit_id

    # Choose smoothly tapered vertical thickness respecting target_vol_frac and >=12% sub_vol contract
    avail_th = np.maximum(0.0, nz - 0.5 - t_cells)
    mean_avail = float(avail_th[fp_2d].mean())
    target_vox = max(target_vol_frac * total_sub_vox, 0.135 * total_sub_vox)
    mean_th = float(np.clip(target_vox / max(1, area_cnt), max(2.2, 3.0 * vs), max(3.5, 0.88 * mean_avail)))
    shape_w = np.clip((score - score[fp_2d].min()) / (score[fp_2d].max() - score[fp_2d].min() + 1e-6), 0.25, 1.25)
    shape_w = shape_w / max(0.25, float(shape_w[fp_2d].mean()))
    th_2d = np.where(fp_2d, np.clip(mean_th * shape_w, 2.0 * vs, avail_th), 0.0)

    is_outcrop = rng.random() < 0.55
    gap_2d = np.zeros((nx, ny), dtype=np.float32) if is_outcrop else np.full((nx, ny), rng.uniform(0.3, 2.2 * vs), dtype=np.float32)
    z_top_body = t_cells + gap_2d
    z_bot_body = np.minimum(nz - 0.3, z_top_body + th_2d)
    body_mask = fp_2d[None, :, :] & (zz >= z_top_body[None, :, :]) & (zz <= z_bot_body[None, :, :])
    if int(body_mask.sum()) < max(32, int(0.08 * nz * nx * ny)):
        return rho, unit_id

    next_uid = 1
    if variant == "landfill_plume":
        # Heterogeneous waste compaction / leachate plumes strictly inside [-0.85, -0.05]
        base_w = float(rng.uniform(-0.62, -0.25))
        field = _apply_density_field_v6(
            rng, body_mask, base_w, t_cells, RHO_LANDFILL_MIN, RHO_LANDFILL_MAX, mode="compaction"
        )
        # Add spatially correlated leachate / compaction cells
        clutter = gaussian_filter(rng.normal(size=(nz, nx, ny)).astype(np.float32),
                                  (max(0.8, 1.2 * vs), max(1.5, 2.5 * hs), max(1.5, 2.5 * hs)), mode="reflect")
        clutter = clutter / (clutter.std() + 1e-6) * float(rng.uniform(0.06, 0.15))
        field = np.where(body_mask, np.clip(field + clutter, RHO_LANDFILL_MIN, RHO_LANDFILL_MAX), 0.0).astype(np.float32)
        rho[body_mask] = field[body_mask]

        # Optional 2-3 lateral landfill cells (distinct unit_ids)
        n_cells = int(rng.choice([1, 2, 3], p=[0.40, 0.40, 0.20]))
        if n_cells > 1:
            ang = rng.uniform(0.0, math.pi)
            proj = xx * math.cos(ang) + yy * math.sin(ang)
            cuts = np.quantile(proj[fp_2d], np.linspace(0.0, 1.0, n_cells + 1)[1:-1])
            cell_id_2d = np.digitize(proj, cuts) + 1
            for ci in range(1, n_cells + 1):
                cm = body_mask & (cell_id_2d[None, :, :] == ci)
                if cm.any():
                    shift = float(rng.uniform(-0.14, 0.14))
                    rho[cm] = np.clip(rho[cm] + shift, RHO_LANDFILL_MIN, RHO_LANDFILL_MAX)
                    unit_id[cm] = next_uid
                    next_uid += 1
        else:
            unit_id[body_mask] = next_uid
            next_uid += 1

        # Optional thin clay/overburden cap on top 1-2 cells ([-0.45, -0.10])
        if rng.random() < 0.55 and th_2d[fp_2d].mean() >= 3.5:
            cap_th = float(rng.uniform(0.8, 1.8) * vs)
            cap_m = body_mask & (zz < (z_top_body[None, :, :] + cap_th))
            if int(cap_m.sum()) >= max(16, area_cnt // 2):
                rho[cap_m] = float(rng.uniform(-0.42, -0.10))
                unit_id[cap_m] = next_uid
                next_uid += 1
    else:
        # Broad shallow basin (negative compaction) OR regional pluton/batholith (positive + linear gradient)
        is_basin = rng.random() < 0.52
        if is_basin:
            base_r = float(rng.uniform(-0.68, -0.22))
            field = _apply_density_field_v6(
                rng, body_mask, base_r, t_cells, RHO_LANDFILL_MIN, RHO_LANDFILL_MAX, mode="compaction"
            )
            rho[body_mask] = field[body_mask]
            unit_id[body_mask] = next_uid
            next_uid += 1
        else:
            base_r = float(rng.uniform(0.18, 0.78))
            field = _apply_density_field_v6(
                rng, body_mask, base_r, t_cells, RHO_WEAK_MIN, 1.15, mode="linear"
            )
            rho[body_mask] = field[body_mask]
            unit_id[body_mask] = next_uid
            next_uid += 1
            # Optional localized dense roof pendant / ore keel inside batholith
            if rng.random() < 0.45:
                kx, ky = np.nonzero(fp_2d)
                cx_p = float(rng.choice(kx))
                cy_p = float(rng.choice(ky))
                rx_p, ry_p = rng.uniform(3.0, 7.0) * hs, rng.uniform(3.0, 7.0) * hs
                cz_p = float(z_top_body[int(cx_p), int(cy_p)] + rng.uniform(1.5, 4.5) * vs)
                rz_p = rng.uniform(1.8, 4.0) * vs
                pendant = _rasterize_primitive(t_cells, nz, cx_p, cy_p, cz_p, rx_p, ry_p, rz_p,
                                               rng.uniform(0.0, math.pi), 0.0, False, border_margin=0)
                if pendant is not None:
                    pend_in = pendant & body_mask
                    if int(pend_in.sum()) >= max(16, int(24 * hs * hs * vs)):
                        rho[pend_in] = float(rng.uniform(0.85, 1.65))
                        unit_id[pend_in] = next_uid
                        next_uid += 1

    # Add smaller secondary anomalies (~50% of large scenes, min_g_ratio=0.05)
    if rng.random() < 0.50:
        next_uid = _place_secondary_anomaly_v6(rng, rho, unit_id, t_cells, nz, hs, vs, next_uid,
                                               min_g_ratio=0.05, border_margin=0)
    return rho, unit_id


def _build_folded_horizons(rng, t_cells, nz, hs, vs):
    """Multi-layer plunging anticline/syncline strata truncated at terrain unconformity."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), dtype=np.float32)
    unit_id = np.zeros((nz, nx, ny), dtype=np.int32)
    zz, xx, yy = np.meshgrid(
        np.arange(nz, dtype=np.float32) + 0.5,
        np.arange(nx, dtype=np.float32) + 0.5,
        np.arange(ny, dtype=np.float32) + 0.5,
        indexing="ij",
    )
    th = rng.uniform(0.0, math.pi)
    ct, st = math.cos(th), math.sin(th)
    cx, cy = 0.5 * nx + rng.uniform(-0.12, 0.12) * nx, 0.5 * ny + rng.uniform(-0.12, 0.12) * ny
    u_across = (xx - cx) * ct + (yy - cy) * st
    v_along = -(xx - cx) * st + (yy - cy) * ct

    wl = max(8.0, rng.uniform(16.0, 34.0) * hs)
    fold_amp = rng.uniform(2.5, 6.0) * vs
    plunge_slope = rng.uniform(-0.22, 0.22) * (vs / max(hs, 1e-3))
    phase = rng.uniform(0.0, 2.0 * math.pi)
    z_fold = zz - fold_amp * np.sin(2.0 * math.pi * u_across / wl + phase) - plunge_slope * v_along

    # Window laterally when requested so we don't always span 100% perimeter
    use_window = rng.random() < 0.55
    if use_window:
        env = ((u_across / max(6.0, rng.uniform(12.0, 24.0) * hs)) ** 2
               + (v_along / max(6.0, rng.uniform(14.0, 26.0) * hs)) ** 2) <= 1.0
    else:
        env = _trim_border_for_detrend(rng, np.ones((nx, ny), dtype=bool), max_border_frac=0.68)[None, :, :]
        env = np.broadcast_to(env, (nz, nx, ny))

    n_layers = int(rng.choice([2, 3]))
    z_start = float(t_cells.mean()) + rng.uniform(1.0, 5.0) * vs
    next_uid = 1
    for li in range(n_layers):
        th_l = rng.uniform(2.2, 5.0) * vs
        z_end = z_start + th_l
        lm = env & (z_fold >= z_start) & (z_fold < z_end) & (zz >= t_cells[None, :, :])
        z_start = z_end
        if int(lm.sum()) < max(16, int(20 * hs * hs * vs)):
            continue
        r_val, lo, hi, _ = _pick_v6_contrast(rng, nz, max(1.0, z_start - float(t_cells.mean())), allow_cavity=False)
        if rng.random() < 0.45:
            field = _apply_density_field_v6(rng, lm, r_val, t_cells, lo, hi, mode="linear")
            rho[lm] = field[lm]
        else:
            rho[lm] = r_val
        unit_id[lm] = next_uid
        next_uid += 1
    return rho, unit_id


def _build_intrusive_plumbing(rng, t_cells, nz, hs, vs):
    """Connected feeder pipe, curved laccolith/sill, and radial dyke arms."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), dtype=np.float32)
    unit_id = np.zeros((nz, nx, ny), dtype=np.int32)
    cx = rng.uniform(0.28 * nx, 0.72 * nx)
    cy = rng.uniform(0.28 * ny, 0.72 * ny)
    top_ref = float(t_cells[int(np.clip(cx, 0, nx - 1)), int(np.clip(cy, 0, ny - 1))])

    # 1. Laccolith / domed sill body
    gap = rng.uniform(1.0, max(2.2, 5.0 * vs))
    rx1, ry1 = rng.uniform(5.5, 11.5) * hs, rng.uniform(4.5, 10.0) * hs
    rz1 = rng.uniform(1.8, 3.6) * vs
    th1 = rng.uniform(0.0, math.pi)
    cz1 = min(nz - rz1 - 4.0 * vs, top_ref + gap + rz1)
    if cz1 <= top_ref + 1.0:
        cz1 = top_ref + gap + rz1
    lac = _rasterize_primitive(t_cells, nz, cx, cy, cz1, rx1, ry1, rz1, th1,
                               rng.uniform(-0.20, 0.20), False, bend=rng.uniform(-0.2, 0.2), border_margin=0)
    if lac is None or int(lac.sum()) < max(16, int(24 * hs * hs * vs)):
        return rho, unit_id

    # 2. Steep feeder pipe below laccolith
    rx2, ry2 = rng.uniform(1.8, 3.4) * hs, rng.uniform(1.8, 3.4) * hs
    cz2 = min(nz - 1.5 * vs, cz1 + rz1 * 0.5 + rng.uniform(3.0, 6.0) * vs)
    rz2 = max(2.0 * vs, (nz - 1.0) - cz1) * 0.5
    pipe = _rasterize_primitive(t_cells, nz, cx + rng.uniform(-1.5, 1.5) * hs, cy + rng.uniform(-1.5, 1.5) * hs,
                                cz1 + rz2 * 0.75, rx2, ry2, rz2, th1, rng.uniform(-0.2, 0.2), False, border_margin=0)
    plume = lac if pipe is None else (lac | pipe)

    # 3. 1-2 radial dyke arms branching outward
    n_dykes = int(rng.choice([1, 2]))
    for _ in range(n_dykes):
        ang = rng.uniform(0.0, 2.0 * math.pi)
        len_d = rng.uniform(5.0, 11.0) * hs
        dx_d = math.cos(ang) * 0.55 * len_d
        dy_d = math.sin(ang) * 0.55 * len_d
        dyke = _rasterize_primitive(
            t_cells, nz, cx + dx_d, cy + dy_d, cz1 + rng.uniform(-0.5, 2.5) * vs,
            0.55 * len_d, rng.uniform(0.9, 1.9) * hs, rng.uniform(2.0, 4.5) * vs,
            ang, rng.uniform(-0.3, 0.3), True, border_margin=0,
        )
        if dyke is not None:
            plume = plume | dyke

    base_r = float(rng.uniform(0.28, 1.35))
    lo, hi = (0.15, 1.85)
    if rng.random() < 0.50:
        field = _apply_density_field_v6(rng, plume, base_r, t_cells, lo, hi, mode="linear")
        rho[plume] = field[plume]
    else:
        rho[plume] = base_r
    unit_id[plume] = 1
    return rho, unit_id


def _build_basement_cover_step(rng, t_cells, nz, hs, vs):
    """ColumnPoint-style tilted/faulted dense basement + overlying basin fill (with perimeter guard)."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), dtype=np.float32)
    unit_id = np.zeros((nz, nx, ny), dtype=np.int32)
    zz, xx, yy = np.meshgrid(
        np.arange(nz, dtype=np.float32) + 0.5,
        np.arange(nx, dtype=np.float32) + 0.5,
        np.arange(ny, dtype=np.float32) + 0.5,
        indexing="ij",
    )
    ang = rng.uniform(0.0, 2.0 * math.pi)
    s_coord = (xx - 0.5 * nx) * math.cos(ang) + (yy - 0.5 * ny) * math.sin(ang)
    # Faulted/stepped basement horizon rising shallow on one half of survey (truncated laterally to avoid DC slab)
    step_pos = rng.uniform(-0.18 * nx, 0.18 * nx)
    throw = rng.uniform(4.0, 10.0) * vs
    z_base_mean = float(t_cells.mean()) + rng.uniform(6.0, 11.0) * vs
    z_iface = z_base_mean - 0.5 * throw * np.tanh((s_coord[0] - step_pos) / max(1.5, 2.5 * hs)) + 0.08 * s_coord[0]
    z_iface = np.clip(z_iface, t_cells + 1.2 * vs, nz - 2.0 * vs)

    # Bound laterally inside ~45-70% of footprint so BorderPlanar keeps the gradient crisp
    fp_2d = _trim_border_for_detrend(
        rng,
        (((xx[0] - 0.5 * nx) / max(4.0, 0.44 * nx)) ** 2 + ((yy[0] - 0.5 * ny) / max(4.0, 0.44 * ny)) ** 2) <= 1.0,
        max_border_frac=0.68,
    )
    base_thick = rng.uniform(5.0, 10.0) * vs
    m_base = fp_2d[None, :, :] & (zz >= z_iface[None, :, :]) & (zz <= z_iface[None, :, :] + base_thick) & (zz < nz)
    m_cover = fp_2d[None, :, :] & (zz >= t_cells[None, :, :] + 0.5 * vs) & (zz < z_iface[None, :, :])
    if int(m_base.sum()) < 24:
        return rho, unit_id

    r_base = float(rng.uniform(0.22, 0.78))
    rho[m_base] = _apply_density_field_v6(rng, m_base, r_base, t_cells, 0.10, 0.95, mode="linear")[m_base]
    unit_id[m_base] = 1
    if int(m_cover.sum()) >= 24:
        r_cov = float(rng.uniform(-0.55, -0.10))
        rho[m_cover] = _apply_density_field_v6(rng, m_cover, r_cov, t_cells, -0.85, -0.05, mode="compaction")[m_cover]
        unit_id[m_cover] = 2
    return rho, unit_id


def _build_graded_halo_v6(rng, t_cells, nz, hs, vs):
    """Smooth continuous porphyry/alteration halo or two-unit graded envelope."""
    nx, ny = t_cells.shape
    rho = np.zeros((nz, nx, ny), dtype=np.float32)
    unit_id = np.zeros((nz, nx, ny), dtype=np.int32)
    bm = 0 if rng.random() < 0.30 else 3
    for _ in range(12):
        gap = rng.uniform(0.6, max(2.0, 5.5 * vs))
        rx = rng.uniform(5.0, 12.5) * hs
        ry = rng.uniform(4.5, 11.5) * hs
        rz = rng.uniform(2.5, 5.8) * vs
        th = rng.uniform(0.0, math.pi)
        dip = rng.uniform(-0.4, 0.4) if rng.random() < 0.4 else 0.0
        reach_x, reach_y, z_ext = _primitive_reaches(rx, ry, rz, th, dip, False)
        mx = bm + (0.3 * reach_x if bm == 0 else reach_x)
        my = bm + (0.3 * reach_y if bm == 0 else reach_y)
        if 2.0 * mx >= nx or 2.0 * my >= ny:
            continue
        cx, cy = rng.uniform(mx, nx - mx), rng.uniform(my, ny - my)
        i0, i1 = int(max(0, cx - reach_x)), int(min(nx, cx + reach_x + 1))
        j0, j1 = int(max(0, cy - reach_y)), int(min(ny, cy + reach_y + 1))
        top_ref = float(t_cells[i0:i1, j0:j1].max()) if i1 > i0 and j1 > j0 else float(t_cells.max())
        cz = top_ref + gap + z_ext
        if cz + z_ext > nz - 0.5:
            continue
        zz, xx, yy = np.meshgrid(np.arange(nz, dtype=np.float32) + 0.5,
                                 np.arange(nx, dtype=np.float32) + 0.5,
                                 np.arange(ny, dtype=np.float32) + 0.5, indexing="ij")
        ct, st = math.cos(th), math.sin(th)
        cd, sd = math.cos(dip), math.sin(dip)
        u0 = (xx - cx) * ct + (yy - cy) * st
        v = -(xx - cx) * st + (yy - cy) * ct
        w0 = zz - cz
        u = u0 * cd + w0 * sd
        w = -u0 * sd + w0 * cd
        r2 = (u / max(rx, 0.5)) ** 2 + (v / max(ry, 0.5)) ** 2 + (w / max(rz, 0.5)) ** 2
        mask = (r2 <= 1.0) & (zz >= t_cells[None, :, :] + 0.2)
        if bm > 0:
            mask[:, :bm, :] = False
            mask[:, -bm:, :] = False
            mask[:, :, :bm] = False
            mask[:, :, -bm:] = False
        if int(mask.sum()) < max(20, int(30 * hs * hs * vs)):
            continue
        if rng.random() < 0.55:
            # Smooth Gaussian porphyry core-to-margin fade (single unit_id=1 so l_grad preserves smoothness)
            r_outer = float(rng.uniform(0.10, 0.40))
            r_core = float(rng.uniform(0.75, 1.75))
            vals = r_outer + (r_core - r_outer) * np.exp(-2.8 * r2)
            rho[mask] = vals[mask].astype(np.float32)
            unit_id[mask] = 1
        else:
            # Negative weathered/leached outer skirt grading into light/sediment
            r_outer = float(rng.uniform(-0.55, -0.12))
            r_core = float(rng.uniform(-1.35, -0.65))
            vals = r_outer + (r_core - r_outer) * np.exp(-2.6 * r2)
            rho[mask] = vals[mask].astype(np.float32)
            unit_id[mask] = 1
        return rho, unit_id
    return rho, unit_id


def _assign_unit_ids_from_piecewise(rho):
    """Assign compact integer unit_ids per connected same-value lithology body (for legacy builders)."""
    unit_id = np.zeros(rho.shape, dtype=np.int32)
    next_uid = 1
    for v in np.unique(rho[rho != 0.0]):
        lbl, n_c = label(rho == v)
        for cid in range(1, n_c + 1):
            unit_id[lbl == cid] = next_uid
            next_uid += 1
    return unit_id


def random_bodies_v6(rng, t, nz, dz, dx, hard_prob=0.50, fault_prob=0.40,
                     large_prob=LARGE_BODY_PROB_V6, scene_type=None, return_meta=False):
    """Generate v6 geological model returning ``(rho, unit_id[, meta])`` below terrain ``t``.

    Covers primary waste-disposal/landfill/basin/pluton large footprints (~22%), 3D structural geology
    (plunging folds, continuous listric/graben faults, intrusive plumbing, basement steps, graded
    halos), refined multi-unit hard contacts (with ``border_margin=0`` & ``min_g_ratio=0.05``), and
    isolated bodies with realistic weak/moderate contrasts and balanced (~11%) cavity rate.
    """
    nx, ny = t.shape
    t_cells = t / dz
    hs = nx / 64.0
    vs = nz / 32.0
    can_struct = min(nx, ny) >= 24 and nz >= 12

    if scene_type is None:
        if not can_struct:
            scene_type = "isolated"
        else:
            u = rng.random()
            if u < large_prob:
                scene_type = "landfill_plume" if rng.random() < 0.58 else "broad_basin_pluton"
            else:
                # Rebalanced v6.1 structural & hard-archetype mix (~22% isolated at default hard_prob=0.50)
                u2 = (u - large_prob) / max(1e-6, 1.0 - large_prob)
                eff_struct = hard_prob if hard_prob < 0.30 else min(0.86, 1.44 * hard_prob)
                if u2 < max(0.20, eff_struct):
                    if rng.random() < 0.56:
                        scene_type = str(rng.choice(
                            ["listric_graben", "folded_horizons", "intrusive_plumbing", "basement_cover", "graded_halo"],
                            p=[0.25, 0.25, 0.16, 0.17, 0.17],
                        ))
                    else:
                        scene_type = str(rng.choice(HARD_SCENE_TYPES, p=[0.13, 0.13, 0.24, 0.25, 0.25]))
                else:
                    scene_type = "isolated"

    fault_applied = False
    if scene_type in ("landfill_plume", "broad_basin_pluton"):
        rho, unit_id = _build_large_landfill_or_basin(rng, t_cells, nz, hs, vs, variant=scene_type)
    elif scene_type == "folded_horizons":
        rho, unit_id = _build_folded_horizons(rng, t_cells, nz, hs, vs)
    elif scene_type == "listric_graben":
        rho, unit_id = _build_folded_horizons(rng, t_cells, nz, hs, vs) if rng.random() < 0.5 else (
            random_bodies(rng, t, nz, dz, dx, hard_prob=1.0, scene_type="abutting_contact", fault_prob=0.0),
            None,
        )
        if unit_id is None:
            unit_id = _assign_unit_ids_from_piecewise(rho)
        fmode = "listric" if rng.random() < 0.55 else "graben"
        rho, unit_id, fault_applied = apply_continuous_fault_v6(rng, rho, unit_id, t_cells, mode=fmode)
    elif scene_type == "intrusive_plumbing":
        rho, unit_id = _build_intrusive_plumbing(rng, t_cells, nz, hs, vs)
    elif scene_type == "basement_cover":
        rho, unit_id = _build_basement_cover_step(rng, t_cells, nz, hs, vs)
    elif scene_type == "graded_halo":
        rho, unit_id = _build_graded_halo_v6(rng, t_cells, nz, hs, vs)
    elif scene_type in HARD_SCENE_TYPES:
        rho = random_bodies(rng, t, nz, dz, dx, hard_prob=1.0, scene_type=scene_type, fault_prob=0.0)
        unit_id = _assign_unit_ids_from_piecewise(rho)
        # Optionally apply linear Mod3D density gradient inside ~30% of non-cavity hard rock units
        if rng.random() < 0.30:
            for uid in np.unique(unit_id[unit_id > 0]):
                um = unit_id == uid
                mean_r = float(rho[um].mean())
                if mean_r > -1.60:
                    lo_r, hi_r = (-1.60, -0.05) if mean_r < 0 else (0.05, 1.90)
                    rho[um] = _apply_density_field_v6(rng, um, mean_r, t_cells, lo_r, hi_r, mode="linear")[um]
    else:
        # v6 isolated multi-body: lower cavity rate (~11%), border_margin=0 (25%), min_g_ratio=0.05 (35%), weak contrasts
        rho = np.zeros((nz, nx, ny), dtype=np.float32)
        unit_id = np.zeros((nz, nx, ny), dtype=np.int32)
        n_b = 1 if rng.random() < 0.35 else int(rng.integers(2, 5))
        bm = 0 if rng.random() < 0.25 else 3
        min_gr = 0.05 if rng.random() < 0.40 else 0.14
        blocked = np.zeros((nz, nx, ny), dtype=bool)
        max_g = 0.0
        next_uid = 1
        for _ in range(80):
            if next_uid - 1 >= n_b:
                break
            gap = rng.uniform(0.5, max(1.2, 0.48 * nz if rng.random() > 0.35 else 4.2 * vs))
            rho_val, lo_r, hi_r, is_cav = _pick_v6_contrast(rng, nz, gap, allow_cavity=(rng.random() < 0.50))
            if is_cav:
                rx, ry = rng.uniform(1.2, 3.6, 2) * hs
                rz = rng.uniform(0.9, 2.5) * vs
                dip, is_box = 0.0, (rng.random() < 0.25)
            elif rng.random() < 0.22:
                # Steep tabular dyke / sheet (50-82 deg dip)
                rx = rng.uniform(1.0, 2.2) * hs
                ry = rng.uniform(4.2, 9.0) * hs
                rz = rng.uniform(2.5, 5.5) * vs
                dip = float(rng.choice([-1.0, 1.0]) * rng.uniform(math.radians(50.0), math.radians(82.0)))
                is_box = True
            else:
                rx, ry = rng.uniform(1.8, 9.0, 2) * hs
                rz = rng.uniform(1.0, 5.2) * vs
                dip = float(rng.uniform(-0.45, 0.45)) if rng.random() < 0.38 else 0.0
                is_box = rng.random() >= 0.52
            th = rng.uniform(0.0, math.pi)
            reach_x, reach_y, z_ext = _primitive_reaches(rx, ry, rz, th, dip, is_box)
            mx = bm + (0.3 * reach_x if bm == 0 else reach_x)
            my = bm + (0.3 * reach_y if bm == 0 else reach_y)
            if 2.0 * mx >= nx or 2.0 * my >= ny:
                continue
            cx, cy = rng.uniform(mx, nx - mx), rng.uniform(my, ny - my)
            i0, i1 = int(max(0, cx - reach_x)), int(min(nx, cx + reach_x + 1))
            j0, j1 = int(max(0, cy - reach_y)), int(min(ny, cy + reach_y + 1))
            top_s = float(t_cells[i0:i1, j0:j1].max()) if i1 > i0 and j1 > j0 else float(t_cells.max())
            cz = top_s + gap + z_ext
            if cz + z_ext > nz - 0.5:
                continue
            g_est = abs(rho_val) * ((4.0 / 3.0) * math.pi * rx * ry * rz) / max(1.0, gap + z_ext) ** 2
            if max_g > 0.0 and g_est < min_gr * max_g:
                continue
            m = _rasterize_primitive(t_cells, nz, cx, cy, cz, rx, ry, rz, th, dip, is_box,
                                     cavity=is_cav, border_margin=bm)
            if m is None or (m & blocked).any():
                continue
            if not is_cav and rng.random() < 0.30:
                rho[m] = _apply_density_field_v6(rng, m, rho_val, t_cells, lo_r, hi_r, mode="linear")[m]
            else:
                rho[m] = rho_val
            unit_id[m] = next_uid
            next_uid += 1
            _dilate_into_blocked(blocked, m, 2)
            max_g = max(max_g, _body_gravity_est(abs(rho_val), m, t_cells))

    # Fallback guarantee if geometric checks yielded empty volume
    if not (rho != 0).any():
        scene_type = "isolated"
        rho = random_bodies(rng, t, nz, dz, dx, hard_prob=0.0, fault_prob=0.0)
        unit_id = _assign_unit_ids_from_piecewise(rho)

    # Optional continuous fault deformation (no integer-shift tears) across non-faulted scenes
    if fault_prob > 0.0 and scene_type not in ("listric_graben",) and rng.random() < fault_prob:
        fmode = str(rng.choice(["planar", "listric", "graben"], p=[0.50, 0.30, 0.20]))
        rho, unit_id, fault_applied = apply_continuous_fault_v6(rng, rho, unit_id, t_cells, mode=fmode)

    rho, unit_id = _finalize_units_v6(rho, unit_id, t, dz)
    if return_meta:
        return rho, unit_id, {"scene_type": scene_type, "fault_applied": bool(fault_applied)}
    return rho, unit_id


def _guard_airborne_snr_v6(rng, rho, t, clr_map, dx, dz, keep_blurred_prob=0.10):
    """Scale airborne clearance down ONLY if signal falls below physical detection SNR (no type leakage!)."""
    med_clr = float(np.median(clr_map))
    if med_clr <= 2.0:
        return clr_map
    kz, kx, ky = np.nonzero(rho)
    if kz.size == 0:
        return clr_map
    z_top_rel = np.maximum(0.5, (kz * dz - t[kx, ky]))
    # Compute true ground signal proxy & per-component horizontal half-width (avoids multi-body span inflation)
    g_ground = float(np.sum(np.abs(rho[kz, kx, ky]) / np.maximum(dx, z_top_rel + 1.0) ** 2))
    amp_fac = float(np.clip(g_ground * dx * dx / 0.18, 0.25, 2.5))
    z_eff_med = float(np.quantile(z_top_rel, 0.25)) + 0.5 * dz
    lbl, n_c = label(rho != 0)
    comp_ws = []
    for cid in range(1, min(n_c, 16) + 1):
        _, ckx, cky = np.nonzero(lbl == cid)
        if ckx.size >= 4:
            comp_ws.append(max(dx, 2.0 * float(np.hypot(ckx.std(), cky.std()) + 0.5) * dx))
    w_est = float(np.median(comp_ws)) if comp_ws else max(dx, 2.0 * float(np.hypot(kx.std(), ky.std()) + 0.5) * dx)
    att = (z_eff_med + w_est) / max(1.0, z_eff_med + w_est + med_clr)
    snr_proxy = float((att ** 2) * amp_fac / 0.04)  # relative to ~4% host/noise floor
    # Only rescale if completely drowned AND not kept in the ~10% resolution-blur bucket for log_var
    if snr_proxy < 4.0 and rng.random() >= keep_blurred_prob:
        # Target at least SNR ~ 4.0 by scaling excess clearance above 2 m proportionally
        need_att = math.sqrt(min(1.0, 4.0 * 0.04 / amp_fac))
        max_safe_clr = max(2.0, (z_eff_med + w_est) * (1.0 / max(need_att, 1e-3) - 1.0))
        if med_clr > max_safe_clr:
            scale = (max_safe_clr - 1.5) / max(med_clr - 1.5, 1e-3)
            clr_map = np.clip(1.5 + (clr_map - 1.5) * scale, 1.5, CLEARANCE_HIGH_DRAPE_MAX_V6).astype(np.float32)
    return clr_map


def _sample_clearance_map_v6(rng, rho, t, dx, dz, ground_prob=GROUND_PROB_V6, max_airborne=CLEARANCE_AIRBORNE_V6[1]):
    """Sample 2D spatial clearance map [m] above terrain: ~67% primary ground, ~33% relief-drape airborne."""
    nx, ny = t.shape
    hs = nx / 64.0
    if rng.random() < ground_prob or max_airborne <= 2.0:
        # Primary Ground Survey: [0.30, 2.0 m], mostly crisp [0.30, 0.45 m] tripod height
        if rng.random() < 0.80:
            base = rng.uniform(0.30, 0.46)
            jit_amp = 0.08
        else:
            base = rng.uniform(0.45, 1.85)
            jit_amp = 0.18
        if rng.random() < 0.30:
            jit = gaussian_filter(rng.normal(size=(nx, ny)), max(2.0, 6.0 * hs), mode="reflect")
            jit = jit / (jit.std() + 1e-6) * jit_amp
            clr_map = np.clip(base + jit, 0.30, 2.0).astype(np.float32)
        else:
            clr_map = np.full((nx, ny), base, dtype=np.float32)
        return clr_map

    # Secondary Airborne / Height-Grid (~33%): mostly relief-following smooth drape + ~25% FlightElevation/HighDrape
    u_air = rng.random()
    if u_air < 0.75:
        # Relief-following smooth drape: 2 - min(80, max_airborne) m AGL (bridges ultra-low drone band)
        hi_agl = min(80.0, float(max_airborne))
        h_agl = math.exp(rng.uniform(math.log(2.0), math.log(max(2.5, hi_agl))))
        t_sm = gaussian_filter(t, rng.uniform(3.5, 9.0) * max(1.0, hs), mode="reflect")
        lag = rng.uniform(0.35, 0.90)
        clr_map = np.clip(h_agl + lag * (t - t_sm), 2.0, min(160.0, CLEARANCE_HIGH_DRAPE_MAX_V6)).astype(np.float32)
    else:
        # High drape (80-160 m) OR partial FlightElevation / ElevationGrid plane mismatch (<=215 m)
        if rng.random() < 0.50:
            hi_agl = min(160.0, float(max_airborne))
            lo_agl = min(80.0, 0.6 * hi_agl)
            h_agl = rng.uniform(lo_agl, max(lo_agl + 1.0, hi_agl))
            t_sm = gaussian_filter(t, rng.uniform(4.0, 10.0) * max(1.0, hs), mode="reflect")
            clr_map = np.clip(h_agl + 0.5 * (t - t_sm), 2.0, min(180.0, float(max_airborne))).astype(np.float32)
        else:
            xg, yg = np.meshgrid(np.linspace(-1.0, 1.0, nx), np.linspace(-1.0, 1.0, ny), indexing="ij")
            h_ridge = rng.uniform(8.0, min(65.0, float(max_airborne)))
            beta = rng.uniform(0.45, 1.0)
            tilt = rng.uniform(-12.0, 12.0) * xg + rng.uniform(-12.0, 12.0) * yg
            clr_map = np.clip(h_ridge + beta * (t - float(t.min())) + tilt, 2.0, CLEARANCE_HIGH_DRAPE_MAX_V6).astype(np.float32)

    return _guard_airborne_snr_v6(rng, rho, t, clr_map, dx, dz, keep_blurred_prob=0.10)


def sample_geometry_v6(rng, nx=NX, ny=NY, nz=NZ, dx=CELL, dy=CELL, dz=CELL,
                       hard_prob=0.50, fault_prob=0.40, large_prob=LARGE_BODY_PROB_V6,
                       ground_prob=GROUND_PROB_V6, max_airborne=CLEARANCE_AIRBORNE_V6[1],
                       scene_type=None):
    """Draw one v6 geometry + 2D spatial clearance sample ``(rho, unit_id, t, clearance_map, noise_frac)``."""
    t = random_terrain(rng, nx, ny, nz * dz)
    rho, unit_id, _ = random_bodies_v6(
        rng, t, nz, dz, dx,
        hard_prob=hard_prob, fault_prob=fault_prob, large_prob=large_prob,
        scene_type=scene_type, return_meta=True,
    )
    clr_map = _sample_clearance_map_v6(rng, rho, t, dx, dz, ground_prob=ground_prob, max_airborne=max_airborne)
    noise_frac = np.float32(rng.uniform(*NOISE_FRAC))
    return rho, unit_id, t, clr_map, noise_frac


def compute_max_resolvable_clearance(rho, t_cells, dx, dz, scene_type="isolated",
                                     fault_applied=False, clearance_range=CLEARANCE):
    """Cap sensor clearance when high-frequency targets cannot be resolved through upward continuation."""
    lo, hi = float(clearance_range[0]), float(clearance_range[1])
    high_freq_max = 2.0
    if hi <= high_freq_max:
        return (lo, hi)
    cap = (lo, min(hi, high_freq_max))
    if fault_applied or scene_type in ("tight_cluster", "nested_halo", "abutting_contact", "vertical_stack"):
        return cap
    if not (rho != 0).any() or (rho <= -1.60).any():
        return cap

    nz, nx, ny = rho.shape
    hs, vs = nx / 64.0, nz / 32.0
    w_min_m = float("inf")
    z_top_m = float("inf")
    boxes = []
    for v in np.unique(rho[rho != 0]):
        lbl, n_c = label(rho == v)
        for cid in range(1, n_c + 1):
            kz, kx, ky = np.nonzero(lbl == cid)
            if kz.size < 4:
                continue
            span_x = int(kx.max() - kx.min() + 1)
            span_y = int(ky.max() - ky.min() + 1)
            span_z = int(kz.max() - kz.min() + 1)
            w_min_cells = min(span_x, span_y)
            if w_min_cells < 3.5 * hs or span_z < 1.5 * vs:
                return cap
            w_min_m = min(w_min_m, w_min_cells * float(dx))
            z_top_cells = max(0.0, float((kz - t_cells[kx, ky]).min()))
            z_top_m = min(z_top_m, z_top_cells * float(dz))
            boxes.append((int(kx.min()), int(kx.max()), int(ky.min()), int(ky.max())))

    if not boxes:
        return cap
    for idx_a in range(len(boxes)):
        x0, x1, y0, y1 = boxes[idx_a]
        for idx_b in range(idx_a + 1, len(boxes)):
            a0, a1, b0, b1 = boxes[idx_b]
            gap_x = max(0, a0 - x1 - 1, x0 - a1 - 1)
            gap_y = max(0, b0 - y1 - 1, y0 - b1 - 1)
            if math.hypot(gap_x, gap_y) < 3.0 * hs:
                return cap

    h_phys = float(np.clip(0.32 * w_min_m - 0.25 * z_top_m, high_freq_max, hi))
    return (lo, min(hi, h_phys))


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
    rho, meta = random_bodies(rng, t, nz, dz, dx, hard_prob=hard_prob,
                              fault_prob=fault_prob, return_meta=True)
    eff_range = compute_max_resolvable_clearance(
        rho, t / dz, dx, dz,
        scene_type=meta["scene_type"],
        fault_applied=meta["fault_applied"],
        clearance_range=clearance_range,
    )
    clearance = _sample_clearance(rng, eff_range)
    noise_frac = np.float32(rng.uniform(*NOISE_FRAC))
    return rho, t, clearance, noise_frac


class BodyTerrainDataset(Dataset):
    """Yields geometry only: ``(rho (1,nz,nx,ny), t (nx,ny), clearance, noise_frac, noise_seed)``
    (or v6 ``(rho, unit_id, t, clearance_map, noise_frac, noise_seed)`` when ``v6_mode=True``;
    appends ``aspect_idx`` when ``aspect_ratios`` is provided).

    ``seed=None`` -> fresh random sample on every access (infinite training data);
    ``seed=int``  -> sample ``idx`` is deterministic (validation).
    """

    def __init__(self, n_samples=2000, nx=NX, ny=NY, nz=NZ, dx=CELL, dy=CELL, dz=CELL,
                 seed=None, hard_prob=0.50, fault_prob=0.0, clearance_range=CLEARANCE,
                 aspect_ratios=None, v6_mode=False, large_prob=LARGE_BODY_PROB_V6,
                 ground_prob=GROUND_PROB_V6, max_airborne=CLEARANCE_AIRBORNE_V6[1]):
        self.n, self.seed, self.hard_prob = n_samples, seed, float(hard_prob)
        self.fault_prob = float(fault_prob)
        self.clearance_range = tuple(clearance_range)
        self.aspect_ratios = tuple(float(a) for a in aspect_ratios) if aspect_ratios else None
        self.v6_mode = bool(v6_mode)
        self.large_prob = float(large_prob)
        self.ground_prob = float(ground_prob)
        self.max_airborne = float(max_airborne)
        self.g = (nx, ny, nz, dx, dy, dz)
        self._curr_state = torch.tensor(
            [self.hard_prob, self.fault_prob, float(self.clearance_range[1]),
             self.large_prob, self.ground_prob, self.max_airborne],
            dtype=torch.float64,
        )
        try:
            self._curr_state.share_memory_()
        except Exception:
            pass

    def set_curriculum(self, hard_prob=None, fault_prob=None, clearance_max=None,
                       large_prob=None, ground_prob=None, max_airborne=None):
        """Update sampling difficulty bounds across curriculum epochs (visible to persistent workers)."""
        if hard_prob is not None:
            self.hard_prob = float(hard_prob)
        if fault_prob is not None:
            self.fault_prob = float(fault_prob)
        if clearance_max is not None:
            lo = float(self.clearance_range[0])
            self.clearance_range = (lo, max(lo, float(clearance_max)))
            if max_airborne is None:
                self.max_airborne = max(float(CLEARANCE[0]), float(clearance_max))
        if large_prob is not None:
            self.large_prob = float(large_prob)
        if ground_prob is not None:
            self.ground_prob = float(ground_prob)
        if max_airborne is not None:
            self.max_airborne = float(max_airborne)
        self._curr_state[0] = self.hard_prob
        self._curr_state[1] = self.fault_prob
        self._curr_state[2] = float(self.clearance_range[1])
        self._curr_state[3] = self.large_prob
        self._curr_state[4] = self.ground_prob
        self._curr_state[5] = self.max_airborne

    def __len__(self):
        return self.n

    def __getitem__(self, idx):
        rng = np.random.default_rng() if self.seed is None else np.random.default_rng([self.seed, idx])
        nx, ny, nz, dx, dy, dz = self.g
        cur_hard, cur_fault, cur_cmax, cur_large, cur_ground, cur_air_max = (
            float(self._curr_state[k].item()) for k in range(6)
        )
        cur_clr = (float(self.clearance_range[0]), max(float(self.clearance_range[0]), cur_cmax))
        aspect_idx = 0
        dz_cur = dz
        if self.aspect_ratios is not None:
            aspect_idx = int(rng.integers(0, len(self.aspect_ratios)))
            dz_cur = dx * self.aspect_ratios[aspect_idx]
        if self.v6_mode:
            rho, unit_id, t, clr_map, nf = sample_geometry_v6(
                rng, nx, ny, nz, dx, dy, dz_cur,
                hard_prob=cur_hard, fault_prob=cur_fault,
                large_prob=cur_large, ground_prob=cur_ground,
                max_airborne=cur_air_max,
            )
            noise_seed = int(rng.integers(0, 2**31 - 1))
            items = (
                torch.from_numpy(rho)[None],
                torch.from_numpy(unit_id)[None],
                torch.from_numpy(t),
                torch.from_numpy(clr_map),
                torch.tensor(nf),
                torch.tensor(noise_seed),
            )
        else:
            rho, t, c, nf = sample_geometry(
                rng, nx, ny, nz, dx, dy, dz_cur,
                hard_prob=cur_hard, fault_prob=cur_fault,
                clearance_range=cur_clr,
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


def _clearance_to_map(clearance, nx: int, ny: int, b: int, device, dtype):
    """Broadcast scalar, ``(B,)``, ``(B, H, W)``, or ``(B, 1, H, W)`` clearance into ``(B, H, W)``."""
    if isinstance(clearance, torch.Tensor):
        c = clearance.to(device=device, dtype=dtype)
        if c.ndim == 0:
            return c.view(1, 1, 1).expand(b, nx, ny)
        if c.ndim == 1:
            return c.view(b, 1, 1).expand(b, nx, ny)
        if c.ndim == 2:
            if c.shape == (nx, ny) and b == 1:
                return c.unsqueeze(0)
            if c.shape == (b, 1):
                return c.view(b, 1, 1).expand(b, nx, ny)
            raise ValueError(f"Incompatible 2D clearance shape {tuple(c.shape)} for batch={b}, grid=({nx},{ny})")
        if c.ndim == 3:
            if c.shape == (b, nx, ny):
                return c
            if c.shape == (b, 1, 1):
                return c.expand(b, nx, ny)
        if c.ndim == 4 and c.shape[0] == b and c.shape[1] == 1:
            return c[:, 0].expand(b, nx, ny)
        raise ValueError(f"Unsupported clearance shape {tuple(c.shape)}")
    return torch.full((b, nx, ny), float(clearance), device=device, dtype=dtype)


_PLANE_PROJ_CACHE: dict[tuple, tuple[torch.Tensor, torch.Tensor, torch.Tensor | None]] = {}


def _get_plane_projector(nx: int, ny: int, mode: str, device, dtype):
    key = (int(nx), int(ny), mode, str(device), str(dtype))
    cached = _PLANE_PROJ_CACHE.get(key)
    if cached is not None:
        return cached
    if mode in ("cpp_border_proj", "cpp_border_eval"):
        cx = torch.arange(nx, device="cpu", dtype=torch.float64) + 0.5 - 0.5 * nx
        cy = torch.arange(ny, device="cpu", dtype=torch.float64) + 0.5 - 0.5 * ny
        gx, gy = torch.meshgrid(cx, cy, indexing="ij")
        x_all = torch.stack([torch.ones_like(gx), gx, gy], dim=-1).reshape(nx * ny, 3)
        bmask = torch.zeros((nx, ny), device="cpu", dtype=torch.bool)
        if nx <= 2 or ny <= 2:
            bmask.fill_(True)
        else:
            bmask[0, :] = True
            bmask[-1, :] = True
            bmask[:, 0] = True
            bmask[:, -1] = True
        w_perim = torch.where(bmask, (gx * gx + gy * gy).clamp_min(1e-12), torch.zeros_like(gx))
        w_perim = w_perim / w_perim.sum().clamp_min(1e-12)
        sx2 = (w_perim * gx * gx).sum().clamp_min(1e-12)
        sy2 = (w_perim * gy * gy).sum().clamp_min(1e-12)
        est_all = torch.stack([
            w_perim.reshape(-1),
            (w_perim * gx).reshape(-1) / sx2,
            (w_perim * gy).reshape(-1) / sy2,
        ], dim=1)
        idx = torch.nonzero(bmask.reshape(-1), as_tuple=False).squeeze(1)
        pinv_T = est_all[idx].to(device=device, dtype=dtype).contiguous()
        x_all_T = x_all.T.to(device=device, dtype=dtype).contiguous()
        idx = idx.to(device=device)
        _PLANE_PROJ_CACHE[key] = (pinv_T, x_all_T, idx)
        return pinv_T, x_all_T, idx
    xs = torch.linspace(-1.0, 1.0, nx, device="cpu", dtype=torch.float64)
    ys = torch.linspace(-1.0, 1.0, ny, device="cpu", dtype=torch.float64)
    gx, gy = torch.meshgrid(xs, ys, indexing="ij")
    x_all = torch.stack([torch.ones_like(gx), gx, gy], dim=-1).reshape(nx * ny, 3)  # (N, 3)
    if mode == "border_planar":
        bmask = torch.zeros((nx, ny), device="cpu", dtype=torch.bool)
        bmask[0, :] = True
        bmask[-1, :] = True
        bmask[:, 0] = True
        bmask[:, -1] = True
        idx = torch.nonzero(bmask.reshape(-1), as_tuple=False).squeeze(1)
        x_sub = x_all[idx]
        idx = idx.to(device=device)
    else:
        idx = None
        x_sub = x_all
    pinv_T = torch.linalg.pinv(x_sub).T.to(device=device, dtype=dtype).contiguous()  # (M, 3)
    x_all_T = x_all.T.to(device=device, dtype=dtype).contiguous()                    # (3, N)
    _PLANE_PROJ_CACHE[key] = (pinv_T, x_all_T, idx)
    return pinv_T, x_all_T, idx


def _normalize_detrend_mode(mode: str) -> str:
    m = str(mode).strip().lower()
    if m in ("raw", "none", ""):
        return "raw"
    if m in ("constant", "constant_shift", "constantshift"):
        return "constant"
    if m in ("planar", "planar_ramp", "planarramp"):
        return "planar"
    if m in ("border_planar", "borderplanar"):
        return "border_planar"
    if m in ("cpp_border_proj", "cpp_border_eval"):
        return m
    raise ValueError(f"Unknown detrend mode: {mode!r}")


def apply_mod3d_detrend(obs: torch.Tensor, modes) -> torch.Tensor:
    """Apply Mod3D-exact regional detrending (``BorderPlanar``, ``PlanarRamp``, ``cpp_border_*``, ``ConstantShift``, ``raw``).

    ``obs`` has shape ``(B, 1, H, W)`` (or ``(B, H, W)``). ``modes`` is a single mode string or
    sequence of length ``B``.
    """
    squeeze_ch = obs.ndim == 3
    x = obs.unsqueeze(1) if squeeze_ch else obs
    b, _, nx, ny = x.shape
    if isinstance(modes, str):
        norm_modes = [_normalize_detrend_mode(modes)] * b
    else:
        norm_modes = [_normalize_detrend_mode(m) for m in modes]
        if len(norm_modes) != b:
            raise ValueError(f"Expected {b} detrend modes, got {len(norm_modes)}")

    out = x.clone()
    flat = out.reshape(b, nx * ny)
    for target_mode in ("constant", "planar", "border_planar", "cpp_border_proj", "cpp_border_eval"):
        idxs = [i for i, m in enumerate(norm_modes) if m == target_mode]
        if not idxs:
            continue
        idx_t = torch.tensor(idxs, device=x.device, dtype=torch.long)
        sub = flat.index_select(0, idx_t)
        if target_mode == "constant":
            trend = sub.mean(dim=1, keepdim=True)
            flat.index_copy_(0, idx_t, sub - trend)
        else:
            pinv_T, x_all_T, b_idx = _get_plane_projector(nx, ny, target_mode, x.device, x.dtype)
            pts = sub if b_idx is None else sub.index_select(1, b_idx)
            coeffs = pts @ pinv_T            # (G, 3)
            trend = coeffs @ x_all_T         # (G, nx*ny)
            rem = sub - trend
            if target_mode == "cpp_border_eval":
                rem = rem - rem.mean(dim=1, keepdim=True)
            flat.index_copy_(0, idx_t, rem)
    return out[:, 0] if squeeze_ch else out


def align_pred_field_to_obs_detrend(pred_field: torch.Tensor, obs_field: torch.Tensor, detrend_modes) -> torch.Tensor:
    """Apply the matching detrend operator to ``pred_field`` so it lies in the same basis as ``obs_field``."""
    if detrend_modes is None:
        return pred_field
    squeeze_ch = pred_field.ndim == 3
    p = pred_field.unsqueeze(1) if squeeze_ch else pred_field
    o = obs_field.unsqueeze(1) if obs_field.ndim == 3 else obs_field
    b = p.shape[0]
    modes_in = [detrend_modes] * b if isinstance(detrend_modes, str) else list(detrend_modes)
    if len(modes_in) != b:
        raise ValueError(f"Expected {b} detrend modes, got {len(modes_in)}")
    exec_modes = []
    for i, m in enumerate(modes_in):
        nm = _normalize_detrend_mode(m)
        if nm == "planar":
            exec_modes.append("cpp_border_eval")
        elif nm == "border_planar":
            oi = o[i, 0]
            scale = oi.abs().max().clamp_min(1e-4)
            exec_modes.append("cpp_border_eval" if float(oi.mean().abs()) < 1e-3 * float(scale) else "border_planar")
        else:
            exec_modes.append(nm)
    aligned = apply_mod3d_detrend(p, exec_modes)
    return aligned[:, 0] if squeeze_ch else aligned


def _apply_oblique_line_mask(vm: torch.Tensor, rng_np) -> None:
    nx, ny = vm.shape
    theta = float(_rand_uniform(rng_np, 0.25, math.pi - 0.25, (1,), vm.device, vm.dtype).item())
    step = float(_rand_uniform(rng_np, 3.0, 6.0, (1,), vm.device, vm.dtype).item())
    width = float(_rand_uniform(rng_np, 0.9, 1.35, (1,), vm.device, vm.dtype).item())
    phase = float(_rand_uniform(rng_np, 0.0, step, (1,), vm.device, vm.dtype).item())
    xs = torch.arange(nx, device=vm.device, dtype=vm.dtype)[:, None]
    ys = torch.arange(ny, device=vm.device, dtype=vm.dtype)[None, :]
    proj = xs * math.cos(theta) + ys * math.sin(theta) + phase
    mod = torch.remainder(proj, step)
    keep = mod >= width
    vm.mul_(keep.to(vm.dtype))


def _apply_boundary_void_mask(vm: torch.Tensor, rng_np) -> None:
    nx, ny = vm.shape
    theta = float(_rand_uniform(rng_np, 0.0, 2.0 * math.pi, (1,), vm.device, vm.dtype).item())
    xs = torch.linspace(-1.0, 1.0, nx, device=vm.device, dtype=vm.dtype)[:, None]
    ys = torch.linspace(-1.0, 1.0, ny, device=vm.device, dtype=vm.dtype)[None, :]
    proj = xs * math.cos(theta) + ys * math.sin(theta)
    thresh = float(_rand_uniform(rng_np, 0.45, 0.85, (1,), vm.device, vm.dtype).item())
    # Optionally add an orthogonal clip corner for irregular polygon outlines
    if float(_rand_uniform(rng_np, 0.0, 1.0, (1,), vm.device, vm.dtype).item()) < 0.55:
        proj2 = -xs * math.sin(theta) + ys * math.cos(theta)
        void = (proj > thresh) | ((proj > 0.25) & (proj2 > thresh + 0.10))
    else:
        void = proj > thresh
    vm.masked_fill_(void, 0.0)


@torch.no_grad()
def _add_exterior_offgrid_mass(obs_vox: torch.Tensor, peak: torch.Tensor, t: torch.Tensor,
                               z_obs: torch.Tensor, dx: float, dy: float, dz: float,
                               prob: float, rng_list, single_rng) -> None:
    """Perturb ``obs_vox`` ONLY with harmonic gravity fields of exterior off-grid sources."""
    b, _, nx, ny = obs_vox.shape
    lx, ly = nx * float(dx), ny * float(dy)
    xs = (torch.arange(nx, device=obs_vox.device, dtype=obs_vox.dtype) + 0.5) * float(dx)
    ys = (torch.arange(ny, device=obs_vox.device, dtype=obs_vox.dtype) + 0.5) * float(dy)
    gx, gy = torch.meshgrid(xs, ys, indexing="ij")
    for i in range(b):
        rng_i = single_rng if rng_list is None else rng_list[i]
        u = float(_rand_uniform(rng_i, 0.0, 1.0, (1,), obs_vox.device, obs_vox.dtype).item())
        if u >= prob:
            continue
        n_ext = 2 if float(_rand_uniform(rng_i, 0.0, 1.0, (1,), obs_vox.device, obs_vox.dtype).item()) < 0.40 else 1
        t_edge = float(t[i].mean().item())
        for _ in range(n_ext):
            side = int(math.floor(float(_rand_uniform(rng_i, 0.0, 3.999, (1,), obs_vox.device, obs_vox.dtype).item())))
            out_cells = float(_rand_uniform(rng_i, 0.8, 3.6, (1,), obs_vox.device, obs_vox.dtype).item())
            along = float(_rand_uniform(rng_i, -0.10, 1.10, (1,), obs_vox.device, obs_vox.dtype).item())
            if side == 0:
                x_e, y_e = -out_cells * dx, along * ly
            elif side == 1:
                x_e, y_e = lx + out_cells * dx, along * ly
            elif side == 2:
                x_e, y_e = along * lx, -out_cells * dy
            else:
                x_e, y_e = along * lx, ly + out_cells * dy
            z_e = t_edge + float(_rand_uniform(rng_i, 0.8, 4.5, (1,), obs_vox.device, obs_vox.dtype).item()) * dz
            dz_e = (z_e - z_obs[i]).clamp_min(0.25 * dz)
            r2 = (gx - x_e) ** 2 + (gy - y_e) ** 2 + dz_e ** 2 + 1.0
            kern = dz_e / (r2 * torch.sqrt(r2))
            kmax = kern.amax().clamp_min(1e-12)
            sign = -1.0 if float(_rand_uniform(rng_i, 0.0, 1.0, (1,), obs_vox.device, obs_vox.dtype).item()) < 0.5 else 1.0
            amp = float(_rand_uniform(rng_i, 0.10, 0.40, (1,), obs_vox.device, obs_vox.dtype).item()) * float(peak[i].item())
            obs_vox[i, 0].add_(kern * (sign * amp / kmax))


@torch.no_grad()
def _add_bouguer_topo_clutter(rho_vox: torch.Tensor, t: torch.Tensor, z_top: torch.Tensor, dz: float,
                              prob: float, rng_list, single_rng) -> None:
    """Add mild strictly relief-correlated Bouguer terrain density error into ``rho_vox`` ONLY."""
    b, _, nz, nx, ny = rho_vox.shape
    below = (z_top >= t.unsqueeze(1).unsqueeze(2)).to(rho_vox.dtype)
    shallow = (z_top < (t.unsqueeze(1).unsqueeze(2) + max(1.5 * dz, 80.0))).to(rho_vox.dtype)
    zone = below * shallow
    for i in range(b):
        if float((t[i].max() - t[i].min()).item()) < 15.0:
            continue
        rng_i = single_rng if rng_list is None else rng_list[i]
        u = float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho_vox.device, rho_vox.dtype).item())
        if u >= prob:
            continue
        t_4d = t[i:i + 1].unsqueeze(1)
        t_pad = torch.nn.functional.pad(t_4d, (4, 4, 4, 4), mode="replicate")
        t_res = t_4d - torch.nn.functional.avg_pool2d(t_pad, kernel_size=9, stride=1)
        t_res = t_res - t_res.mean()
        std_r = t_res.std().clamp_min(1e-6)
        rho_peak = float(rho_vox[i, 0].abs().amax().clamp_min(0.10).item())
        sign = -1.0 if float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho_vox.device, rho_vox.dtype).item()) < 0.5 else 1.0
        amp = float(_rand_uniform(rng_i, 0.008, 0.022, (1,), rho_vox.device, rho_vox.dtype).item()) * min(1.0, rho_peak / 0.50)
        rho_vox[i, 0].add_((t_res[0] * (sign * amp / std_r)) * zone[i, 0])


@torch.no_grad()
def simulate_batch(op: TerrainGravityOperator, rho, t, clearance, noise_frac, noise_seed=None,
                   body_vox_noise=BODY_VOX_NOISE, bg_vox_scale=BG_VOX_NOISE_SCALE,
                   dropout_prob: float = 0.0, return_mask: bool = False,
                   v6_physics: bool = False,
                   subvox_cap_op=None, subvox_cap_prob: float = 0.0,
                   detrend_probs=(0.35, 0.25, 0.15, 0.25),
                   exterior_mass_prob: float = 0.40,
                   pregrid_blur_prob: float = 0.20,
                   line_drift_prob: float = 0.30,
                   bouguer_err_prob: float = 0.0,
                   anisotropic_clutter: bool = False,
                   return_detrend_modes: bool = False):
    """Batched forward on ``op``'s device. Returns ``(obs_noisy, obs_clean, z_obs[, valid_mask][, detrend_modes])``.

    ``obs_clean`` is always the exact un-detrended gravity field of clean ``rho`` (for ``l_data``).
    In ``v6_physics=True`` mode, ``obs_noisy`` also incorporates anisotropic subsurface clutter,
    mild relief-correlated Bouguer terrain density error, continuous thin-slab sub-voxel surface
    slivers, exterior off-grid mass edge ramps, flight-line leveling drift, 5-mode missing-station
    dropout + harmonic infill, optional 2x-3x pre-gridded blur (with ``valid_mask=1``), and
    post-infill Mod3D regional detrending (``BorderPlanar``/``PlanarRamp``/``ConstantShift``/``raw``).
    """
    b, _, nz, nx, ny = rho.shape
    clr_map = _clearance_to_map(clearance, nx, ny, b, rho.device, rho.dtype)
    z_obs = t - clr_map

    rng_list = None
    rng_np = None
    if noise_seed is not None:
        flat_seeds = noise_seed.flatten().cpu().tolist()
        rng_list = [np.random.default_rng(int(s) + 17) for s in flat_seeds]
        rng_np = rng_list[0]
        noise = torch.stack([torch.randn((1, nx, ny), generator=torch.Generator().manual_seed(int(s)))
                             for s in flat_seeds]).to(rho.device, dtype=rho.dtype)
        vox_n = torch.stack([torch.randn(rho.shape[1:], generator=torch.Generator().manual_seed(int(s) + 1))
                             for s in flat_seeds]).to(rho.device, dtype=rho.dtype)
    else:
        noise = torch.randn((b, 1, nx, ny), device=rho.device, dtype=rho.dtype)
        vox_n = torch.randn_like(rho)

    use_aniso = bool(anisotropic_clutter or v6_physics)
    if body_vox_noise > 0.0 or bg_vox_scale > 0.0:
        z_top = torch.arange(op.nz, device=rho.device, dtype=rho.dtype).view(1, 1, op.nz, 1, 1) * op.dz
        below = (z_top >= t.unsqueeze(1).unsqueeze(2)).to(rho.dtype)
        amp = rho.abs().amax(dim=(1, 2, 3, 4), keepdim=True).clamp_min(0.1)
        sigma_vox = body_vox_noise * rho.abs() + bg_vox_scale * noise_frac.view(-1, 1, 1, 1, 1) * amp
        vox_corr = torch.nn.functional.avg_pool3d(vox_n, kernel_size=3, stride=1, padding=1) * 3.0
        if use_aniso:
            vox_bed = torch.nn.functional.avg_pool3d(
                vox_n, kernel_size=(1, 3, 3), stride=1, padding=(0, 1, 1)
            ) * 2.4
            vox_mix = 0.55 * vox_n + 0.25 * vox_corr + 0.20 * vox_bed
        else:
            vox_mix = 0.72 * vox_n + 0.28 * vox_corr
        rho_vox = rho + sigma_vox * vox_mix * below
        if v6_physics and bouguer_err_prob > 0.0:
            _add_bouguer_topo_clutter(rho_vox, t, z_top, op.dz, bouguer_err_prob, rng_list, rng_np)

        try:
            both = op(torch.cat([rho, rho_vox], dim=1), z_obs)
            clean, obs_vox = both[:, 0:1], both[:, 1:2]
        except Exception:
            both = op(torch.cat([rho, rho_vox], dim=0), torch.cat([z_obs, z_obs], dim=0))
            clean, obs_vox = both[:b], both[b:]
    else:
        rho_vox = rho
        clean = op(rho, z_obs)
        obs_vox = clean.clone()

    if v6_physics and subvox_cap_op is not None and subvox_cap_prob > 0.0:
        cap_mask = torch.zeros((b, 1, 1, 1), device=rho.device, dtype=rho.dtype)
        for i in range(b):
            rng_i = rng_np if rng_list is None else rng_list[i]
            if float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho.device, rho.dtype).item()) < subvox_cap_prob:
                cap_mask[i, 0, 0, 0] = 1.0
        if bool((cap_mask > 0).any()):
            iz_top = torch.ceil(t / op.dz).long().clamp(0, op.nz - 1)
            thickness = (iz_top.to(t.dtype) * op.dz - t).clamp(0.0, op.dz)
            rho_top = rho_vox[:, 0].gather(1, iz_top.unsqueeze(1))
            cap_gz = subvox_cap_op(rho_top, thickness, clr_map.clamp_min(0.25))
            obs_vox = obs_vox + cap_mask * cap_gz

    peak = clean.abs().amax(dim=(1, 2, 3), keepdim=True).clamp_min(1e-6)
    if v6_physics and exterior_mass_prob > 0.0:
        _add_exterior_offgrid_mass(obs_vox, peak, t, z_obs, op.dx, op.dy, op.dz,
                                   exterior_mass_prob, rng_list, rng_np)

    noise_corr = torch.nn.functional.avg_pool2d(noise, kernel_size=3, stride=1, padding=1) * 2.1
    noise_mix = 0.75 * noise + 0.25 * noise_corr
    noisy = obs_vox + noise_mix * noise_frac.view(-1, 1, 1, 1) * peak

    if v6_physics and line_drift_prob > 0.0:
        coord_1d = torch.linspace(-1.0, 1.0, max(nx, ny), device=rho.device, dtype=rho.dtype)
        for i in range(b):
            rng_i = rng_np if rng_list is None else rng_list[i]
            if float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho.device, rho.dtype).item()) >= line_drift_prob:
                continue
            axis_x = float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho.device, rho.dtype).item()) < 0.5
            sig_l = float(_rand_uniform(rng_i, 0.010, 0.026, (1,), rho.device, rho.dtype).item()) * float(peak[i].item())
            if axis_x:
                if rng_i is None:
                    dc = torch.randn((nx, 1), device=rho.device, dtype=rho.dtype)
                    tilt = torch.randn((nx, 1), device=rho.device, dtype=rho.dtype) * 0.5
                else:
                    dc = torch.from_numpy(rng_i.standard_normal((nx, 1)).astype(np.float32)).to(rho.device)
                    tilt = torch.from_numpy(rng_i.standard_normal((nx, 1)).astype(np.float32) * 0.5).to(rho.device)
                drift = (dc + tilt * coord_1d[:ny].view(1, ny)) * sig_l
            else:
                if rng_i is None:
                    dc = torch.randn((1, ny), device=rho.device, dtype=rho.dtype)
                    tilt = torch.randn((1, ny), device=rho.device, dtype=rho.dtype) * 0.5
                else:
                    dc = torch.from_numpy(rng_i.standard_normal((1, ny)).astype(np.float32)).to(rho.device)
                    tilt = torch.from_numpy(rng_i.standard_normal((1, ny)).astype(np.float32) * 0.5).to(rho.device)
                drift = (dc + tilt * coord_1d[:nx].view(nx, 1)) * sig_l
            noisy[i, 0].add_(drift)

    valid_mask = torch.ones((b, 1, nx, ny), device=rho.device, dtype=rho.dtype)
    did_dropout = [False] * b
    if dropout_prob > 0.0:
        n_modes = 5 if v6_physics else 3
        for i in range(b):
            rng_i = rng_np if rng_list is None else rng_list[i]
            u_d = float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho.device, rho.dtype).item())
            if u_d >= dropout_prob:
                continue
            vm = valid_mask[i, 0]
            mode = int(math.floor(u_d / max(dropout_prob, 1e-6) * float(n_modes))) % n_modes
            if mode == 0:  # irregular cutout / survey corner/border missing patch
                x0 = int(_rand_uniform(rng_i, 0, max(1, nx // 2), (1,), rho.device, rho.dtype).item())
                y0 = int(_rand_uniform(rng_i, 0, max(1, ny // 2), (1,), rho.device, rho.dtype).item())
                w_x = int(_rand_uniform(rng_i, max(2, nx // 8), max(3, nx // 3), (1,), rho.device, rho.dtype).item())
                w_y = int(_rand_uniform(rng_i, max(2, ny // 8), max(3, ny // 3), (1,), rho.device, rho.dtype).item())
                vm[x0:min(nx, x0 + w_x), y0:min(ny, y0 + w_y)] = 0.0
            elif mode == 1:  # axis-aligned missing flight/survey traverse lines
                step = max(2, int(_rand_uniform(rng_i, 3, 6, (1,), rho.device, rho.dtype).item()))
                phase = int(_rand_uniform(rng_i, 0, step, (1,), rho.device, rho.dtype).item())
                if phase % 2 == 0:
                    vm[phase::step, :] = 0.0
                else:
                    vm[:, phase::step] = 0.0
            elif mode == 2:  # random sparse dummy holes
                keep = _rand_uniform(rng_i, 0.0, 1.0, (nx, ny), rho.device, rho.dtype) > 0.22
                vm.mul_(keep.to(rho.dtype))
            elif mode == 3:  # oblique rotated flight traverse lines
                _apply_oblique_line_mask(vm, rng_i)
            else:  # non-rectangular polygonal survey border/corner void
                _apply_boundary_void_mask(vm, rng_i)

            # Safety floor: keep >= 45% valid stations so infill remains physical
            if float(vm.mean().item()) < 0.45:
                vm.fill_(1.0)
            else:
                did_dropout[i] = True
        noisy = infill_missing_obs(noisy, valid_mask)

    detrend_modes = ["raw"] * b
    if v6_physics:
        if pregrid_blur_prob > 0.0:
            grid_k = torch.arange(-2, 3, device=rho.device, dtype=rho.dtype)
            for i in range(b):
                if did_dropout[i]:
                    continue
                rng_i = rng_np if rng_list is None else rng_list[i]
                if float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho.device, rho.dtype).item()) >= pregrid_blur_prob:
                    continue
                sig_g = float(_rand_uniform(rng_i, 0.85, 1.65, (1,), rho.device, rho.dtype).item())
                k1 = torch.exp(-0.5 * (grid_k / sig_g) ** 2)
                k1 = k1 / k1.sum()
                k2 = (k1[:, None] * k1[None, :]).view(1, 1, 5, 5)
                padded = torch.nn.functional.pad(noisy[i:i + 1], (2, 2, 2, 2), mode="replicate")
                noisy[i:i + 1] = torch.nn.functional.conv2d(padded, k2)

        probs = np.asarray(detrend_probs, dtype=np.float64)
        probs = probs / max(float(probs.sum()), 1e-12)
        cdf = np.cumsum(probs)
        mode_names = ("border_planar", "planar", "constant", "raw")
        xs_n = torch.linspace(-1.0, 1.0, nx, device=rho.device, dtype=rho.dtype)[:, None]
        ys_n = torch.linspace(-1.0, 1.0, ny, device=rho.device, dtype=rho.dtype)[None, :]
        exec_modes = ["raw"] * b
        for i in range(b):
            rng_i = rng_np if rng_list is None else rng_list[i]
            u_m = float(_rand_uniform(rng_i, 0.0, 0.999999, (1,), rho.device, rho.dtype).item())
            m_idx = int(np.searchsorted(cdf, u_m, side="right"))
            m_name = mode_names[min(m_idx, 3)]
            detrend_modes[i] = m_name
            if m_name == "planar":
                exec_modes[i] = "cpp_border_eval"
            elif m_name == "border_planar":
                u_c = float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho.device, rho.dtype).item())
                exec_modes[i] = "cpp_border_eval" if u_c < 0.50 else ("cpp_border_proj" if u_c < 0.75 else "border_planar")
            else:
                exec_modes[i] = m_name
            if m_name != "raw":
                # Mild regional quadratic curvature on ~25% of detrended scenes so planar removal leaves subtle leakage
                if float(_rand_uniform(rng_i, 0.0, 1.0, (1,), rho.device, rho.dtype).item()) < 0.25:
                    q_scale = 0.03 if m_name == "constant" else 0.06
                    c_xx = float(_rand_uniform(rng_i, -q_scale, q_scale, (1,), rho.device, rho.dtype).item())
                    c_yy = float(_rand_uniform(rng_i, -q_scale, q_scale, (1,), rho.device, rho.dtype).item())
                    c_xy = float(_rand_uniform(rng_i, -q_scale, q_scale, (1,), rho.device, rho.dtype).item())
                    quad = c_xx * (xs_n ** 2) + c_yy * (ys_n ** 2) + c_xy * (xs_n * ys_n)
                    quad = (quad - quad.mean()) * float(peak[i].item())
                    noisy[i, 0].add_(quad)
        noisy = apply_mod3d_detrend(noisy, exec_modes)

    if return_mask and return_detrend_modes:
        return noisy, clean, z_obs, valid_mask, detrend_modes
    if return_mask:
        return noisy, clean, z_obs, valid_mask
    if return_detrend_modes:
        return noisy, clean, z_obs, detrend_modes
    return noisy, clean, z_obs


def terrain_channel(t, depth_total):
    """Network terrain input: terrain depth / domain depth, (B, 1, nx, ny)."""
    return (t / depth_total).unsqueeze(1)


def build_v5_input(obs, t, depth_total, valid_mask=None, aspect=1.0, clearance=0.35, dxy_canon=CELL,
                   clearance_clamp=4.0):
    """Build 5-channel v5/v6 input ``(B, 5, H, W)``:
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
    clr_t = _clearance_to_map(clearance, nx, ny, b, obs.device, obs.dtype).unsqueeze(1)
    log_asp = torch.log(asp_t.clamp(0.1, 10.0))
    clr_norm = (clr_t / float(dxy_canon)).clamp(0.0, float(clearance_clamp))
    return torch.cat([obs, terr, valid_mask, log_asp, clr_norm], dim=1)


def build_v6_input(obs, t, depth_total, valid_mask=None, aspect=1.0, clearance=0.35, dxy_canon=CELL):
    """Build 5-channel v6 input ``(B, 5, H, W)`` with canonical 400 m (``8.0``) clearance ceiling."""
    return build_v5_input(
        obs, t, depth_total, valid_mask=valid_mask, aspect=aspect, clearance=clearance,
        dxy_canon=dxy_canon, clearance_clamp=CLEARANCE_CLAMP_V6,
    )


# Normalized detrend-mode conditioning codes for v7 channel 5 (includes both canonical
# ``simulate_batch(..., return_detrend_modes=True)`` names and C++/config aliases).
DETREND_MODE_CODE = {
    "raw": 0.0,
    "none": 0.0,
    "constant": 0.33,
    "constant_shift": 0.33,
    "planar": 0.67,
    "planar_ramp": 0.67,
    "border_planar": 1.0,
    "cpp_border": 1.0,
    "cpp_border_eval": 1.0,
    "cpp_border_proj": 1.0,
}


def _detrend_mode_to_code(mode) -> float:
    if mode is None:
        return 1.0
    if isinstance(mode, (int, float)):
        return float(mode)
    key = str(mode).strip().lower()
    if key in DETREND_MODE_CODE:
        return float(DETREND_MODE_CODE[key])
    try:
        norm = _normalize_detrend_mode(key)
        return float(DETREND_MODE_CODE.get(norm, 1.0))
    except ValueError:
        return 1.0


def build_v7_input(obs, t, depth_total, valid_mask=None, aspect=1.0, clearance=0.35,
                   clearance_clamp=CLEARANCE_CLAMP_V6, detrend_modes=None, dxy_canon=CELL):
    """Build 6-channel v7 input ``(B, 6, H, W)``:
    ``[gz_50m_eq, terr/depth, valid_mask, log(dz/dxy), clearance/dxy_canon, detrend_mode_code]``.
    """
    if float(clearance_clamp) == float(CLEARANCE_CLAMP_V6):
        x5 = build_v6_input(
            obs, t, depth_total, valid_mask=valid_mask, aspect=aspect,
            clearance=clearance, dxy_canon=dxy_canon,
        )
    else:
        x5 = build_v5_input(
            obs, t, depth_total, valid_mask=valid_mask, aspect=aspect,
            clearance=clearance, dxy_canon=dxy_canon, clearance_clamp=clearance_clamp,
        )
    b, _, nx, ny = x5.shape
    if detrend_modes is None:
        ch5 = torch.ones((b, 1, nx, ny), device=x5.device, dtype=x5.dtype)
    elif isinstance(detrend_modes, torch.Tensor):
        if detrend_modes.shape == (b, 1, nx, ny):
            ch5 = detrend_modes.to(device=x5.device, dtype=x5.dtype)
        else:
            ch5 = detrend_modes.to(device=x5.device, dtype=x5.dtype).reshape(b, 1, 1, 1).expand(b, 1, nx, ny)
    elif isinstance(detrend_modes, np.ndarray) and detrend_modes.dtype.kind not in ("U", "S", "O"):
        ch5 = torch.as_tensor(detrend_modes, device=x5.device, dtype=x5.dtype).reshape(b, 1, 1, 1).expand(b, 1, nx, ny)
    else:
        if isinstance(detrend_modes, str):
            modes_list = [detrend_modes] * b
        else:
            modes_list = list(detrend_modes)
            if len(modes_list) != b:
                raise ValueError(f"Expected {b} detrend modes, got {len(modes_list)}")
        codes = [_detrend_mode_to_code(m) for m in modes_list]
        ch5 = torch.tensor(codes, device=x5.device, dtype=x5.dtype).view(b, 1, 1, 1).expand(b, 1, nx, ny)
    return torch.cat([x5, ch5], dim=1)


def augment_d4(rho, t, obs, clean, z_obs, valid_mask=None, k=None, flip=None,
               clearance=None, unit_id=None):
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
        if clearance is not None and isinstance(clearance, torch.Tensor) and clearance.ndim >= 2:
            clearance = torch.rot90(clearance, k, dims=(-2, -1))
        if unit_id is not None:
            unit_id = torch.rot90(unit_id, k, dims=(-2, -1))
    if flip:
        rho = torch.flip(rho, dims=(-1,))
        t = torch.flip(t, dims=(-1,))
        obs = torch.flip(obs, dims=(-1,))
        clean = torch.flip(clean, dims=(-1,))
        z_obs = torch.flip(z_obs, dims=(-1,))
        if valid_mask is not None:
            valid_mask = torch.flip(valid_mask, dims=(-1,))
        if clearance is not None and isinstance(clearance, torch.Tensor) and clearance.ndim >= 2:
            clearance = torch.flip(clearance, dims=(-1,))
        if unit_id is not None:
            unit_id = torch.flip(unit_id, dims=(-1,))
    out = [rho, t, obs, clean, z_obs]
    if valid_mask is not None:
        out.append(valid_mask)
    if clearance is not None:
        out.append(clearance)
    if unit_id is not None:
        out.append(unit_id)
    return tuple(out)


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
