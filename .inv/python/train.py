"""Training Pipeline for 3D Potential Field Swin Transformer with Zhdanov Focusing.

Usage:
    python train.py --epochs 50 --batch-size 8 --lr 1e-4 --save-dir ./checkpoints
"""

import argparse
import os
import torch
from torch.utils.data import DataLoader
from dataset import SyntheticPotentialFieldDataset
from model_swin3d import PotentialFieldTransformer3D
from losses import CompoundInversionLoss


def parse_args():
    parser = argparse.ArgumentParser(description="Train 3D Swin Potential Field Inversion Transformer")
    parser.add_argument("--epochs", type=int, default=30, help="Number of training epochs")
    parser.add_argument("--batch-size", type=int, default=8, help="Batch size")
    parser.add_argument("--lr", type=float, default=2e-4, help="Learning rate")
    parser.add_argument("--n-samples", type=int, default=2000, help="Number of synthetic samples")
    parser.add_argument("--save-dir", type=str, default="./checkpoints", help="Directory to save checkpoints")
    parser.add_argument("--device", type=str, default="cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu"))
    return parser.parse_args()


def main():
    args = parse_args()
    os.makedirs(args.save_dir, exist_ok=True)

    print("==================================================================")
    print("  .inv: Training 3D Potential Field Inversion Transformer         ")
    print(f"  Device: {args.device} | Epochs: {args.epochs} | Batch size: {args.batch_size}")
    print("==================================================================")

    # 1. Dataset & DataLoader
    print("[1] Initializing Procedural 3D Geology Dataset...")
    dataset = SyntheticPotentialFieldDataset(n_samples=args.n_samples, nx=64, ny=64, nz=32)
    loader = DataLoader(dataset, batch_size=args.batch_size, shuffle=True, num_workers=0)

    # 2. Model
    print("[2] Initializing 3D Swin Transformer...")
    model = PotentialFieldTransformer3D(in_channels=1, out_depth=32, embed_dim=96, num_heads=4)
    model.to(args.device)

    # 3. Optimizer & Loss
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    criterion = CompoundInversionLoss(lambda_sup=1.0, lambda_data=0.0, lambda_ms=0.04, lambda_mgs=0.02)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs, eta_min=1e-6)

    # 4. Training Loop
    print("[3] Starting Training Loop...")
    best_loss = float("inf")

    for epoch in range(1, args.epochs + 1):
        model.train()
        epoch_loss = 0.0
        epoch_ms = 0.0
        epoch_mgs = 0.0

        for i, (obs_field, density_true) in enumerate(loader):
            obs_field = obs_field.to(args.device)
            density_true = density_true.to(args.device)

            optimizer.zero_grad()
            density_pred = model(obs_field)

            loss, metrics = criterion(density_pred, density_true, epoch=epoch, total_epochs=args.epochs)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            optimizer.step()

            epoch_loss += metrics["loss_total"]
            epoch_ms += metrics["loss_ms"]
            epoch_mgs += metrics["loss_mgs"]

        scheduler.step()
        n_batches = len(loader)
        avg_loss = epoch_loss / n_batches
        avg_ms = epoch_ms / n_batches
        avg_mgs = epoch_mgs / n_batches

        print(f"Epoch [{epoch:02d}/{args.epochs:02d}] "
              f"Loss: {avg_loss:.4f} | S_MS: {avg_ms:.4f} | S_MGS: {avg_mgs:.4f} | "
              f"Focus e: {metrics['annealed_e']:.4f} | LR: {scheduler.get_last_lr()[0]:.2e}")

        # Save best model checkpoint
        if avg_loss < best_loss:
            best_loss = avg_loss
            ckpt_path = os.path.join(args.save_dir, "best_swin3d.pt")
            torch.save({
                "epoch": epoch,
                "model_state_dict": model.state_dict(),
                "optimizer_state_dict": optimizer.state_dict(),
                "loss": best_loss,
            }, ckpt_path)

    print(f"\n[4] Training Complete! Best Checkpoint saved to: {os.path.join(args.save_dir, 'best_swin3d.pt')}")


if __name__ == "__main__":
    main()
