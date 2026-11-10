"""Validation metrics for isolated-body gravity inversion (network or C++ solver output).

  * l1_rel           mean |m_pred - m_true| / max|m_true|            (zero model: ~body fraction)
  * body_l1_rel      same, restricted to true body cells             (zero model: ~1.0)
  * data_rel_misfit  ||G m_pred - d|| / ||d||                        (0 perfect, 1 zero model)
  * iou              IoU of |m| >= 0.5 max|m| regions (pred vs truth)
  * centroid_dz      |depth centroid of |m|: pred - true|            [m]
  * amp_ratio        sum(m_pred * sign(m_true) over body cells) / sum|m_true|  (1 == right mass)

CLI: python evaluate.py --checkpoint checkpoints/best_swin3d.pt --samples 64
"""

import argparse

import numpy as np
import torch


def _centroid_depth(m, dz):
    a = m.abs()
    w = a.sum(dim=(-2, -1))
    z = (torch.arange(m.shape[-3], device=m.device, dtype=m.dtype) + 0.5) * dz
    tot = w.sum()
    return float((w * z).sum() / tot) if tot > 0 else float("nan")


@torch.no_grad()
def compute_metrics(pred, truth, d_clean, z_obs, forward_op, dz=50.0):
    """pred/truth: (B,1,D,H,W); d_clean: (B,1,H,W); z_obs: (B,H,W). Batch-averaged metrics."""
    keys = ("l1_rel", "body_l1_rel", "data_rel_misfit", "iou", "centroid_dz", "amp_ratio")
    out = {k: [] for k in keys}
    dp_all = forward_op(pred, z_obs)
    for i in range(pred.shape[0]):
        p, t, d, dp = pred[i, 0], truth[i, 0], d_clean[i], dp_all[i]
        a = float(t.abs().max())
        body = t != 0
        out["l1_rel"].append(float((p - t).abs().mean()) / a)
        out["body_l1_rel"].append(float((p - t)[body].abs().mean()) / a)
        out["data_rel_misfit"].append(float(torch.linalg.norm(dp - d) / (torch.linalg.norm(d) + 1e-12)))
        pa, ta = p.abs(), t.abs()
        pm = float(pa.max())
        A = (pa >= 0.5 * pm) & (pm > 0)
        B = ta >= 0.5 * a
        union = (A | B).sum().item()
        out["iou"].append((A & B).sum().item() / union if union else 1.0)
        zp, zt = _centroid_depth(p, dz), _centroid_depth(t, dz)
        out["centroid_dz"].append(abs(zp - zt) if not (np.isnan(zp) or np.isnan(zt)) else float("nan"))
        out["amp_ratio"].append(float((p * torch.sign(t))[body].sum() / ta.sum()))
    return {k: float(np.nanmean(v)) for k, v in out.items()}


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
    rows = {"network": compute_metrics(pred, truth, clean, z_obs, op, g["cell"]),
            "zero predictor": compute_metrics(torch.zeros_like(truth), truth, clean, z_obs, op, g["cell"])}
    keys = list(rows["network"].keys())
    print(f"{'':16s}" + "".join(f"{k:>16s}" for k in keys))
    for name, r in rows.items():
        print(f"{name:16s}" + "".join(f"{r[k]:16.4f}" for k in keys))


if __name__ == "__main__":
    main()
