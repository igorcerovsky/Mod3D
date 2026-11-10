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

from losses import _dilate3d


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
    """Mass-weighted per-body vertical-column depth centroid error [m]."""
    if float(p.abs().max()) < 1e-6:
        return float("nan")
    vals = [v for v in torch.unique(t) if v != 0]
    if not vals:
        return float("nan")
    errs, weights = [], []
    zp_global = _centroid_depth(p, dz)
    for v in vals:
        bm = (t == v)
        zt_b = _centroid_depth(t * bm, dz)
        foot = bm.any(dim=0, keepdim=True).unsqueeze(0).float()
        foot_dil = (F.max_pool2d(foot, kernel_size=7, stride=1, padding=3)[0] > 0)
        p_cyl = torch.where(foot_dil & (p * v > 0), p, torch.zeros_like(p))
        zp_b = _centroid_depth(p_cyl, dz)
        if np.isnan(zp_b):
            zp_b = zp_global
        if not (np.isnan(zp_b) or np.isnan(zt_b)):
            errs.append(abs(zp_b - zt_b))
            weights.append(float(bm.sum()) * abs(float(v)))
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
        thresh = torch.full_like(pa, 0.5 * ref_p)
        for v in torch.unique(t[body]).tolist():
            zv = _dilate3d((t == v).float().unsqueeze(0).unsqueeze(0), 3)[0, 0] > 0
            pm_z = float(pa[zv].max()) if zv.any() else 0.0
            av = abs(float(v))
            ref_z = min(pm_z, av) if pm_z >= 0.25 * av else av
            thresh = torch.where(zv, torch.full_like(thresh, 0.5 * ref_z), thresh)
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


def make_fixed_set(n, seed, op, device, grid):
    """Deterministic validation tensors on ``device``: obs (B,2,H,W), truth, clean, z_obs."""
    from dataset import BodyTerrainDataset, simulate_batch, terrain_channel
    nx, ny, nz, dx, dy, dz = grid
    ds = BodyTerrainDataset(n, nx, ny, nz, dx, dy, dz, seed=seed)
    items = [ds[i] for i in range(n)]
    rho, t, c, nf, ns = (torch.stack([it[k] for it in items]) for k in range(5))
    rho, t, c, nf = rho.to(device), t.to(device), c.to(device), nf.to(device)
    obs, clean, z_obs = simulate_batch(op, rho, t, c, nf, ns)
    x = torch.cat([obs, terrain_channel(t, nz * dz)], dim=1)
    return x, rho, clean, z_obs


def main():
    from dataset import NX, NY, NZ, CELL
    from forward import TerrainGravityOperator
    from model_swin3d import PotentialFieldTransformer3D

    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--samples", type=int, default=64)
    ap.add_argument("--seed", type=int, default=12345)
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    ap.add_argument("--device", default=default_dev)
    args = ap.parse_args()

    ck = torch.load(args.checkpoint, map_location="cpu")
    g = ck.get("grid", {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL})
    grid = (g["nx"], g["ny"], g["nz"], g["cell"], g["cell"], g["cell"])
    model = PotentialFieldTransformer3D(g["nx"], g["ny"], g["nz"], **ck.get("arch", {}))
    model.load_state_dict(ck["model_state_dict"])
    model.to(args.device).eval()
    op = TerrainGravityOperator(*grid).to(args.device)
    x, truth, clean, z_obs = make_fixed_set(args.samples, args.seed, op, args.device, grid)
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
