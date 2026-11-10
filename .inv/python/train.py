"""Train the 3D gravity-inversion transformer on isolated bodies / cavities below random terrain.

Usage:
    python train.py --gui                             # live macOS GUI window + graphs every epoch
    python train.py                                   # saves checkpoints/training_monitor.png + dashboard.html
    python train.py --epochs 2 --steps 20 --n-val 8   # smoke test (~1 min)
    python export_onnx.py --checkpoint checkpoints/best_swin3d.pt

Data is generated ON THE FLY (infinite, never repeats): geometry in DataLoader workers (numpy),
exact draped-station forward model batched on the training device (~2 ms/sample on MPS).
Validation uses a FIXED seeded set; the best checkpoint is chosen by the validation loss.
"""

import argparse
import json
import os
import time

import numpy as np
import torch
from torch.utils.data import DataLoader

from dataset import CELL, NX, NY, NZ, BodyTerrainDataset, simulate_batch, terrain_channel
from evaluate import compute_metrics, make_fixed_set
from forward import TerrainGravityOperator
from losses import CompoundInversionLoss
from model_swin3d import PotentialFieldTransformer3D

ARCH = {"embed_dim": 96, "num_heads": 4, "depth": 4}


def parse_args():
    p = argparse.ArgumentParser(description="Train the 3D gravity-inversion transformer")
    p.add_argument("--epochs", type=int, default=20)
    p.add_argument("--steps", type=int, default=200, help="optimizer steps per epoch")
    p.add_argument("--batch-size", type=int, default=8)
    p.add_argument("--lr", type=float, default=5e-4)
    p.add_argument("--n-val", type=int, default=64)
    p.add_argument("--num-workers", type=int, default=0)
    p.add_argument("--lambda-data", type=float, default=0.25)
    p.add_argument("--lambda-ms", type=float, default=0.05)
    p.add_argument("--save-dir", type=str, default="./checkpoints")
    p.add_argument("--resume", type=str, default="", help="continue from a checkpoint")
    p.add_argument("--gui", action="store_true", help="open a live native GUI window with training graphs")
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    p.add_argument("--device", type=str, default=default_dev)
    return p.parse_args()


class TrainingMonitor:
    """Renders a 6-panel training monitor (loss curves, 3D IoU/amplitude, depth error, and live
    gravity + vertical cross-section under terrain) to a native GUI window and/or PNG + HTML."""

    def __init__(self, save_dir, gui=False):
        import matplotlib
        if not gui:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        self.plt = plt
        self.save_dir = save_dir
        self.gui = gui
        self.hist = []
        if gui:
            plt.ion()
        self.fig, self.axes = plt.subplots(2, 3, figsize=(15, 8.5), constrained_layout=True)
        if gui and hasattr(self.fig.canvas, "manager") and self.fig.canvas.manager is not None:
            try:
                self.fig.canvas.manager.set_window_title("Mod3D .inv — 3D Swin Transformer Training Monitor")
            except Exception:
                pass
        self._write_html()

    def _write_html(self):
        html_path = os.path.join(self.save_dir, "dashboard.html")
        with open(html_path, "w") as f:
            f.write("""<!DOCTYPE html><html><head><meta charset="utf-8">
<meta http-equiv="refresh" content="5">
<title>Mod3D .inv — Swin 3D Training Monitor</title>
<style>body{margin:0;background:#111827;color:#f3f4f6;font-family:system-ui,sans-serif;text-align:center}
h2{margin:12px 0 4px}img{max-width:98vw;max-height:90vh;border-radius:8px;box-shadow:0 4px 20px #0008}</style>
</head><body><h2>Mod3D .inv — Live 3D Swin Transformer Training Monitor (auto-refreshes every 5s)</h2>
<img src="training_monitor.png" alt="Waiting for Epoch 1..."></body></html>""")

    def update(self, rec, vx, vtrue, vpred, vclean, vz):
        self.hist.append(rec)
        with open(os.path.join(self.save_dir, "training_history.json"), "w") as f:
            json.dump(self.hist, f, indent=2)

        ep = [r["epoch"] for r in self.hist]
        self.fig.clf()
        ax = self.fig.subplots(2, 3)

        # 1. Losses
        ax[0, 0].plot(ep, [r["train_loss"] for r in self.hist], "o-", label="Train Total")
        ax[0, 0].plot(ep, [r["val_loss"] for r in self.hist], "s-", label="Val Total")
        ax[0, 0].plot(ep, [r["data_rel_misfit"] for r in self.hist], "^--", label="Val Data Misfit ||Gm-d||/||d||")
        ax[0, 0].set_title("Loss & Relative Gravity Data Misfit")
        ax[0, 0].set_xlabel("Epoch")
        ax[0, 0].grid(True, alpha=0.3)
        ax[0, 0].legend(fontsize=8)

        # 2. 3D IoU & Amplitude Recovery
        ax[0, 1].plot(ep, [r["iou"] for r in self.hist], "o-", color="#10b981", label="3D Volumetric IoU")
        ax[0, 1].plot(ep, [r["amp_ratio"] for r in self.hist], "s-", color="#3b82f6", label="Amplitude Ratio")
        ax[0, 1].plot(ep, [1.0 - r["body_l1_rel"] for r in self.hist], "^--", color="#f59e0b", label="1 - Body Rel L1")
        ax[0, 1].set_ylim(0.0, 1.05)
        ax[0, 1].set_title("3D Body IoU & Amplitude Recovery")
        ax[0, 1].set_xlabel("Epoch")
        ax[0, 1].grid(True, alpha=0.3)
        ax[0, 1].legend(fontsize=8)

        # 3. Depth Centroid Error
        ax[0, 2].plot(ep, [r["centroid_dz"] for r in self.hist], "o-", color="#ef4444", label="|dz centroid| [m]")
        ax[0, 2].axhline(CELL, color="#6b7280", linestyle=":", label=f"1 cell ({CELL:.0f} m)")
        ax[0, 2].set_title("3D Depth Centroid Error [m]")
        ax[0, 2].set_xlabel("Epoch")
        ax[0, 2].set_ylabel("Metres")
        ax[0, 2].grid(True, alpha=0.3)
        ax[0, 2].legend(fontsize=8)

        # Pick validation sample 0 and slice through its peak |truth| column
        t_vol = vtrue[0, 0].detach().cpu().numpy()                 # (nz, nx, ny)
        p_vol = vpred[0, 0].detach().cpu().numpy()
        obs_map = vx[0, 0].detach().cpu().numpy()                  # (nx, ny)
        terr_m = vx[0, 1].detach().cpu().numpy() * (NZ * CELL)     # terrain depth [m]
        iz_pk, ix_pk, iy_pk = np.unravel_index(np.argmax(np.abs(t_vol)), t_vol.shape)
        x_km = np.arange(NX) * CELL / 1000.0
        y_km = np.arange(NY) * CELL / 1000.0
        z_m = np.arange(NZ) * CELL

        # 4. Surface Draped Gravity Anomaly Map
        vlim_g = max(1e-3, float(np.abs(obs_map).max()))
        im0 = ax[1, 0].imshow(obs_map.T, origin="lower", extent=[x_km[0], x_km[-1], y_km[0], y_km[-1]],
                              cmap="RdBu_r", vmin=-vlim_g, vmax=vlim_g, aspect="equal")
        ax[1, 0].axhline(y_km[iy_pk], color="k", linestyle="--", linewidth=1.0, label=f"Profile Y={y_km[iy_pk]:.2f} km")
        ax[1, 0].set_title(f"Val #0 Draped Gravity gz [mGal] (max {vlim_g:.2f})")
        ax[1, 0].set_xlabel("X [km]")
        ax[1, 0].set_ylabel("Y [km]")
        ax[1, 0].legend(loc="upper right", fontsize=8)
        self.fig.colorbar(im0, ax=ax[1, 0], fraction=0.046)

        # 5 & 6. True vs Predicted X-Z Section under Terrain Relief
        vmax_r = max(0.1, float(max(np.abs(t_vol[:, :, iy_pk]).max(), np.abs(p_vol[:, :, iy_pk]).max())))
        for col_idx, (sec, title) in enumerate([(t_vol[:, :, iy_pk], "Ground Truth Density [g/cm³]"),
                                                (p_vol[:, :, iy_pk], f"Swin-3D Prediction (Epoch {rec['epoch']})")]):
            a = ax[1, 1 + col_idx]
            im = a.imshow(sec, origin="upper", extent=[x_km[0], x_km[-1], z_m[-1] + CELL, 0.0],
                          cmap="coolwarm", vmin=-vmax_r, vmax=vmax_r, aspect="auto")
            a.fill_between(x_km, 0.0, terr_m[:, iy_pk], color="#d1d5db", alpha=0.85, label="Air above relief")
            a.plot(x_km, terr_m[:, iy_pk], color="#111827", linewidth=1.5, label="Terrain surface")
            a.set_title(title)
            a.set_xlabel("X [km]")
            a.set_ylabel("Depth below grid top [m]")
            a.legend(loc="lower right", fontsize=8)
            self.fig.colorbar(im, ax=a, fraction=0.046)

        out_png = os.path.join(self.save_dir, "training_monitor.png")
        self.fig.savefig(out_png, dpi=130)
        if self.gui:
            self.fig.canvas.draw_idle()
            self.plt.pause(0.05)


def main():
    args = parse_args()
    os.makedirs(args.save_dir, exist_ok=True)
    dev = args.device
    grid = (NX, NY, NZ, CELL, CELL, CELL)
    depth_total = NZ * CELL
    print(f"Device {dev} | {args.epochs} epochs x {args.steps} steps x batch {args.batch_size} | "
          f"grid {NX}x{NY}x{NZ} @ {CELL} m | ground clearance 0.30-0.40 m")
    print(f"Monitor: {os.path.join(args.save_dir, 'training_monitor.png')} "
          f"| HTML: {os.path.join(args.save_dir, 'dashboard.html')} | GUI window: {'ON' if args.gui else 'OFF (pass --gui)'}")

    monitor = TrainingMonitor(args.save_dir, gui=args.gui)
    op = TerrainGravityOperator(*grid).to(dev)
    train_ds = BodyTerrainDataset(args.steps * args.batch_size, *grid)
    loader = DataLoader(train_ds, batch_size=args.batch_size, num_workers=args.num_workers,
                        persistent_workers=args.num_workers > 0, drop_last=True)
    vx, vtrue, vclean, vz = make_fixed_set(args.n_val, 2024, op, dev, grid)

    model = PotentialFieldTransformer3D(NX, NY, NZ, **ARCH).to(dev)
    print(f"Parameters: {sum(p.numel() for p in model.parameters()) / 1e6:.2f} M")
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    total_steps = args.epochs * args.steps
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=args.lr, total_steps=total_steps, pct_start=0.05)
    crit = CompoundInversionLoss(lambda_data=args.lambda_data, lambda_ms=args.lambda_ms, forward_op=op)
    start_epoch, best = 1, float("inf")
    if args.resume:
        ck = torch.load(args.resume, map_location="cpu")
        model.load_state_dict(ck["model_state_dict"])
        print(f"Resumed weights from {args.resume} (epoch {ck.get('epoch')})")

    def validate():
        model.eval()
        with torch.no_grad():
            pred = torch.cat([model(vx[i:i + 8]) for i in range(0, len(vx), 8)])
            vloss, _ = crit(pred, vtrue, vclean, vz)
        model.train()
        return float(vloss), compute_metrics(pred, vtrue, vclean, vz, op, CELL), pred

    zero = compute_metrics(torch.zeros_like(vtrue), vtrue, vclean, vz, op, CELL)
    print("Zero predictor: " + " ".join(f"{k} {v:.3f}" for k, v in zero.items()))

    for epoch in range(start_epoch, args.epochs + 1):
        t0, acc, n = time.time(), {}, 0
        for rho, t, c, nf, _ in loader:
            rho, t, c, nf = rho.to(dev), t.to(dev), c.to(dev), nf.to(dev)
            obs, clean, z_obs = simulate_batch(op, rho, t, c, nf)
            x = torch.cat([obs, terrain_channel(t, depth_total)], dim=1)
            pred = model(x)
            loss, m = crit(pred, rho, clean, z_obs)
            opt.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            sched.step()
            for k, v in m.items():
                acc[k] = acc.get(k, 0.0) + v
            n += 1
        vloss, vm, vpred = validate()
        tr = " ".join(f"{k[5:]} {v / n:.3f}" for k, v in acc.items())
        print(f"Epoch {epoch:02d}/{args.epochs} [{time.time() - t0:.0f}s] train {tr} | val loss {vloss:.3f} "
              f"L1 {vm['l1_rel']:.3f} bodyL1 {vm['body_l1_rel']:.3f} data {vm['data_rel_misfit']:.3f} "
              f"IoU {vm['iou']:.2f} dz {vm['centroid_dz']:.0f} m amp {vm['amp_ratio']:.2f}", flush=True)
        ck = {"epoch": epoch, "model_state_dict": model.state_dict(), "val_loss": vloss, "metrics": vm,
              "grid": {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL}, "arch": ARCH}
        torch.save(ck, os.path.join(args.save_dir, "last_swin3d.pt"))
        if vloss < best:
            best = vloss
            torch.save(ck, os.path.join(args.save_dir, "best_swin3d.pt"))
        monitor.update({"epoch": epoch, "train_loss": acc["loss_total"] / n, "val_loss": vloss, **vm},
                       vx, vtrue, vpred, vclean, vz)
    print(f"Done. Best val loss {best:.4f}. Checkpoint: {os.path.join(args.save_dir, 'best_swin3d.pt')}")


if __name__ == "__main__":
    main()
