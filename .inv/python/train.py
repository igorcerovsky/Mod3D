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
import os
import time

import numpy as np
import torch
from torch.utils.data import DataLoader

from dataset import (
    ASPECT_MULTI,
    CELL,
    CLEARANCE,
    CLEARANCE_MULTI,
    NX,
    NY,
    NZ,
    BodyTerrainDataset,
    augment_d4,
    build_v5_input,
    simulate_batch,
    terrain_channel,
)
from evaluate import compute_metrics, make_category_showcase, make_fixed_set
from forward import TerrainGravityOperator
from losses import CompoundInversionLoss
from model_swin3d import (
    PotentialFieldTransformer3D,
    PotentialFieldTransformer3DV3,
    PotentialFieldTransformer3DV4,
    PotentialFieldTransformer3DV5,
    create_model,
)
from dashboard import TrainingMonitor

ARCH_BY_VARIANT = {
    "v5": {"variant": "v5", "embed_dim": 128, "num_heads": 4, "depth": 6, "ch_mid": 40, "ch_vox": 24,
           "n_sub": 3, "num_classes": 7, "dropout": 0.05},
    "v4": {"variant": "v4", "embed_dim": 128, "num_heads": 4, "depth": 6, "ch_mid": 40, "ch_vox": 24,
           "n_sub": 3, "num_classes": 7},
    "v3": {"variant": "v3", "embed_dim": 128, "num_heads": 4, "depth": 6, "ch_mid": 40, "ch_vox": 24,
           "n_sub": 3, "num_classes": 7},
    "v2": {"variant": "v2", "embed_dim": 128, "num_heads": 4, "depth": 6},
    "v1": {"variant": "v1", "embed_dim": 96, "num_heads": 4, "depth": 4},
}
ARCH = ARCH_BY_VARIANT["v5"]


class BatchAspectOperator:
    """Dispatches forward modeling across per-sample cell aspect ratios inside a batch."""

    def __init__(self, op_bank, canonical_op):
        self.op_bank = list(op_bank)
        self.canonical_op = canonical_op
        self.aspect_indices = None

    def __call__(self, rho, z_obs, level_stride=1):
        idx = self.aspect_indices
        if idx is None:
            return self.canonical_op(rho, z_obs, level_stride=level_stride)
        if (idx == idx[0]).all():
            return self.op_bank[int(idx[0].item())](rho, z_obs, level_stride=level_stride)
        out = torch.empty(
            (rho.shape[0], 1, rho.shape[3], rho.shape[4]),
            device=rho.device,
            dtype=rho.dtype,
        )
        for ai, op_i in enumerate(self.op_bank):
            m = idx == ai
            if m.any():
                out[m] = op_i(rho[m], z_obs[m], level_stride=level_stride)
        return out


class ModelEMA:
    """Exponential moving average weights with early-step decay warmup."""

    def __init__(self, model: torch.nn.Module, decay: float, create_fn, initial_updates: int = 0):
        self.decay = float(decay)
        self.updates = int(initial_updates)
        self.ema = create_fn().eval() if self.decay > 0.0 else None
        if self.ema is not None:
            self.ema.load_state_dict(model.state_dict())
            for p in self.ema.parameters():
                p.requires_grad_(False)

    @torch.no_grad()
    def update(self, model: torch.nn.Module) -> None:
        if self.ema is None:
            return
        self.updates += 1
        d = min(self.decay, (1.0 + self.updates) / (10.0 + self.updates))
        for ev, mv in zip(self.ema.state_dict().values(), model.state_dict().values()):
            if ev.is_floating_point():
                ev.mul_(d).add_(mv.detach(), alpha=1.0 - d)
            else:
                ev.copy_(mv)


def parse_args():
    p = argparse.ArgumentParser(description="Train the 3D gravity-inversion transformer")
    p.add_argument("--epochs", type=int, default=20)
    p.add_argument("--steps", type=int, default=200, help="optimizer steps per epoch")
    p.add_argument("--batch-size", type=int, default=8)
    p.add_argument("--lr", type=float, default=5e-4)
    p.add_argument("--n-val", type=int, default=64)
    p.add_argument("--hard-prob", type=float, default=0.50,
                   help="fraction of training samples using harder/larger composite & contact scenes [0.0-1.0]")
    p.add_argument("--num-workers", type=int, default=0)
    p.add_argument("--lambda-data", type=float, default=0.25)
    p.add_argument("--lambda-ms", type=float, default=0.05)
    p.add_argument("--lambda-cls", type=float, default=0.25, help="lithology cross-entropy weight")
    p.add_argument("--depth-boost", type=float, default=0.5, help="depth sensitivity ramp in loss [0.0 = uniform]")
    p.add_argument("--ema-decay", type=float, default=0.995, help="EMA decay for eval/best weights (0 disables)")
    p.add_argument("--save-dir", type=str, default="./checkpoints")
    p.add_argument("--vol3d-stride", type=int, default=10,
                   help="save full 3D prediction volume snapshot every N epochs in addition to best/latest (0 = best+latest only)")
    p.add_argument("--variant", type=str, default="v5", choices=["v5", "v4", "v3", "v2", "v1"],
                   help="model architecture variant (default: v5)")
    p.add_argument("--dropout", type=float, default=0.05, help="Dropout3d/MLP rate for v5")
    p.add_argument("--fault-prob", type=float, default=0.40, help="v5 dipping fault cut probability")
    p.add_argument("--overburden-prob", type=float, default=0.45, help="v5 weathered regolith probability")
    p.add_argument("--trend-prob", type=float, default=0.40, help="v5 regional trend + correlated noise prob")
    p.add_argument("--dropout-prob", type=float, default=0.35, help="v5 missing-station infill prob")
    p.add_argument("--no-amp", action="store_true", help="disable bfloat16 autocast on MPS/CUDA")
    p.add_argument("--resume", type=str, default="", help="continue from a checkpoint")
    p.add_argument("--gui", action="store_true", help="open a live native GUI window with training graphs")
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    p.add_argument("--device", type=str, default=default_dev)
    return p.parse_args()


def main():
    args = parse_args()
    os.makedirs(args.save_dir, exist_ok=True)
    dev = args.device
    hard_prob = min(1.0, max(0.0, float(args.hard_prob)))
    grid = (NX, NY, NZ, CELL, CELL, CELL)
    depth_total = NZ * CELL

    arch = dict(ARCH_BY_VARIANT[args.variant])
    if args.variant == "v5":
        arch["dropout"] = float(args.dropout)
    start_epoch, best, ck_resumed = 1, float("inf"), None
    if args.resume:
        ck_resumed = torch.load(args.resume, map_location="cpu")
        arch = ck_resumed.get("arch", {"variant": "v1", "embed_dim": 96, "num_heads": 4, "depth": 4})
        if "epoch" in ck_resumed:
            start_epoch = ck_resumed["epoch"] + 1
        if "val_loss" in ck_resumed:
            best = ck_resumed["val_loss"]
        print(f"Resumed weights from {args.resume} (checkpoint epoch {ck_resumed.get('epoch')}, starting epoch {start_epoch})")

    variant = arch.get("variant", "v1")
    is_v5 = variant == "v5"
    amp_dev = "cuda" if dev.startswith("cuda") else ("mps" if dev.startswith("mps") else "")
    use_amp = (not args.no_amp) and bool(amp_dev)
    fault_prob = float(args.fault_prob) if is_v5 else 0.0
    ob_prob = float(args.overburden_prob) if is_v5 else 0.0
    tr_prob = float(args.trend_prob) if is_v5 else 0.0
    do_prob = float(args.dropout_prob) if is_v5 else 0.0
    clear_rng = CLEARANCE_MULTI if is_v5 else CLEARANCE
    asp_list = ASPECT_MULTI if is_v5 else None

    print(f"Device {dev} (AMP bfloat16={'ON' if use_amp else 'OFF'}) | {args.epochs} epochs x {args.steps} steps x batch {args.batch_size} | "
          f"grid {NX}x{NY}x{NZ} @ {CELL} m | hard-prob {hard_prob:.2f} | "
          f"{'clearance 0.30-80m, multi-aspect' if is_v5 else 'ground clearance 0.30-0.40 m'}")
    print(f"Monitor: {os.path.join(args.save_dir, 'training_monitor.png')} "
          f"| HTML: {os.path.join(args.save_dir, 'dashboard.html')} | GUI window: {'ON' if args.gui else 'OFF (pass --gui)'}")

    if is_v5:
        op_bank = [TerrainGravityOperator(NX, NY, NZ, CELL, CELL, CELL * a, h_min=-85.0).to(dev) for a in asp_list]
        op = op_bank[asp_list.index(1.0)] if 1.0 in asp_list else TerrainGravityOperator(*grid, h_min=-85.0).to(dev)
        batch_op = BatchAspectOperator(op_bank, op)
    else:
        op_bank = None
        op = TerrainGravityOperator(*grid).to(dev)
        batch_op = op

    monitor = TrainingMonitor(args.save_dir, gui=args.gui, vol3d_stride=args.vol3d_stride, forward_op=op)
    train_ds = BodyTerrainDataset(
        args.steps * args.batch_size, *grid, hard_prob=hard_prob,
        fault_prob=fault_prob, clearance_range=clear_rng, aspect_ratios=asp_list,
    )
    loader = DataLoader(train_ds, batch_size=args.batch_size, num_workers=args.num_workers,
                        persistent_workers=args.num_workers > 0, drop_last=True)

    val_ch = 5 if is_v5 else 2
    vx, vtrue, vclean, vz = make_fixed_set(
        args.n_val, 2024, op, dev, grid, hard_prob=0.0, in_channels=val_ch,
        fault_prob=fault_prob * 0.75, clearance_range=clear_rng,
        overburden_prob=ob_prob * 0.75, trend_prob=tr_prob * 0.75, dropout_prob=do_prob * 0.75,
    )
    hard_val = None
    if hard_prob > 0.0:
        n_hard = max(8, args.n_val // 2)
        hard_val = make_fixed_set(
            n_hard, 3024, op, dev, grid, hard_prob=1.0, in_channels=val_ch,
            fault_prob=fault_prob, clearance_range=clear_rng,
            overburden_prob=ob_prob, trend_prob=tr_prob, dropout_prob=do_prob,
        )
    show_x, show_true, show_clean, show_z, show_labels = make_category_showcase(
        op, dev, grid, seed=4024, in_channels=val_ch,
    )
    if hard_val is not None:
        mon_labels = ("Std",) * min(4, len(vx)) + ("Hard",) * min(4, len(hard_val[0])) + show_labels
    else:
        mon_labels = ("Std",) * min(8, len(vx)) + show_labels

    model = create_model(NX, NY, NZ, **arch).to(dev)
    if ck_resumed is not None:
        model.load_state_dict(ck_resumed.get("raw_model_state_dict", ck_resumed["model_state_dict"]))
    init_updates = (start_epoch - 1) * args.steps if args.resume else 0
    ema = ModelEMA(model, args.ema_decay, lambda: create_model(NX, NY, NZ, **arch).to(dev), initial_updates=init_updates)
    if ck_resumed is not None and ema.ema is not None and "ema_state_dict" in ck_resumed:
        ema.ema.load_state_dict(ck_resumed["ema_state_dict"])
    eval_model = ema.ema if ema.ema is not None else model
    forward_with_aux = ((lambda m, inp: m(inp, return_aux=True)) if variant in ("v2", "v3", "v4", "v5")
                        else (lambda m, inp: (m(inp), None)))

    print(f"Architecture: {variant} | Parameters: {sum(p.numel() for p in model.parameters()) / 1e6:.2f} M")
    crit = CompoundInversionLoss(lambda_data=args.lambda_data, lambda_ms=args.lambda_ms,
                                 lambda_cls=args.lambda_cls, depth_boost=args.depth_boost,
                                 forward_op=batch_op)

    end_epoch = start_epoch + args.epochs - 1
    total_steps = max(1, (end_epoch - start_epoch + 1) * args.steps)
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    if args.resume:
        sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=total_steps, eta_min=args.lr * 0.02)
    else:
        pct_start = min(0.3, max(0.05, 2.0 / total_steps)) if total_steps > 2 else 0.5
        sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=args.lr, total_steps=total_steps,
                                                    pct_start=pct_start, div_factor=10.0, final_div_factor=50.0)

    def _eval_split(xs, ys, cs, zs):
        if is_v5:
            batch_op.aspect_indices = None
        preds, aux_chunks = [], []
        with torch.no_grad():
            for i in range(0, len(xs), 8):
                p_i, a_i = forward_with_aux(eval_model, xs[i:i + 8])
                preds.append(p_i)
                if a_i is not None:
                    aux_chunks.append(a_i)
            pred = torch.cat(preds)
            vaux = ({k: torch.cat([c[k] for c in aux_chunks])
                     if isinstance(aux_chunks[0][k], torch.Tensor) else aux_chunks[0][k]
                     for k in aux_chunks[0]} if aux_chunks else None)
            sloss, _ = crit(pred, ys, cs, zs, aux=vaux)
        return float(sloss), compute_metrics(pred, ys, cs, zs, op, CELL), pred

    def _pack_monitor(parts):
        xs, ts, ps, cs, zs = zip(*parts)
        return tuple(torch.cat(seq, dim=0) for seq in (xs, ts, ps, cs, zs))

    def validate():
        eval_model.eval()
        std_loss, std_m, std_pred = _eval_split(vx, vtrue, vclean, vz)
        with torch.no_grad():
            show_pred, _ = forward_with_aux(eval_model, show_x)
        show_part = (show_x[:, :2], show_true, show_pred, show_clean, show_z)
        if hard_val is not None:
            vhx, vhtrue, vhclean, vhz = hard_val
            hard_loss, hard_m, hard_pred = _eval_split(vhx, vhtrue, vhclean, vhz)
            if eval_model is model:
                model.train()
            comb_loss = (1.0 - hard_prob) * std_loss + hard_prob * hard_loss
            comb_m = {}
            for k in std_m:
                vs, vh = std_m[k], hard_m[k]
                if np.isnan(vs) and np.isnan(vh):
                    comb_m[k] = float("nan")
                elif np.isnan(vs):
                    comb_m[k] = vh
                elif np.isnan(vh):
                    comb_m[k] = vs
                else:
                    comb_m[k] = (1.0 - hard_prob) * vs + hard_prob * vh
            n_s = min(4, len(vx), len(std_pred))
            n_h = min(4, len(vhx), len(hard_pred))
            mon_tensors = _pack_monitor([
                (vx[:n_s, :2], vtrue[:n_s], std_pred[:n_s], vclean[:n_s], vz[:n_s]),
                (vhx[:n_h, :2], vhtrue[:n_h], hard_pred[:n_h], vhclean[:n_h], vhz[:n_h]),
                show_part,
            ])
            return comb_loss, comb_m, std_loss, std_m, hard_loss, hard_m, mon_tensors
        if eval_model is model:
            model.train()
        n_s = min(8, len(vx), len(std_pred))
        mon_tensors = _pack_monitor([
            (vx[:n_s, :2], vtrue[:n_s], std_pred[:n_s], vclean[:n_s], vz[:n_s]),
            show_part,
        ])
        return std_loss, std_m, std_loss, std_m, None, None, mon_tensors

    if args.resume:
        best, *_ = validate()

    zero = compute_metrics(torch.zeros_like(vtrue), vtrue, vclean, vz, op, CELL)
    print("Zero predictor (std): " + " ".join(f"{k} {v:.3f}" for k, v in zero.items()))
    if hard_val is not None:
        vhx, vhtrue, vhclean, vhz = hard_val
        zero_h = compute_metrics(torch.zeros_like(vhtrue), vhtrue, vhclean, vhz, op, CELL)
        print("Zero predictor (hard): " + " ".join(f"{k} {v:.3f}" for k, v in zero_h.items()))

    for epoch in range(start_epoch, end_epoch + 1):
        t0, acc, n = time.time(), {}, 0
        for batch in loader:
            if is_v5:
                rho, t, c, nf, _, asp_idx = batch
                rho, t, c, nf = rho.to(dev), t.to(dev), c.to(dev), nf.to(dev)
                asp_idx = asp_idx.to(dev)
                b = rho.shape[0]
                obs = torch.empty((b, 1, NX, NY), device=dev, dtype=rho.dtype)
                clean = torch.empty((b, 1, NX, NY), device=dev, dtype=rho.dtype)
                z_obs = torch.empty((b, NX, NY), device=dev, dtype=rho.dtype)
                vm = torch.ones((b, 1, NX, NY), device=dev, dtype=rho.dtype)
                for ai, op_i in enumerate(op_bank):
                    m_a = asp_idx == ai
                    if m_a.any():
                        o_a, c_a, z_a, v_a = simulate_batch(
                            op_i, rho[m_a], t[m_a], c[m_a], nf[m_a],
                            overburden_prob=ob_prob, trend_prob=tr_prob,
                            dropout_prob=do_prob, return_mask=True,
                        )
                        obs[m_a], clean[m_a], z_obs[m_a], vm[m_a] = o_a, c_a, z_a, v_a
                rho, t, obs, clean, z_obs, vm = augment_d4(rho, t, obs, clean, z_obs, valid_mask=vm)
                asp_vals = torch.tensor([asp_list[int(i)] for i in asp_idx.tolist()], device=dev, dtype=rho.dtype)
                depth_totals = (NZ * CELL) * asp_vals.view(b, 1, 1)
                x = build_v5_input(obs, t, depth_totals, valid_mask=vm, aspect=asp_vals, clearance=c)
                batch_op.aspect_indices = asp_idx
            else:
                rho, t, c, nf, _ = batch
                rho, t, c, nf = rho.to(dev), t.to(dev), c.to(dev), nf.to(dev)
                obs, clean, z_obs = simulate_batch(op, rho, t, c, nf)
                rho, t, obs, clean, z_obs = augment_d4(rho, t, obs, clean, z_obs)
                x = torch.cat([obs, terrain_channel(t, depth_total)], dim=1)

            with torch.amp.autocast(device_type=amp_dev or "cpu", dtype=torch.bfloat16, enabled=use_amp):
                pred, aux = forward_with_aux(model, x)
            if use_amp:
                pred = pred.float()
                if aux is not None:
                    aux = {k: (v.float() if isinstance(v, torch.Tensor) else v) for k, v in aux.items()}
            loss, m = crit(pred, rho, clean, z_obs, aux=aux)
            opt.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            ema.update(model)
            sched.step()
            for k, v in m.items():
                acc[k] = acc.get(k, 0.0) + v
            n += 1
        vloss, vm, std_loss, std_m, hard_loss, hard_m, mon_t = validate()
        tr = " ".join(f"{k[5:]} {v / n:.3f}" for k, v in acc.items())
        msg = (f"Epoch {epoch:02d}/{end_epoch} [{time.time() - t0:.0f}s] train {tr} | "
               f"val loss {std_loss:.3f} L1 {std_m['l1_rel']:.3f} bodyL1 {std_m['body_l1_rel']:.3f} "
               f"data {std_m['data_rel_misfit']:.3f} IoU {std_m['iou']:.2f} dz {std_m['centroid_dz']:.0f} m "
               f"amp {std_m['amp_ratio']:.2f} halo {std_m['halo_opp']:.3f}")
        if hard_m is not None:
            msg += (f" | hard loss {hard_loss:.3f} data {hard_m['data_rel_misfit']:.3f} "
                    f"IoU {hard_m['iou']:.2f} dz {hard_m['centroid_dz']:.0f} m amp {hard_m['amp_ratio']:.2f}")
        print(msg, flush=True)
        ck = {"epoch": epoch, "model_state_dict": eval_model.state_dict(),
              "raw_model_state_dict": model.state_dict(), "val_loss": vloss, "metrics": std_m,
              "hard_prob": hard_prob,
              "grid": {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL}, "arch": arch}
        if hard_m is not None:
            ck["val_std_loss"] = std_loss
            ck["val_hard_loss"] = hard_loss
            ck["hard_metrics"] = hard_m
        if ema.ema is not None:
            ck["ema_state_dict"] = ema.ema.state_dict()
        torch.save(ck, os.path.join(args.save_dir, "last_swin3d.pt"))
        is_best = False
        if vloss < best:
            best = vloss
            is_best = True
            torch.save(ck, os.path.join(args.save_dir, "best_swin3d.pt"))
        rec_mon = {"epoch": epoch, "train_loss": acc["loss_total"] / n, "val_loss": vloss, **vm}
        if hard_m is not None:
            rec_mon["val_std_loss"] = std_loss
            rec_mon["val_hard_loss"] = hard_loss
            rec_mon["hard_iou"] = hard_m["iou"]
            rec_mon["hard_dz"] = hard_m["centroid_dz"]
        monitor.update(rec_mon, *mon_t, sample_labels=mon_labels, is_best=is_best)
    print(f"Done. Best val loss {best:.4f}. Checkpoint: {os.path.join(args.save_dir, 'best_swin3d.pt')}")


if __name__ == "__main__":
    main()
