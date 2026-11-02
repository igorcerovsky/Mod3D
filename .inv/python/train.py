"""Training pipeline for the 3D potential-field Swin Transformer (physics + Zhdanov focusing).

Usage:
    python train.py --epochs 30 --batch-size 8 --lr 2e-4 --save-dir ./checkpoints
    python train.py --epochs 2 --n-samples 64 --nx 32 --ny 32 --nz 16      # quick smoke test

Grid sizes must be multiples of 16 horizontally (4x4 patches, x4 upsampling) and of 4 in depth.
Training samples are generated on the fly (Option A); the validation set is a FIXED seeded set so
epochs are comparable. The best checkpoint is chosen by validation L1 (not by the training loss,
whose focusing term changes with the annealed e). A zero-predictor baseline is printed for context.
"""

import argparse
import os

import torch
from torch.utils.data import DataLoader
from tqdm import tqdm

from dataset import SyntheticPotentialFieldDataset
from evaluate import compute_metrics
from forward import GravityFFTOperator
from losses import CompoundInversionLoss
from model_swin3d import PotentialFieldTransformer3D


def parse_args():
    p = argparse.ArgumentParser(description="Train 3D Swin Potential Field Inversion Transformer")
    p.add_argument("--epochs", type=int, default=30)
    p.add_argument("--batch-size", type=int, default=8)
    p.add_argument("--lr", type=float, default=2e-4)
    p.add_argument("--n-samples", type=int, default=2000, help="Synthetic samples per epoch")
    p.add_argument("--n-val", type=int, default=32, help="Fixed validation samples")
    p.add_argument("--num-workers", type=int, default=0)
    p.add_argument("--nx", type=int, default=64)
    p.add_argument("--ny", type=int, default=64)
    p.add_argument("--nz", type=int, default=32)
    p.add_argument("--spacing", type=float, default=50.0, help="Cell size dx=dy=dz [m]")
    p.add_argument("--lambda-data", type=float, default=1.0)
    p.add_argument("--lambda-ms", type=float, default=0.02)
    p.add_argument("--lambda-mgs", type=float, default=0.01)
    p.add_argument("--save-dir", type=str, default="./checkpoints")
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    p.add_argument("--device", type=str, default=default_dev)
    return p.parse_args()


def main():
    args = parse_args()
    os.makedirs(args.save_dir, exist_ok=True)
    s = args.spacing
    grid = {"nx": args.nx, "ny": args.ny, "nz": args.nz, "dx": s, "dy": s, "dz": s}

    print("=" * 66)
    print("  .inv: Training 3D Potential Field Inversion Transformer")
    print(f"  Device: {args.device} | Epochs: {args.epochs} | Batch: {args.batch_size} | Grid: {args.nx}x{args.ny}x{args.nz} @ {s} m")
    print("  Data: on-the-fly procedural geology + physical Nagy-prism FFT forward (Option A)")
    print("=" * 66)

    op = GravityFFTOperator(args.nx, args.ny, args.nz, s, s, s).to(args.device)
    train_ds = SyntheticPotentialFieldDataset(args.n_samples, args.nx, args.ny, args.nz, s, s, s,
                                              operator=GravityFFTOperator(args.nx, args.ny, args.nz, s, s, s))
    loader = DataLoader(train_ds, batch_size=args.batch_size, shuffle=True,
                        num_workers=args.num_workers, pin_memory=(args.device == "cuda"))
    val_ds = SyntheticPotentialFieldDataset(args.n_val, args.nx, args.ny, args.nz, s, s, s, seed=2024,
                                            operator=train_ds.operator)
    val_obs = torch.stack([val_ds[i][0] for i in range(len(val_ds))]).to(args.device)
    val_true = torch.stack([val_ds[i][1] for i in range(len(val_ds))]).to(args.device)
    spacing = (s, s, s)

    model = PotentialFieldTransformer3D(in_channels=1, out_depth=args.nz, embed_dim=96, num_heads=4).to(args.device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs, eta_min=1e-6)
    criterion = CompoundInversionLoss(lambda_sup=1.0, lambda_data=args.lambda_data, lambda_ms=args.lambda_ms,
                                      lambda_mgs=args.lambda_mgs, spacing=spacing, forward_op=op)

    base = compute_metrics(torch.zeros_like(val_true), val_true, val_obs, op, spacing)
    print(f"Zero-predictor baseline on validation set: L1={base['l1']:.4f} data_rel_misfit={base['data_rel_misfit']:.3f}")

    best_val = float("inf")
    for epoch in range(1, args.epochs + 1):
        model.train()
        acc = {"loss_total": 0.0, "loss_sup": 0.0, "loss_data": 0.0, "loss_ms": 0.0, "loss_mgs": 0.0}
        pbar = tqdm(loader, desc=f"Epoch {epoch:02d}/{args.epochs:02d}", leave=False)
        m = {}
        for obs, dens in pbar:
            obs, dens = obs.to(args.device), dens.to(args.device)
            optimizer.zero_grad()
            pred = model(obs)
            loss, m = criterion(pred, dens, d_obs=obs, epoch=epoch, total_epochs=args.epochs)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            optimizer.step()
            for k in acc:
                acc[k] += m[k]
            pbar.set_postfix({"loss": f"{m['loss_total']:.4f}", "data": f"{m['loss_data']:.3f}",
                              "S_MS": f"{m['loss_ms']:.3f}", "e": f"{m['annealed_e']:.3f}"})
        scheduler.step()
        nb = len(loader)

        model.eval()
        with torch.no_grad():
            val_pred = model(val_obs)
        vm = compute_metrics(val_pred, val_true, val_obs, op, spacing)

        print(f"Epoch [{epoch:02d}/{args.epochs:02d}] train: loss {acc['loss_total']/nb:.4f} "
              f"sup {acc['loss_sup']/nb:.4f} data {acc['loss_data']/nb:.3f} "
              f"S_MS {acc['loss_ms']/nb:.3f} S_MGS {acc['loss_mgs']/nb:.3f} e {m['annealed_e']:.4f} | "
              f"val: L1 {vm['l1']:.4f} data {vm['data_rel_misfit']:.3f} IoU {vm['iou']:.2f} "
              f"dz {vm['centroid_dz']:.0f} m peak x{vm['peak_ratio']:.2f} | LR {scheduler.get_last_lr()[0]:.1e}")

        if vm["l1"] < best_val:
            best_val = vm["l1"]
            torch.save({"epoch": epoch, "model_state_dict": model.state_dict(),
                        "optimizer_state_dict": optimizer.state_dict(), "val_l1": best_val, "grid": grid},
                       os.path.join(args.save_dir, "best_swin3d.pt"))

    print(f"\nTraining complete. Best validation L1 = {best_val:.4f} (zero predictor: {base['l1']:.4f}).")
    print(f"Checkpoint: {os.path.join(args.save_dir, 'best_swin3d.pt')}")


if __name__ == "__main__":
    main()
