"""Validation metrics for 3D gravity inversion predictions (network or C++ solver output).

All metrics are physical and comparable between the Transformer and ``inv`` (C++):
  * l1               mean |m_pred - m_true|                          [g/cm^3]
  * data_rel_misfit  ||G m_pred - d_obs|| / ||d_obs||                 (0 == perfect, 1 == zero model)
  * iou              IoU of the positive half-maximum regions
  * centroid_dz      |depth centroid of positive mass: pred - true|   [m]
  * peak_ratio       max(pred) / max(true)
  * s_ms, s_mgs      focusing functionals (volume / face-area fractions, e = e_rel * max|m|)

CLI: python evaluate.py --checkpoint checkpoints/best_swin3d.pt --samples 64
The zero predictor is always reported as a baseline: a trained model must beat it on every row.
"""

import argparse

import numpy as np
import torch

from losses import ZhdanovFocusingLoss


def _centroid_depth(m, dz):
    pos = torch.clamp(m, min=0.0)
    w = pos.sum(dim=(-2, -1))                       # (D,)
    z = (torch.arange(m.shape[-3], device=m.device, dtype=m.dtype) + 0.5) * dz
    tot = w.sum()
    return float((w * z).sum() / tot) if tot > 0 else float("nan")


@torch.no_grad()
def compute_metrics(pred, truth, obs, forward_op, spacing=(50.0, 50.0, 50.0), e_rel=0.05):
    """pred/truth: (B,1,D,H,W); obs: (B,1,H,W). Returns a dict of batch-averaged metrics."""
    foc = ZhdanovFocusingLoss(spacing)
    out = {k: [] for k in ("l1", "data_rel_misfit", "iou", "centroid_dz", "peak_ratio", "s_ms", "s_mgs")}
    for i in range(pred.shape[0]):
        p, t, d = pred[i:i + 1], truth[i:i + 1], obs[i:i + 1]
        out["l1"].append(float((p - t).abs().mean()))
        dp = forward_op(p)
        out["data_rel_misfit"].append(float(torch.linalg.norm(dp - d) / (torch.linalg.norm(d) + 1e-12)))
        pm, tm = float(p.max()), float(t.max())
        a, b = (p >= 0.5 * pm) & (pm > 0), (t >= 0.5 * tm) & (tm > 0)
        union = (a | b).sum().item()
        out["iou"].append((a & b).sum().item() / union if union else 1.0)
        zp, zt = _centroid_depth(p[0, 0], spacing[0]), _centroid_depth(t[0, 0], spacing[0])
        out["centroid_dz"].append(abs(zp - zt) if not (np.isnan(zp) or np.isnan(zt)) else float("nan"))
        out["peak_ratio"].append(pm / tm if tm > 0 else float("nan"))
        e = max(1e-4, e_rel * float(p.abs().max()))
        ms, mgs = foc(p, e)
        out["s_ms"].append(float(ms))
        out["s_mgs"].append(float(mgs))
    return {k: float(np.nanmean(v)) if len(v) else float("nan") for k, v in out.items()}


def main():
    from dataset import SyntheticPotentialFieldDataset
    from forward import GravityFFTOperator
    from model_swin3d import PotentialFieldTransformer3D

    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--samples", type=int, default=64)
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--device", default="cpu")
    args = ap.parse_args()

    ck = torch.load(args.checkpoint, map_location=args.device)
    g = ck.get("grid", {"nx": 64, "ny": 64, "nz": 32, "dx": 50.0, "dy": 50.0, "dz": 50.0})
    model = PotentialFieldTransformer3D(in_channels=1, out_depth=g["nz"], embed_dim=96, num_heads=4)
    model.load_state_dict(ck["model_state_dict"])
    model.to(args.device).eval()
    op = GravityFFTOperator(g["nx"], g["ny"], g["nz"], g["dx"], g["dy"], g["dz"]).to(args.device)
    ds = SyntheticPotentialFieldDataset(args.samples, g["nx"], g["ny"], g["nz"], g["dx"], g["dy"], g["dz"],
                                        seed=args.seed, operator=op)
    obs = torch.stack([ds[i][0] for i in range(len(ds))]).to(args.device)
    truth = torch.stack([ds[i][1] for i in range(len(ds))]).to(args.device)
    spacing = (g["dz"], g["dx"], g["dy"])
    with torch.no_grad():
        pred = model(obs)
    rows = {"network": compute_metrics(pred, truth, obs, op, spacing),
            "zero predictor": compute_metrics(torch.zeros_like(truth), truth, obs, op, spacing)}
    keys = list(rows["network"].keys())
    print(f"{'':16s}" + "".join(f"{k:>18s}" for k in keys))
    for name, r in rows.items():
        print(f"{name:16s}" + "".join(f"{r[k]:18.4f}" for k in keys))


if __name__ == "__main__":
    main()
