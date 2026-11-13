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


def _sample_centroid_dz(p, t, dz):
    """Mass-weighted per-component 3D Voronoi vertical depth centroid error [m]."""
    if float(p.abs().max()) < 1e-6:
        return float("nan")
    t_np = t.detach().cpu().numpy()
    vals = [float(v) for v in np.unique(t_np) if v != 0.0]
    if not vals:
        return float("nan")

    comps = []
    for v in vals:
        lbl, n_c = label(t_np == v)
        for cid in range(1, n_c + 1):
            comps.append((v, lbl == cid))

    # Partition same-sign space by 3D Voronoi cells so nested or stacked bodies don't interfere
    voronoi = [None] * len(comps)
    for sgn in (1.0, -1.0):
        idxs = [k for k, (v, _) in enumerate(comps) if v * sgn > 0.0]
        if len(idxs) > 1:
            dists = np.stack([distance_transform_edt(~comps[k][1]) for k in idxs], axis=0)
            owners = np.argmin(dists, axis=0)
            for pos_i, k in enumerate(idxs):
                voronoi[k] = torch.from_numpy(owners == pos_i).to(t.device)

    errs, weights = [], []
    zp_global = _centroid_depth(p, dz)
    for k, (v, comp_np) in enumerate(comps):
        bm = torch.from_numpy(comp_np).to(t.device)
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
            weights.append(float(bm.sum()) * abs(v))
    return float(np.average(errs, weights=weights)) if errs else float("nan")


@torch.no_grad()
def compute_metrics(pred, truth, d_clean, z_obs, forward_op, dz=50.0):
    """pred/truth: (B,1,D,H,W); d_clean: (B,1,H,W); z_obs: (B,H,W). Batch-averaged metrics."""
    keys = ("l1_rel", "body_l1_rel", "data_rel_misfit", "iou", "centroid_dz", "amp_ratio", "halo_opp")
    out = {k: [] for k in keys}
    dp_all = forward_op(pred, z_obs)
    pos_m, neg_m = (truth > 0).float(), (truth < 0).float()
    pos_p = _dilate3d(pos_m, 1) + _dilate3d(pos_m, 3) + _dilate3d(pos_m, 7)
    neg_p = _dilate3d(neg_m, 1) + _dilate3d(neg_m, 3) + _dilate3d(neg_m, 7)
    for i in range(pred.shape[0]):
        p, t, d, dp = pred[i, 0], truth[i, 0], d_clean[i], dp_all[i]
        a = float(t.abs().max())
        body = t != 0
        pa, ta = p.abs(), t.abs()
        out["l1_rel"].append(float((p - t).abs().mean()) / a)
        out["body_l1_rel"].append(float(((p - t)[body].abs() / ta[body]).mean()))
        out["data_rel_misfit"].append(float(torch.linalg.norm(dp - d) / (torch.linalg.norm(d) + 1e-12)))
        pm = float(pa.max())
        ref_p = min(pm, a) if pm > 0 else 0.0
        body_thresh = torch.full_like(pa, 0.5 * ref_p)
        bg_thresh = torch.full_like(pa, float("inf"))
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
        out["centroid_dz"].append(_sample_centroid_dz(p, t, dz))
        t_mass = float(ta.sum()) + 1e-12
        out["amp_ratio"].append(float((p * torch.sign(t))[body].sum()) / t_mass)
        pp, np_ = pos_p[i, 0], neg_p[i, 0]
        only_pos = float((t < 0).sum() == 0)
        only_neg = float((t > 0).sum() == 0)
        pos_z = (((pp > np_).float() + only_pos).clamp_max(1.0)) * (~body).float()
        neg_z = (((np_ > pp).float() + only_neg).clamp_max(1.0)) * (~body).float()
        opp_halo = (F.relu(-p) * pos_z + F.relu(p) * neg_z).sum()
        out["halo_opp"].append(float(opp_halo) / (float(pa.sum()) + t_mass))
    return {
        k: (float(np.mean(valid)) if (valid := [x for x in v if not np.isnan(x)]) else float("nan"))
        for k, v in out.items()
    }


def make_fixed_set(n, seed, op, device, grid, hard_prob=0.0, in_channels=2,
                   fault_prob=0.0, clearance_range=None, overburden_prob=0.0,
                   trend_prob=0.0, dropout_prob=0.0):
    """Deterministic validation tensors on ``device``: obs (B,C,H,W), truth, clean, z_obs."""
    from dataset import CLEARANCE, BodyTerrainDataset, build_v5_input, simulate_batch, terrain_channel
    nx, ny, nz, dx, dy, dz = grid
    cr = CLEARANCE if clearance_range is None else clearance_range
    ds = BodyTerrainDataset(n, nx, ny, nz, dx, dy, dz, seed=seed, hard_prob=hard_prob,
                            fault_prob=fault_prob, clearance_range=cr)
    items = [ds[i] for i in range(n)]
    rho, t, c, nf, ns = (torch.stack([it[k] for it in items]) for k in range(5))
    rho, t, c, nf = rho.to(device), t.to(device), c.to(device), nf.to(device)
    if in_channels >= 5 or overburden_prob > 0.0 or trend_prob > 0.0 or dropout_prob > 0.0:
        obs, clean, z_obs, vm = simulate_batch(
            op, rho, t, c, nf, ns,
            overburden_prob=overburden_prob, trend_prob=trend_prob,
            dropout_prob=dropout_prob, return_mask=True,
        )
        if in_channels >= 5:
            x = build_v5_input(obs, t, nz * dz, valid_mask=vm, aspect=dz / dx, clearance=c)
        else:
            x = torch.cat([obs, terrain_channel(t, nz * dz)], dim=1)
    else:
        obs, clean, z_obs = simulate_batch(op, rho, t, c, nf, ns)
        x = torch.cat([obs, terrain_channel(t, nz * dz)], dim=1)
    return x, rho, clean, z_obs


def make_category_showcase(op, device, grid, seed=4024, max_tries=120, in_channels=2):
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
        x = build_v5_input(obs, terr_t, nz * dz, aspect=dz / dx, clearance=clear_t)
    else:
        x = torch.cat([obs, terrain_channel(terr_t, nz * dz)], dim=1)
    return x, rho_t, clean, z_obs, tuple(name for name, _, _ in specs)


@torch.no_grad()
def export_examples_figure(model, op, device, grid, out_path, epoch=None):
    """Render the 16 dashboard validation & showcase X-Z cross-sections in ``training_monitor.png`` style.

    Arranges the 16 pairs (4 Std, 4 Hard, 8 curated category showcase) in an 8-row by 2-pair
    layout (4 cross-section columns: Truth | Pred, Truth | Pred) suited for Markdown README width.
    """
    import os
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    nx, ny, nz, dx, _, dz = grid
    x_km = np.arange(nx) * dx / 1000.0
    z_m = np.arange(nz) * dz
    in_ch = 5 if hasattr(model, "logvar_head") else 2

    sx, st, _, _ = make_fixed_set(4, 2024, op, device, grid, hard_prob=0.0, in_channels=in_ch)
    hx, ht, _, _ = make_fixed_set(4, 3024, op, device, grid, hard_prob=1.0, in_channels=in_ch)
    cx, ct, _, _, show_labels = make_category_showcase(op, device, grid, seed=4024, in_channels=in_ch)

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
def export_vol3d_snapshot(model, op, device, grid, out_dir, epoch=None, is_best=True):
    """Evaluate the 16 dashboard validation/showcase samples and write 3D volume sidecars."""
    nx, ny, nz, _, _, dz = grid
    y_km = np.arange(ny) * dz / 1000.0
    in_ch = 5 if hasattr(model, "logvar_head") else 2
    sx, st, _, sz = make_fixed_set(4, 2024, op, device, grid, hard_prob=0.0, in_channels=in_ch)
    hx, ht, _, hz = make_fixed_set(4, 3024, op, device, grid, hard_prob=1.0, in_channels=in_ch)
    cx, ct, _, cz, show_labels = make_category_showcase(op, device, grid, seed=4024, in_channels=in_ch)

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
    from dataset import NX, NY, NZ, CELL
    from forward import TerrainGravityOperator
    from model_swin3d import create_model

    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--samples", type=int, default=64)
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--hard-prob", type=float, default=0.0, help="fraction of harder/larger scenes in evaluation [0-1]")
    ap.add_argument("--export-examples", type=str, default="", help="save 16-sample dashboard Truth/Pred X-Z figure to PNG")
    ap.add_argument("--export-vol3d", type=str, default="", help="write 3D volume sidecar files (.json/.js) to directory for dashboard 3D mode")
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    ap.add_argument("--device", default=default_dev)
    args = ap.parse_args()

    ck = torch.load(args.checkpoint, map_location="cpu")
    g = ck.get("grid", {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL})
    grid = (g["nx"], g["ny"], g["nz"], g["cell"], g["cell"], g["cell"])
    model = create_model(g["nx"], g["ny"], g["nz"], **ck.get("arch", {}))
    model.load_state_dict(ck["model_state_dict"])
    model.to(args.device).eval()
    op = TerrainGravityOperator(*grid).to(args.device)
    if args.export_examples or args.export_vol3d:
        if args.export_examples:
            out_p = export_examples_figure(model, op, args.device, grid, args.export_examples, epoch=ck.get("epoch"))
            print(f"Exported examples grid to {out_p}")
        if args.export_vol3d:
            out_d = export_vol3d_snapshot(model, op, args.device, grid, args.export_vol3d, epoch=ck.get("epoch"), is_best=True)
            print(f"Exported 3D volume sidecars to {out_d}")
        return
    in_ch = 5 if ck.get("arch", {}).get("variant") == "v5" else 2
    x, truth, clean, z_obs = make_fixed_set(
        args.samples, args.seed, op, args.device, grid,
        hard_prob=args.hard_prob, in_channels=in_ch,
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
