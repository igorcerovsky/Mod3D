"""Validation metrics for isolated-body gravity inversion (network or C++ solver output).

  * l1_rel           mean |m_pred - m_true| / max|m_true|            (zero model: ~body fraction)
  * body_l1_rel      same, restricted to true body cells             (zero model: ~1.0)
  * data_rel_misfit  ||G m_pred - d|| / ||d||                        (0 perfect, 1 zero model)
  * iou              IoU of |m| >= 0.5 max|m| regions (pred vs truth)
  * centroid_dz      |depth centroid of |m|: pred - true|            [m]
  * amp_ratio        sum(m_pred * sign(m_true) over body cells) / sum|m_true|  (1 == right mass)
  * halo_opp         opposite-sign compensating halo mass / (||m_pred||_1 + ||m_true||_1) in [0, 1]

CLI: python evaluate.py --checkpoint checkpoints/best_swin3d.pt --samples 64
"""

import argparse

import numpy as np
import torch
import torch.nn.functional as F
from scipy.ndimage import distance_transform_edt, label

from losses import _dilate3d
from vol3d_io import write_vol3d_sidecars


@torch.no_grad()
def project_amplitude(pred, d_obs, z_obs, forward_op, bounds=(0.25, 4.0)):
    """1-step optimal linear least-squares amplitude projection: alpha* = <G pred, d_obs> / ||G pred||^2."""
    dp = forward_op(pred, z_obs)
    num = (dp * d_obs).flatten(1).sum(1)
    den = (dp * dp).flatten(1).sum(1)
    alpha = torch.where(den > 1e-9, (num / den.clamp_min(1e-9)).clamp(*bounds), torch.ones_like(den))
    return pred * alpha.view(-1, 1, 1, 1, 1)


def _centroid_depth(m, dz, thresh_frac=0.20):
    a = m.abs()
    amax = float(a.max())
    if amax < 1e-6:
        return float("nan")
    mask = (a >= thresh_frac * amax).float()
    w = (a * mask).sum(dim=(-2, -1))
    z = (torch.arange(m.shape[-3], device=m.device, dtype=m.dtype) + 0.5) * dz
    tot = w.sum()
    return float((w * z).sum() / tot) if tot > 0 else float("nan")


def _sample_centroid_dz(p, t, dz, uid=None):
    """Mass-weighted per-component (or integer ``unit_id``) 3D Voronoi vertical depth centroid error [m]."""
    if float(p.abs().max()) < 1e-6:
        return float("nan")

    comps = []
    if uid is not None:
        u_max = int(uid.max().item())
        for u in range(1, u_max + 1):
            bm = (uid == u) & (t != 0)
            if int(bm.sum().item()) == 0:
                continue
            v_mean = float(t[bm].mean().item())
            sgn = 1.0 if v_mean >= 0.0 else -1.0
            mass_u = float(t[bm].abs().sum().item())
            comps.append((sgn, bm, mass_u))
        if len(comps) > 1:
            t_np_comps = [bm.detach().cpu().numpy() for _, bm, _ in comps]
        else:
            t_np_comps = None
    else:
        t_np = t.detach().cpu().numpy()
        vals = [float(v) for v in np.unique(t_np) if v != 0.0]
        if not vals:
            return float("nan")
        t_np_comps = []
        for v in vals:
            lbl, n_c = label(t_np == v)
            for cid in range(1, n_c + 1):
                c_np = lbl == cid
                comps.append((v, c_np, float(np.count_nonzero(c_np)) * abs(v)))
                t_np_comps.append(c_np)

    if not comps:
        return float("nan")

    # Partition same-sign space by 3D Voronoi cells so nested or stacked bodies don't interfere
    voronoi = [None] * len(comps)
    for sgn in (1.0, -1.0):
        idxs = [k for k, (v, _, _) in enumerate(comps) if v * sgn > 0.0]
        if len(idxs) > 1:
            dists = np.stack([distance_transform_edt(~t_np_comps[k]) for k in idxs], axis=0)
            owners = np.argmin(dists, axis=0)
            for pos_i, k in enumerate(idxs):
                voronoi[k] = torch.from_numpy(owners == pos_i).to(t.device)

    errs, weights = [], []
    zp_global = _centroid_depth(p, dz)
    for k, (v, comp_obj, mass_w) in enumerate(comps):
        bm = comp_obj if isinstance(comp_obj, torch.Tensor) else torch.from_numpy(comp_obj).to(t.device)
        zt_b = _centroid_depth(t * bm, dz)
        foot = bm.any(dim=0, keepdim=True).unsqueeze(0).float()
        foot_dil = (F.max_pool2d(foot, kernel_size=7, stride=1, padding=3)[0] > 0)
        reg = foot_dil & (p * v > 0)
        if voronoi[k] is not None:
            reg = reg & voronoi[k]
        p_cyl = torch.where(reg, p, torch.zeros_like(p))
        zp_b = _centroid_depth(p_cyl, dz)
        if np.isnan(zp_b):
            zp_b = zp_global
        if not (np.isnan(zp_b) or np.isnan(zt_b)):
            errs.append(abs(zp_b - zt_b))
            weights.append(mass_w)
    return float(np.average(errs, weights=weights)) if errs else float("nan")


@torch.no_grad()
def compute_metrics(pred, truth, d_clean, z_obs, forward_op, dz=50.0, unit_id=None):
    """pred/truth: (B,1,D,H,W); d_clean: (B,1,H,W); z_obs: (B,H,W). Batch-averaged metrics.

    When integer ``unit_id`` ``(B,1,D,H,W)`` is supplied, per-unit thresholds and Voronoi centroids
    are computed directly from geological units (fast, vectorized, and graded-rock safe).
    """
    keys = (
        "l1_rel", "body_l1_rel", "data_rel_misfit", "iou", "centroid_dz",
        "amp_ratio", "halo_opp", "sediment_recall", "mafic_recall",
    )
    out = {k: [] for k in keys}
    dp_all = forward_op(pred, z_obs)
    pos_m, neg_m = (truth > 0).float(), (truth < 0).float()
    pos_p = _dilate3d(pos_m, 1) + _dilate3d(pos_m, 3) + _dilate3d(pos_m, 7)
    neg_p = _dilate3d(neg_m, 1) + _dilate3d(neg_m, 3) + _dilate3d(neg_m, 7)
    for i in range(pred.shape[0]):
        p, t, d, dp = pred[i, 0], truth[i, 0], d_clean[i], dp_all[i]
        a = max(float(t.abs().max()), 1e-6)
        body = t != 0
        pa, ta = p.abs(), t.abs()
        out["l1_rel"].append(float((p - t).abs().mean()) / a)
        out["body_l1_rel"].append(float(((p - t)[body].abs() / ta[body]).mean()) if body.any() else 0.0)
        out["data_rel_misfit"].append(float(torch.linalg.norm(dp - d) / (torch.linalg.norm(d) + 1e-12)))
        pm = float(pa.max())
        ref_p = min(pm, a) if pm > 0 else 0.0
        body_thresh = torch.full_like(pa, 0.5 * ref_p)
        bg_thresh = torch.full_like(pa, float("inf"))

        if unit_id is not None:
            uid_i = unit_id[i, 0].long()
            k_max = int(uid_i.max().item())
            if k_max > 0:
                # Vectorized over all K <= 16 geological units in one 3D max_pool3d call (no float unique!)
                u_masks = F.one_hot(uid_i.clamp(0, k_max), k_max + 1)[..., 1:].permute(3, 0, 1, 2).float()
                u_active = u_masks * body.unsqueeze(0).float()
                zv_all = F.max_pool3d(u_active.unsqueeze(1), kernel_size=3, stride=1, padding=1)[:, 0] > 0
                counts = u_active.sum(dim=(1, 2, 3))
                pa_masked = torch.where(zv_all, pa.unsqueeze(0), torch.zeros_like(zv_all, dtype=pa.dtype))
                pm_z_all = pa_masked.amax(dim=(1, 2, 3))
                av_u_all = (ta.unsqueeze(0) * u_active).amax(dim=(1, 2, 3))
                keep = counts > 0
                ref_u = torch.where((pm_z_all >= 0.25 * av_u_all) & keep, torch.minimum(pm_z_all, av_u_all), av_u_all)
                tv_u = 0.5 * ref_u
                # Reduces identically to 0.5 * ref_u on piecewise-constant units and scales smoothly on graded halos
                scale_u = (tv_u / av_u_all.clamp_min(1e-6)).view(k_max, 1, 1, 1)
                body_map = (u_active * scale_u).sum(dim=0) * ta
                body_thresh = torch.where(body & (uid_i > 0), body_map, body_thresh)
                zv_bg = zv_all & (~body).unsqueeze(0) & keep.view(k_max, 1, 1, 1)
                tv_bg = torch.where(zv_bg, tv_u.view(k_max, 1, 1, 1), torch.full_like(zv_bg, float("inf"), dtype=pa.dtype))
                bg_thresh = tv_bg.amin(dim=0)
        else:
            for v in torch.unique(t[body]).tolist():
                bm = (t == v)
                zv = _dilate3d(bm.float().unsqueeze(0).unsqueeze(0), 3)[0, 0] > 0
                pm_z = float(pa[zv].max()) if zv.any() else 0.0
                av = abs(float(v))
                ref_z = min(pm_z, av) if pm_z >= 0.25 * av else av
                tv = 0.5 * ref_z
                body_thresh = torch.where(bm, torch.full_like(body_thresh, tv), body_thresh)
                bg_thresh = torch.where(zv & (~body), torch.minimum(bg_thresh, torch.full_like(bg_thresh, tv)), bg_thresh)

        bg_thresh = torch.where(torch.isinf(bg_thresh), torch.full_like(bg_thresh, 0.5 * ref_p), bg_thresh)
        thresh = torch.where(body, body_thresh, bg_thresh)
        A = (pa >= thresh) & (pm > 0)
        B = body
        union = (A | B).sum().item()
        out["iou"].append((A & B).sum().item() / union if union else 1.0)
        uid_i_dz = unit_id[i, 0] if unit_id is not None else None
        out["centroid_dz"].append(_sample_centroid_dz(p, t, dz, uid=uid_i_dz))
        t_mass = float(ta.sum()) + 1e-12
        out["amp_ratio"].append(float((p * torch.sign(t))[body].sum()) / t_mass)
        pp, np_ = pos_p[i, 0], neg_p[i, 0]
        only_pos = float((t < 0).sum() == 0)
        only_neg = float((t > 0).sum() == 0)
        pos_z = (((pp > np_).float() + only_pos).clamp_max(1.0)) * (~body).float()
        neg_z = (((np_ > pp).float() + only_neg).clamp_max(1.0)) * (~body).float()
        opp_halo = (F.relu(-p) * pos_z + F.relu(p) * neg_z).sum()
        out["halo_opp"].append(float(opp_halo) / (float(pa.sum()) + t_mass))

        # Lithology recall on weak/moderate targets (sediment/waste [-0.855,-0.02], mafic/weak [0.02,0.80])
        min_hit = torch.maximum(torch.full_like(ta, 0.02), 0.25 * ta)
        hit_same = (p * t > 0) & (pa >= min_hit)
        sed_m = (t <= -0.02) & (t >= -0.855)
        if sed_m.any():
            out["sediment_recall"].append(float(hit_same[sed_m].float().mean()))
        maf_m = (t >= 0.02) & (t <= 0.80)
        if maf_m.any():
            out["mafic_recall"].append(float(hit_same[maf_m].float().mean()))

    return {
        k: (float(np.mean(valid)) if (valid := [x for x in v if not np.isnan(x)]) else float("nan"))
        for k, v in out.items()
    }


@torch.no_grad()
def compute_calibration_metrics(pred, truth, log_var, terrain=None, dz=50.0, log_var_range=(-8.0, 4.0)):
    """Uncertainty & calibration metrics aligned with C++ ``sigma = exp(0.5 * log_var)``.

    Returns dict with ``unc_spearman`` (Spearman corr ``(|err|, sigma)``), ``aurc`` (Area Under
    Risk-Coverage curve on L1 error), ``bd_sig_err_ratio`` (mean ``sigma / |err|`` on 1-cell body
    boundaries), and ``fg_1sig_cov`` (fraction of true body voxels with ``|err| <= sigma``).
    """
    if log_var is None:
        return {"unc_spearman": float("nan"), "aurc": float("nan"),
                "bd_sig_err_ratio": float("nan"), "fg_1sig_cov": float("nan")}
    sigma = torch.exp(0.5 * log_var.clamp(float(log_var_range[0]), float(log_var_range[1])))
    err = (pred - truth).abs()

    if terrain is not None:
        z_top = torch.arange(pred.shape[-3], device=pred.device, dtype=pred.dtype).view(1, 1, -1, 1, 1) * float(dz)
        sub_mask = z_top >= terrain.unsqueeze(1).unsqueeze(2)
    else:
        sub_mask = torch.ones_like(err, dtype=torch.bool)

    e_sub = err[sub_mask]
    s_sub = sigma[sub_mask]
    n_sub = int(e_sub.numel())
    if n_sub < 16:
        return {"unc_spearman": float("nan"), "aurc": float("nan"),
                "bd_sig_err_ratio": float("nan"), "fg_1sig_cov": float("nan")}

    # Subsample deterministically if huge (>180k voxels) to keep sorting fast on CPU/MPS
    stride = max(1, n_sub // 150000)
    e_s = e_sub[::stride].detach().cpu().float()
    s_s = s_sub[::stride].detach().cpu().float()

    # Spearman rank correlation via argsort-of-argsort
    def _rank(x: torch.Tensor) -> torch.Tensor:
        order = torch.argsort(x)
        r = torch.empty_like(x)
        r[order] = torch.arange(x.numel(), dtype=torch.float32)
        return r

    re = _rank(e_s)
    rs = _rank(s_s)
    re_c = re - re.mean()
    rs_c = rs - rs.mean()
    den = torch.sqrt((re_c ** 2).sum() * (rs_c ** 2).sum()).clamp_min(1e-12)
    unc_spearman = float((re_c * rs_c).sum() / den)

    # AURC: cumulative mean L1 risk ordered from most confident (smallest sigma) to least confident
    conf_order = torch.argsort(s_s)
    e_sorted = e_s[conf_order]
    cum_mean = torch.cumsum(e_sorted, dim=0) / torch.arange(1, e_sorted.numel() + 1, dtype=torch.float32)
    aurc = float(cum_mean.mean())

    # 1-cell body boundary calibration ratio sigma / |err|
    fg = truth != 0
    if fg.any():
        fg_1sig_cov = float((err[fg] <= sigma[fg]).float().mean())
        fg_f = fg.float()
        fg_dil = F.max_pool3d(fg_f, kernel_size=3, stride=1, padding=1) > 0.5
        fg_ero = (-F.max_pool3d(-fg_f, kernel_size=3, stride=1, padding=1)) > 0.5
        bd = (fg_dil ^ fg_ero) & sub_mask
        if bd.any():
            mean_s = float(sigma[bd].mean())
            mean_e = max(float(err[bd].mean()), 1e-6)
            bd_sig_err_ratio = mean_s / mean_e
        else:
            bd_sig_err_ratio = float("nan")
    else:
        fg_1sig_cov = float("nan")
        bd_sig_err_ratio = float("nan")

    return {
        "unc_spearman": unc_spearman,
        "aurc": aurc,
        "bd_sig_err_ratio": bd_sig_err_ratio,
        "fg_1sig_cov": fg_1sig_cov,
    }


def make_fixed_set(n, seed, op, device, grid, hard_prob=0.0, in_channels=2,
                   fault_prob=0.0, clearance_range=None, dropout_prob=0.0,
                   clearance_clamp=4.0, v6_mode=False):
    """Deterministic validation tensors on ``device``: obs (B,C,H,W), truth, clean, z_obs."""
    from dataset import (
        CLEARANCE, BodyTerrainDataset, build_v5_input, simulate_batch, terrain_channel,
    )
    nx, ny, nz, dx, dy, dz = grid
    cr = CLEARANCE if clearance_range is None else clearance_range
    ds = BodyTerrainDataset(n, nx, ny, nz, dx, dy, dz, seed=seed, hard_prob=hard_prob,
                            fault_prob=fault_prob, clearance_range=cr, v6_mode=v6_mode)
    items = [ds[i] for i in range(n)]
    if v6_mode:
        rho, _uid, t, c, nf, ns = (torch.stack([it[k] for it in items]) for k in range(6))
    else:
        rho, t, c, nf, ns = (torch.stack([it[k] for it in items]) for k in range(5))
    rho, t, c, nf = rho.to(device), t.to(device), c.to(device), nf.to(device)
    if in_channels >= 5 or dropout_prob > 0.0:
        obs, clean, z_obs, vm = simulate_batch(
            op, rho, t, c, nf, ns, dropout_prob=dropout_prob, return_mask=True,
            v6_physics=v6_mode,
        )
        if in_channels >= 5:
            x = build_v5_input(
                obs, t, nz * dz, valid_mask=vm, aspect=dz / dx, clearance=c,
                clearance_clamp=clearance_clamp,
            )
        else:
            x = torch.cat([obs, terrain_channel(t, nz * dz)], dim=1)
    else:
        obs, clean, z_obs = simulate_batch(op, rho, t, c, nf, ns)
        x = torch.cat([obs, terrain_channel(t, nz * dz)], dim=1)
    return x, rho, clean, z_obs


def _build_v6_bucket_chunks(
    n_samples: int,
    seed: int,
    device,
    grid,
    op_map: dict,
    cap_map: dict | None,
    aspect_ratios: tuple[float, ...],
    fixed_aspect_idx: int | None,
    ds_kwargs: dict,
    sim_kwargs: dict,
    chunk_size: int = 8,
):
    from dataset import ASPECT_MULTI, BodyTerrainDataset, build_v6_input, simulate_batch

    nx, ny, nz, dx, dy, _ = grid
    ds = BodyTerrainDataset(n_samples, nx, ny, nz, dx, dy, dx, seed=seed, v6_mode=True,
                            aspect_ratios=aspect_ratios, **ds_kwargs)
    items = [ds[i] for i in range(n_samples)]
    groups: dict[int, list] = {}
    for idx, it in enumerate(items):
        a_idx = int(it[-1].item()) if fixed_aspect_idx is None else int(fixed_aspect_idx)
        if fixed_aspect_idx is not None:
            # Re-sample with exact target dz when pinned to canonical aspect
            a_idx = int(fixed_aspect_idx)
        groups.setdefault(a_idx, []).append(it)

    chunks = []
    for a_idx, grp in sorted(groups.items()):
        asp = float(aspect_ratios[a_idx])
        dz_cur = dx * asp
        op_cur = op_map[a_idx]
        cap_cur = None if cap_map is None else cap_map.get(a_idx)
        for s0 in range(0, len(grp), max(1, int(chunk_size))):
            sub = grp[s0:s0 + max(1, int(chunk_size))]
            rho = torch.stack([it[0] for it in sub]).to(device)
            uid = torch.stack([it[1] for it in sub]).to(device)
            t = torch.stack([it[2] for it in sub]).to(device)
            c = torch.stack([it[3] for it in sub]).to(device)
            nf = torch.stack([it[4] for it in sub]).to(device)
            ns = torch.stack([it[5] for it in sub])
            obs, clean, z_obs, vm = simulate_batch(
                op_cur, rho, t, c, nf, ns, return_mask=True, subvox_cap_op=cap_cur, **sim_kwargs
            )
            x = build_v6_input(obs, t, nz * dz_cur, valid_mask=vm, aspect=asp, clearance=c, dxy_canon=dx)
            chunks.append({
                "x": x, "truth": rho, "unit_id": uid, "terrain": t,
                "clean": clean, "z_obs": z_obs, "op": op_cur,
                "dz": dz_cur, "aspect": asp,
            })
    return chunks


def make_v6_val_buckets(
    n_per_bucket: int,
    seed: int,
    canonical_op,
    op_bank: dict,
    device,
    grid,
    subvox_cap_bank: dict | None = None,
    aspect_ratios=None,
    chunk_size: int = 8,
):
    """Build 4 stratified v6 validation buckets matching primary ground + large + airborne + multi-aspect:

      - ``ground_clean``: primary ground ``[0.30..2.0 m]``, clean/moderate anomalies, raw/mild clutter
      - ``ground_hard_large_detrend``: primary ground, hard faults + large landfills/basins + Mod3D detrending
      - ``airborne_relief_drape``: secondary relief-drape airborne ``[2..160+ m]`` + line drift + oblique masks
      - ``multi_aspect``: mixed ground/airborne across all ``ASPECT_MULTI`` ratios ``[0.55..1.40]``
    """
    from dataset import ASPECT_MULTI, CLEARANCE_AIRBORNE_V6

    ratios = tuple(ASPECT_MULTI if aspect_ratios is None else aspect_ratios)
    canon_idx = ratios.index(1.0) if 1.0 in ratios else 0
    canon_op_map = {0: canonical_op}
    canon_cap_map = None if subvox_cap_bank is None else {0: subvox_cap_bank.get(canon_idx)}

    specs = (
        ("ground_clean", 0, (1.0,), canon_op_map, canon_cap_map, 0,
         {"hard_prob": 0.15, "fault_prob": 0.10, "large_prob": 0.05, "ground_prob": 1.0, "max_airborne": 2.0},
         {"dropout_prob": 0.08, "v6_physics": True, "detrend_probs": (0.0, 0.0, 0.0, 1.0),
          "exterior_mass_prob": 0.0, "pregrid_blur_prob": 0.0, "line_drift_prob": 0.0,
          "bouguer_err_prob": 0.0, "subvox_cap_prob": 0.25}),
        ("ground_hard_large_detrend", 1000, (1.0,), canon_op_map, canon_cap_map, 0,
         {"hard_prob": 0.78, "fault_prob": 0.55, "large_prob": 0.45, "ground_prob": 1.0, "max_airborne": 2.0},
         {"dropout_prob": 0.35, "v6_physics": True, "detrend_probs": (0.45, 0.35, 0.20, 0.0),
          "exterior_mass_prob": 0.50, "pregrid_blur_prob": 0.25, "line_drift_prob": 0.25,
          "bouguer_err_prob": 0.35, "subvox_cap_prob": 0.40}),
        ("airborne_relief_drape", 2000, (1.0,), canon_op_map, canon_cap_map, 0,
         {"hard_prob": 0.45, "fault_prob": 0.35, "large_prob": 0.25, "ground_prob": 0.0,
          "max_airborne": CLEARANCE_AIRBORNE_V6[1]},
         {"dropout_prob": 0.35, "v6_physics": True, "detrend_probs": (0.35, 0.30, 0.15, 0.20),
          "exterior_mass_prob": 0.40, "pregrid_blur_prob": 0.20, "line_drift_prob": 0.45,
          "bouguer_err_prob": 0.30, "subvox_cap_prob": 0.30}),
        ("multi_aspect", 3000, ratios, op_bank, subvox_cap_bank, None,
         {"hard_prob": 0.55, "fault_prob": 0.40, "large_prob": 0.25, "ground_prob": 0.67,
          "max_airborne": CLEARANCE_AIRBORNE_V6[1]},
         {"dropout_prob": 0.30, "v6_physics": True, "detrend_probs": (0.35, 0.25, 0.15, 0.25),
          "exterior_mass_prob": 0.35, "pregrid_blur_prob": 0.20, "line_drift_prob": 0.30,
          "bouguer_err_prob": 0.30, "subvox_cap_prob": 0.35}),
    )

    out = {}
    for name, s_off, r_tuple, o_map, c_map, f_idx, d_kw, s_kw in specs:
        out[name] = _build_v6_bucket_chunks(
            n_per_bucket, seed + s_off, device, grid, o_map, c_map,
            r_tuple, f_idx, d_kw, s_kw, chunk_size=chunk_size,
        )
    return out


def make_category_showcase(op, device, grid, seed=4024, max_tries=120, in_channels=2,
                           clearance_clamp=4.0):
    """Curate 8 deterministic showcase samples spanning all lithology & hard-scene categories.

    Returns ``(obs_x, truth, clean, z_obs, labels)`` with 8 samples whose peak slice ``iy``
    exhibits the target category, falling back gracefully on tiny test grids.
    """
    from dataset import (
        CLEARANCE, NOISE_FRAC, RHO_AIR, RHO_LIGHT_MAX, RHO_LIGHT_MIN,
        RHO_MAFIC_MIN, RHO_WATER, build_v5_input, random_bodies, random_terrain,
        simulate_batch, terrain_channel,
    )

    nx, ny, nz, dx, dy, dz = grid
    rng = np.random.default_rng(seed)
    can_hard = min(nx, ny) >= 24 and nz >= 12

    def _peak_slice(rho):
        _, _, iy = np.unravel_index(np.argmax(np.abs(rho)), rho.shape)
        return rho[:, :, iy]

    def _n_distinct(sl):
        nz_vals = sl[sl != 0]
        return len(np.unique(np.round(nz_vals, 2))) if nz_vals.size else 0

    specs = (
        ("Cavities", None,
         lambda sl: ((np.isclose(sl, RHO_AIR, atol=0.05) | np.isclose(sl, RHO_WATER, atol=0.05)).any()
                     and sl.max() <= 0.05)),
        ("Light / Coal", None,
         lambda sl: (((sl <= RHO_LIGHT_MAX + 0.02) & (sl >= RHO_LIGHT_MIN - 0.02)).any()
                     and sl.max() <= 0.05)),
        ("Tight Cluster", "tight_cluster", lambda sl: np.count_nonzero(sl) >= 12),
        ("Mafic / Ore", None, lambda sl: sl.max() >= RHO_MAFIC_MIN and sl.min() >= -0.05),
        ("Large Composite", "large_composite", lambda sl: np.count_nonzero(sl) >= 24),
        ("Nested Halo", "nested_halo", lambda sl: _n_distinct(sl) >= 2),
        ("Abutting Contact", "abutting_contact", lambda sl: _n_distinct(sl) >= 2),
        ("Vertical Stack", "vertical_stack", lambda sl: _n_distinct(sl) >= 2),
    )

    rhos, terrains, clearances, noise_fracs, noise_seeds = [], [], [], [], []
    for _, scene_type, pred in specs:
        chosen = None
        for _ in range(max_tries):
            t = random_terrain(rng, nx, ny, nz * dz)
            rho = random_bodies(rng, t, nz, dz, dx, hard_prob=0.0,
                                scene_type=scene_type if can_hard else None)
            c = np.float32(rng.uniform(*CLEARANCE))
            nf = np.float32(rng.uniform(*NOISE_FRAC))
            ns = int(rng.integers(0, 2**31 - 1))
            chosen = (rho, t, c, nf, ns)
            if pred(_peak_slice(rho)):
                break
        rho, t, c, nf, ns = chosen
        rhos.append(torch.from_numpy(rho)[None])
        terrains.append(torch.from_numpy(t))
        clearances.append(torch.tensor(c))
        noise_fracs.append(torch.tensor(nf))
        noise_seeds.append(torch.tensor(ns))

    rho_t = torch.stack(rhos).to(device)
    terr_t = torch.stack(terrains).to(device)
    clear_t = torch.stack(clearances).to(device)
    nf_t = torch.stack(noise_fracs).to(device)
    ns_t = torch.stack(noise_seeds)
    obs, clean, z_obs = simulate_batch(op, rho_t, terr_t, clear_t, nf_t, ns_t)
    if in_channels >= 5:
        x = build_v5_input(
            obs, terr_t, nz * dz, aspect=dz / dx, clearance=clear_t,
            clearance_clamp=clearance_clamp,
        )
    else:
        x = torch.cat([obs, terrain_channel(terr_t, nz * dz)], dim=1)
    return x, rho_t, clean, z_obs, tuple(name for name, _, _ in specs)


@torch.no_grad()
def export_examples_figure(model, op, device, grid, out_path, epoch=None, clearance_clamp=None):
    """Render the 16 dashboard validation & showcase X-Z cross-sections in ``training_monitor.png`` style.

    Arranges the 16 pairs (4 Std, 4 Hard, 8 curated category showcase) in an 8-row by 2-pair
    layout (4 cross-section columns: Truth | Pred, Truth | Pred) suited for Markdown README width.
    """
    import os
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from dataset import CLEARANCE_CLAMP_V6

    nx, ny, nz, dx, _, dz = grid
    x_km = np.arange(nx) * dx / 1000.0
    z_m = np.arange(nz) * dz
    is_v6 = getattr(model, "variant", None) == "v6" or (
        clearance_clamp is not None and float(clearance_clamp) > 4.0
    )
    in_ch = 5 if (hasattr(model, "logvar_head") or is_v6) else 2
    clr_clamp = CLEARANCE_CLAMP_V6 if is_v6 else (4.0 if clearance_clamp is None else float(clearance_clamp))

    sx, st, _, _ = make_fixed_set(4, 2024, op, device, grid, hard_prob=0.0, in_channels=in_ch, clearance_clamp=clr_clamp)
    hx, ht, _, _ = make_fixed_set(4, 3024, op, device, grid, hard_prob=1.0, in_channels=in_ch, clearance_clamp=clr_clamp)
    cx, ct, _, _, show_labels = make_category_showcase(op, device, grid, seed=4024, in_channels=in_ch, clearance_clamp=clr_clamp)

    all_x = torch.cat([sx, hx, cx], dim=0)
    all_t = torch.cat([st, ht, ct], dim=0)
    labels = ("Std",) * 4 + ("Hard",) * 4 + show_labels
    preds = [model(all_x[i:i + 8]) for i in range(0, len(all_x), 8)]
    all_p = torch.cat(preds, dim=0)

    n_pairs = len(all_x)
    n_cols = 2
    n_rows = (n_pairs + n_cols - 1) // n_cols
    fig = plt.figure(figsize=(15.0, 2.25 * n_rows + 0.9), constrained_layout=True)
    gs = fig.add_gridspec(n_rows, 5, width_ratios=[1.0, 1.0, 0.12, 1.0, 1.0], wspace=0.06, hspace=0.22)
    ep_tag = f"Ep {epoch}" if epoch is not None else "Best"

    for r in range(n_rows):
        sp = fig.add_subplot(gs[r, 2])
        sp.axis("off")

    for idx in range(n_pairs):
        row = idx // n_cols
        col_pair = idx % n_cols
        c0 = 0 if col_pair == 0 else 3

        t_vol = all_t[idx, 0].detach().cpu().numpy()
        p_vol = all_p[idx, 0].detach().cpu().numpy()
        terr_m = all_x[idx, 1].detach().cpu().numpy() * (nz * dz)
        _, _, iy_pk = np.unravel_index(np.argmax(np.abs(t_vol)), t_vol.shape)
        t_sec = t_vol[:, :, iy_pk]
        p_sec = p_vol[:, :, iy_pk]
        vmax_r = max(0.1, float(max(np.abs(t_sec).max(), np.abs(p_sec).max())))

        ax_t = fig.add_subplot(gs[row, c0])
        ax_p = fig.add_subplot(gs[row, c0 + 1])
        pair_titles = (
            f"#{idx} {labels[idx]} — Truth",
            f"#{idx} {labels[idx]} — Pred ({ep_tag})",
        )
        last_im = None
        for panel_i, (a, sec, title) in enumerate(zip((ax_t, ax_p), (t_sec, p_sec), pair_titles)):
            last_im = a.imshow(sec, origin="upper", extent=[x_km[0], x_km[-1], z_m[-1] + dz, 0.0],
                               cmap="coolwarm", vmin=-vmax_r, vmax=vmax_r, aspect="auto")
            a.fill_between(x_km, 0.0, terr_m[:, iy_pk], color="#d1d5db", alpha=0.85,
                           label="Air above relief" if idx == 0 and panel_i == 0 else None)
            a.plot(x_km, terr_m[:, iy_pk], color="#111827", linewidth=1.4,
                   label="Terrain surface" if idx == 0 and panel_i == 0 else None)
            a.set_title(title, fontsize=9.5, pad=3)
            if row == n_rows - 1:
                a.set_xlabel("X [km]", fontsize=8.5)
            else:
                a.tick_params(axis="x", labelbottom=False)
            if col_pair == 0 and panel_i == 0:
                a.set_ylabel("Depth [m]", fontsize=8.5)
            else:
                a.tick_params(axis="y", labelleft=False)
            a.tick_params(labelsize=8)
        cbar = fig.colorbar(last_im, ax=[ax_t, ax_p], fraction=0.025, pad=0.02)
        cbar.ax.tick_params(labelsize=7.5)
        if idx == 0:
            ax_t.legend(loc="lower right", fontsize=7)

    title_str = (f"Mod3D .inv — Dashboard Validation & Category Showcase X-Z Sections "
                 f"(Best Checkpoint: Epoch {epoch}, g/cm³)" if epoch is not None
                 else "Mod3D .inv — Dashboard Validation & Category Showcase X-Z Sections [g/cm³]")
    fig.suptitle(title_str, fontsize=12, fontweight="bold")

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    fig.savefig(out_path, dpi=130, bbox_inches="tight")
    plt.close(fig)
    return out_path


@torch.no_grad()
def export_vol3d_snapshot(model, op, device, grid, out_dir, epoch=None, is_best=True, clearance_clamp=None):
    """Evaluate the 16 dashboard validation/showcase samples and write 3D volume sidecars."""
    from dataset import CLEARANCE_CLAMP_V6

    nx, ny, nz, _, _, dz = grid
    y_km = np.arange(ny) * dz / 1000.0
    is_v6 = getattr(model, "variant", None) == "v6" or (
        clearance_clamp is not None and float(clearance_clamp) > 4.0
    )
    in_ch = 5 if (hasattr(model, "logvar_head") or is_v6) else 2
    clr_clamp = CLEARANCE_CLAMP_V6 if is_v6 else (4.0 if clearance_clamp is None else float(clearance_clamp))
    sx, st, _, sz = make_fixed_set(4, 2024, op, device, grid, hard_prob=0.0, in_channels=in_ch, clearance_clamp=clr_clamp)
    hx, ht, _, hz = make_fixed_set(4, 3024, op, device, grid, hard_prob=1.0, in_channels=in_ch, clearance_clamp=clr_clamp)
    cx, ct, _, cz, show_labels = make_category_showcase(op, device, grid, seed=4024, in_channels=in_ch, clearance_clamp=clr_clamp)

    all_x = torch.cat([sx, hx, cx], dim=0)
    all_t = torch.cat([st, ht, ct], dim=0)
    all_z = torch.cat([sz, hz, cz], dim=0)
    labels = ("Std",) * 4 + ("Hard",) * 4 + show_labels
    preds = [model(all_x[i:i + 8]) for i in range(0, len(all_x), 8)]
    all_p = torch.cat(preds, dim=0)
    pred_f_chunks = [op(all_p[i:i + 8], all_z[i:i + 8]) for i in range(0, len(all_p), 8)]
    all_pf = torch.cat(pred_f_chunks, dim=0)

    n_samples = len(all_x)
    vtrue_vols = [all_t[i, 0].detach().cpu().numpy() for i in range(n_samples)]
    vpred_vols = [all_p[i, 0].detach().cpu().numpy() for i in range(n_samples)]
    terr_xy_list = [all_x[i, 1].detach().cpu().numpy() * (nz * dz) for i in range(n_samples)]
    obs_fields = [all_x[i, 0].detach().cpu().numpy() for i in range(n_samples)]
    pred_fields = [all_pf[i, 0].detach().cpu().numpy() for i in range(n_samples)]
    sample_meta = []
    for i in range(n_samples):
        tv = vtrue_vols[i]
        _, _, iy_pk = np.unravel_index(np.argmax(np.abs(tv)), tv.shape)
        sample_meta.append({
            "id": i,
            "iy": int(iy_pk),
            "y_km": round(float(y_km[iy_pk]), 2),
            "vmax": round(float(np.abs(tv).max()), 3),
            "tag": str(labels[i]),
        })

    ep_num = int(epoch) if epoch is not None else 1
    write_vol3d_sidecars(
        out_dir, ep_num, nx, ny, nz, dz,
        vtrue_vols, vpred_vols, terr_xy_list, sample_meta,
        is_best=is_best, stride=0, write_meta=True,
        obs_fields=obs_fields, pred_fields=pred_fields,
    )
    return out_dir


def main():
    from dataset import ASPECT_MULTI, CLEARANCE_CLAMP_V6, NX, NY, NZ, CELL
    from forward import SubvoxelCapOperator, TerrainGravityOperator
    from model_swin3d import create_model

    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--samples", type=int, default=64)
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--hard-prob", type=float, default=0.0, help="fraction of harder/larger scenes in evaluation [0-1]")
    ap.add_argument("--v6-buckets", action="store_true", help="evaluate all 4 v6 stratified validation buckets")
    ap.add_argument("--export-examples", type=str, default="", help="save 16-sample dashboard Truth/Pred X-Z figure to PNG")
    ap.add_argument("--export-vol3d", type=str, default="", help="write 3D volume sidecar files (.json/.js) to directory for dashboard 3D mode")
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    ap.add_argument("--device", default=default_dev)
    args = ap.parse_args()

    ck = torch.load(args.checkpoint, map_location="cpu")
    g = ck.get("grid", {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL})
    grid = (g["nx"], g["ny"], g["nz"], g["cell"], g["cell"], g["cell"])
    arch = ck.get("arch", {})
    variant = str(arch.get("variant", "v5"))
    model = create_model(g["nx"], g["ny"], g["nz"], **arch)
    model.load_state_dict(ck["model_state_dict"])
    model.to(args.device).eval()
    h_min = -220.0 if variant == "v6" else -85.0
    op = TerrainGravityOperator(*grid, h_min=h_min).to(args.device)
    clr_clamp = CLEARANCE_CLAMP_V6 if variant == "v6" else 4.0
    if args.export_examples or args.export_vol3d:
        if args.export_examples:
            out_p = export_examples_figure(model, op, args.device, grid, args.export_examples,
                                           epoch=ck.get("epoch"), clearance_clamp=clr_clamp)
            print(f"Exported examples grid to {out_p}")
        if args.export_vol3d:
            out_d = export_vol3d_snapshot(model, op, args.device, grid, args.export_vol3d,
                                          epoch=ck.get("epoch"), is_best=True, clearance_clamp=clr_clamp)
            print(f"Exported 3D volume sidecars to {out_d}")
        return
    if args.v6_buckets or variant == "v6":
        n_pb = max(4, args.samples // 4)
        op_bank = {i: TerrainGravityOperator(g["nx"], g["ny"], g["nz"], g["cell"], g["cell"], g["cell"] * a, h_min=-220.0).to(args.device)
                   for i, a in enumerate(ASPECT_MULTI)}
        cap_bank = {
            i: SubvoxelCapOperator(g["nx"], g["ny"], g["cell"], g["cell"], g["cell"] * a).to(args.device)
            for i, a in enumerate(ASPECT_MULTI)
        }
        buckets = make_v6_val_buckets(n_pb, args.seed, op, op_bank, args.device, grid, subvox_cap_bank=cap_bank)
        with torch.no_grad():
            for bname, chunks in buckets.items():
                agg = {}
                tot_n = 0
                for ch in chunks:
                    out = model(ch["x"], return_aux=True)
                    pred, aux = (out[0], out[1]) if isinstance(out, tuple) else (out, {})
                    met = compute_metrics(pred, ch["truth"], ch["clean"], ch["z_obs"], ch["op"], ch["dz"], unit_id=ch["unit_id"])
                    cal = compute_calibration_metrics(pred, ch["truth"], aux.get("log_var"), terrain=ch["terrain"], dz=ch["dz"])
                    met.update(cal)
                    n_c = ch["truth"].shape[0]
                    for k, v in met.items():
                        if not np.isnan(v):
                            agg[k] = agg.get(k, 0.0) + v * n_c
                    tot_n += n_c
                line = " | ".join(f"{k}={agg[k] / tot_n:.4f}" for k in sorted(agg))
                print(f"[{bname}] {line}")
        return
    in_ch = 5 if variant in ("v5", "v6") else 2
    x, truth, clean, z_obs = make_fixed_set(
        args.samples, args.seed, op, args.device, grid,
        hard_prob=args.hard_prob, in_channels=in_ch, clearance_clamp=clr_clamp,
    )
    preds = []
    with torch.no_grad():
        for i in range(0, args.samples, 8):
            preds.append(model(x[i:i + 8]))
    pred = torch.cat(preds)
    pred_proj = project_amplitude(pred, x[:, :1], z_obs, op)
    rows = {"network": compute_metrics(pred, truth, clean, z_obs, op, g["cell"]),
            "network (proj)": compute_metrics(pred_proj, truth, clean, z_obs, op, g["cell"]),
            "zero predictor": compute_metrics(torch.zeros_like(truth), truth, clean, z_obs, op, g["cell"])}
    keys = list(rows["network"].keys())
    print(f"{'':16s}" + "".join(f"{k:>16s}" for k in keys))
    for name, r in rows.items():
        print(f"{name:16s}" + "".join(f"{r[k]:16.4f}" for k in keys))


if __name__ == "__main__":
    main()
