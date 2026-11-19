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
import inspect
import math
import os
import time

import numpy as np
import torch
from torch.utils.data import DataLoader

from dataset import (
    ASPECT_MULTI,
    CELL,
    CLEARANCE,
    CLEARANCE_AIRBORNE_V6,
    CLEARANCE_CLAMP_V6,
    CLEARANCE_MULTI,
    GROUND_PROB_V6,
    LARGE_BODY_PROB_V6,
    NX,
    NY,
    NZ,
    BodyTerrainDataset,
    augment_d4,
    build_v5_input,
    build_v6_input,
    build_v7_input,
    simulate_batch,
    terrain_channel,
)
from evaluate import (
    compute_calibration_metrics,
    compute_metrics,
    compute_selection_score,
    make_category_showcase,
    make_fixed_set,
    make_v6_val_buckets,
)
from forward import SubvoxelCapOperator, TerrainGravityOperator
from losses import CompoundInversionLoss
from model_swin3d import (
    V5_SIZE_PRESETS,
    PotentialFieldTransformer3D,
    PotentialFieldTransformer3DV3,
    PotentialFieldTransformer3DV4,
    PotentialFieldTransformer3DV5,
    create_model,
)
try:
    from model_swin3d import V6_SIZE_PRESETS
except ImportError:
    V6_SIZE_PRESETS = None
try:
    from model_swin3d import V7_SIZE_PRESETS
except ImportError:
    V7_SIZE_PRESETS = None
try:
    from model_swin3d import V8_SIZE_PRESETS
except ImportError:
    V8_SIZE_PRESETS = None
try:
    from model_swin3d import V9_SIZE_PRESETS
except ImportError:
    V9_SIZE_PRESETS = None
try:
    from model_swin3d import V9_1_SIZE_PRESETS
except ImportError:
    V9_1_SIZE_PRESETS = None
try:
    from model_swin3d import PotentialFieldTransformer3DV7
except ImportError:
    PotentialFieldTransformer3DV7 = None
try:
    from model_swin3d import PotentialFieldTransformer3DV9
except ImportError:
    PotentialFieldTransformer3DV9 = None
from dashboard import TrainingMonitor

ARCH_BY_VARIANT = {
    "v5": dict(V5_SIZE_PRESETS["base"]),
    "v4": {"variant": "v4", "embed_dim": 128, "num_heads": 4, "depth": 6, "ch_mid": 40, "ch_vox": 24,
           "n_sub": 3, "num_classes": 7},
    "v3": {"variant": "v3", "embed_dim": 128, "num_heads": 4, "depth": 6, "ch_mid": 40, "ch_vox": 24,
           "n_sub": 3, "num_classes": 7},
    "v2": {"variant": "v2", "embed_dim": 128, "num_heads": 4, "depth": 6},
    "v1": {"variant": "v1", "embed_dim": 96, "num_heads": 4, "depth": 4},
}
if V6_SIZE_PRESETS is not None:
    ARCH_BY_VARIANT["v6"] = dict(V6_SIZE_PRESETS["base"])
if V7_SIZE_PRESETS is not None:
    ARCH_BY_VARIANT["v7"] = dict(V7_SIZE_PRESETS["base"])
if V8_SIZE_PRESETS is not None:
    ARCH_BY_VARIANT["v8"] = dict(V8_SIZE_PRESETS["base"])
if V9_SIZE_PRESETS is not None:
    ARCH_BY_VARIANT["v9"] = dict(V9_SIZE_PRESETS["base"])
if V9_1_SIZE_PRESETS is not None:
    ARCH_BY_VARIANT["v9.1"] = dict(V9_1_SIZE_PRESETS["base"])
ARCH = ARCH_BY_VARIANT["v5"]

V6_BUCKET_WEIGHTS = {
    "ground_clean": 0.35,
    "ground_hard_large_detrend": 0.35,
    "airborne_relief_drape": 0.15,
    "multi_aspect": 0.15,
}


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


def build_v6_scheduler(opt, total_steps: int, warmup_steps: int, min_lr_frac: float = 0.02):
    """Linear warmup from 5%->100% lr followed by cosine annealing decay to ``min_lr_frac``."""
    total_steps = max(1, int(total_steps))
    warmup_steps = max(0, min(int(warmup_steps), max(0, total_steps - 1)))

    def _lr_lambda(step: int) -> float:
        if warmup_steps > 0 and step < warmup_steps:
            return 0.05 + 0.95 * (float(step + 1) / float(warmup_steps))
        rem = max(1, total_steps - warmup_steps)
        prog = min(1.0, max(0.0, float(step - warmup_steps) / float(rem)))
        return min_lr_frac + (1.0 - min_lr_frac) * 0.5 * (1.0 + math.cos(math.pi * prog))

    return torch.optim.lr_scheduler.LambdaLR(opt, lr_lambda=_lr_lambda)


def build_resume_v6_scheduler(opt, ck_resumed, start_epoch: int, end_epoch: int,
                              steps_per_epoch: int, warmup_epochs: int,
                              total_epochs: int = 0, warm_restart: bool = False,
                              min_lr_frac: float = 0.02):
    """Build v6 scheduler handling multi-resume cumulative horizons and cosine warm restarts."""
    spe = max(1, int(steps_per_epoch))
    ext_steps = max(1, (int(end_epoch) - int(start_epoch) + 1) * spe)
    target_end = max(int(end_epoch), int(total_epochs)) if int(total_epochs) > 0 else int(end_epoch)
    cum_steps = max(1, target_end * spe)
    warm_steps = max(0, int(warmup_epochs)) * spe

    if ck_resumed is None:
        return build_v6_scheduler(opt, total_steps=cum_steps, warmup_steps=warm_steps, min_lr_frac=min_lr_frac)

    prev_sd = ck_resumed.get("sched_state_dict") if isinstance(ck_resumed, dict) else None
    prev_step = (
        int(prev_sd.get("last_epoch", (int(start_epoch) - 1) * spe))
        if isinstance(prev_sd, dict)
        else (int(start_epoch) - 1) * spe
    )
    if warm_restart or not isinstance(prev_sd, dict) or prev_step >= cum_steps - spe:
        wr_warm = min(max(spe, warm_steps // 2 if warm_steps > 0 else spe), max(1, ext_steps // 5))
        return build_v6_scheduler(opt, total_steps=ext_steps, warmup_steps=wr_warm, min_lr_frac=min_lr_frac)

    sched = build_v6_scheduler(opt, total_steps=cum_steps, warmup_steps=warm_steps, min_lr_frac=min_lr_frac)
    try:
        sched.load_state_dict(dict(prev_sd))
        for pg, base_lr, lam in zip(opt.param_groups, sched.base_lrs, sched.lr_lambdas):
            pg["lr"] = base_lr * lam(sched.last_epoch)
        sched._last_lr = [pg["lr"] for pg in opt.param_groups]
    except Exception:
        pass
    return sched


def compute_v6_curriculum(epoch: int, curriculum_epochs: int, hard_prob: float, fault_prob: float,
                          do_prob: float, large_prob: float, ground_prob: float):
    """Compute epoch-wise v6 dataset and physics curriculum schedule."""
    if curriculum_epochs <= 0:
        tau = 1.0
    else:
        tau = min(1.0, max(0.0, (epoch - 1) / max(1, curriculum_epochs - 1)))
    cur_hard = min(hard_prob, 0.15 + tau * max(0.0, hard_prob - 0.15)) if hard_prob > 0 else 0.0
    cur_fault = min(fault_prob, 0.10 + tau * max(0.0, fault_prob - 0.10)) if fault_prob > 0 else 0.0
    cur_large = min(large_prob, 0.10 + tau * max(0.0, large_prob - 0.10)) if large_prob > 0 else 0.0
    cur_do = min(do_prob, 0.10 + tau * max(0.0, do_prob - 0.10)) if do_prob > 0 else 0.0
    # Keep ground surveys primary (~0.78 -> ground_prob) while gently widening secondary airborne ceiling
    cur_ground = ground_prob + (1.0 - tau) * max(0.0, 0.78 - ground_prob)
    cur_air_max = 15.0 + tau * (CLEARANCE_AIRBORNE_V6[1] - 15.0)
    return {
        "hard_prob": cur_hard,
        "fault_prob": cur_fault,
        "large_prob": cur_large,
        "do_prob": cur_do,
        "ground_prob": min(1.0, cur_ground),
        "max_airborne": cur_air_max,
        "subvox_cap_prob": 0.0,
        "exterior_mass_prob": 0.15 + 0.25 * tau,
        "pregrid_blur_prob": 0.05 + 0.15 * tau,
    }


def _call_crit(crit, pred, truth, clean, z_obs, aux=None, unit_id=None):
    """Call ``CompoundInversionLoss`` passing ``unit_id`` when supported by ``crit.forward``."""
    params = inspect.signature(crit.forward).parameters
    if unit_id is not None and "unit_id" in params:
        return crit(pred, truth, clean, z_obs, aux=aux, unit_id=unit_id)
    return crit(pred, truth, clean, z_obs, aux=aux)


@torch.no_grad()
def prepare_v6_batch(batch, op_bank, subvox_cap_bank, asp_list, dev, batch_op,
                     dropout_prob: float, sim_kwargs: dict | None = None,
                     use_v7_input: bool = False, in_channels: int = 5):
    """Simulate v6/v7 physics + D4 augment (with 2D clearance & unit_id) + build 5-ch or 6-ch input."""
    rho, uid, t, clr, nf, _, asp_idx = batch
    rho = rho.to(dev)
    uid = uid.to(dev)
    t = t.to(dev)
    clr = clr.to(dev)
    nf = nf.to(dev)
    asp_idx = asp_idx.to(dev)
    b, _, _, nx, ny = rho.shape
    obs = torch.empty((b, 1, nx, ny), device=dev, dtype=rho.dtype)
    clean = torch.empty((b, 1, nx, ny), device=dev, dtype=rho.dtype)
    z_obs = torch.empty((b, nx, ny), device=dev, dtype=rho.dtype)
    vm = torch.ones((b, 1, nx, ny), device=dev, dtype=rho.dtype)
    want_v7 = bool(use_v7_input or int(in_channels) >= 6)
    dmodes = ["raw"] * b if want_v7 else None
    kw = dict(sim_kwargs or {})
    for ai, op_i in enumerate(op_bank):
        m_a = asp_idx == ai
        if m_a.any():
            cap_i = None if subvox_cap_bank is None else subvox_cap_bank[ai]
            if want_v7:
                o_a, c_a, z_a, v_a, dm_a = simulate_batch(
                    op_i, rho[m_a], t[m_a], clr[m_a], nf[m_a],
                    dropout_prob=dropout_prob, return_mask=True, v6_physics=True,
                    subvox_cap_op=cap_i, return_detrend_modes=True, **kw,
                )
                for src_idx, dst_idx in enumerate(torch.nonzero(m_a, as_tuple=False).flatten().tolist()):
                    dmodes[dst_idx] = dm_a[src_idx]
            else:
                o_a, c_a, z_a, v_a = simulate_batch(
                    op_i, rho[m_a], t[m_a], clr[m_a], nf[m_a],
                    dropout_prob=dropout_prob, return_mask=True, v6_physics=True,
                    subvox_cap_op=cap_i, **kw,
                )
            obs[m_a], clean[m_a], z_obs[m_a], vm[m_a] = o_a, c_a, z_a, v_a
    rho, t, obs, clean, z_obs, vm, clr, uid = augment_d4(
        rho, t, obs, clean, z_obs, valid_mask=vm, clearance=clr, unit_id=uid,
    )
    asp_vals = torch.tensor([asp_list[int(i)] for i in asp_idx.tolist()], device=dev, dtype=rho.dtype)
    depth_totals = (rho.shape[2] * CELL) * asp_vals.view(b, 1, 1)
    if want_v7:
        x = build_v7_input(
            obs, t, depth_totals, valid_mask=vm, aspect=asp_vals, clearance=clr,
            detrend_modes=dmodes,
        )
    else:
        x = build_v6_input(obs, t, depth_totals, valid_mask=vm, aspect=asp_vals, clearance=clr)
    batch_op.aspect_indices = asp_idx
    return x, rho, uid, clean, z_obs


_LOSS_COMP_NAMES = (
    "sup", "data", "cls", "coherence", "edt", "coarse",
    "tier", "rho_init", "nll", "peak_amp", "focus", "iter_data",
)


def _record_loss_components(dst: dict, stats: dict, prefix: str, denom: float = 1.0):
    inv_d = 1.0 / max(float(denom), 1.0)
    for name in _LOSS_COMP_NAMES:
        val = float(stats.get(f"wloss_{name}", 0.0)) * inv_d
        if val > 0.0:
            dst[f"{prefix}wloss_{name}"] = val


def _merge_bucket_metrics(bucket_metrics: dict, weights: dict):
    all_keys = set()
    for m in bucket_metrics.values():
        all_keys.update(m.keys())
    comb = {}
    for k in sorted(all_keys):
        num, den = 0.0, 0.0
        for bname, w in weights.items():
            val = bucket_metrics.get(bname, {}).get(k, float("nan"))
            if not np.isnan(val):
                num += w * val
                den += w
        comb[k] = num / den if den > 0 else float("nan")
    return comb


@torch.no_grad()
def validate_v6_buckets(eval_model, model, forward_with_aux, buckets: dict, crit,
                        batch_op, canonical_op, showcase_tuple=None, weights=None,
                        force_v3_loss: bool = False):
    """Evaluate all 4 stratified v6 buckets cleanly and return composite loss/metrics & monitor tensors."""
    eval_model.eval()
    w_map = dict(V6_BUCKET_WEIGHTS if weights is None else weights)
    orig_canon = batch_op.canonical_op
    orig_idx = batch_op.aspect_indices
    bucket_losses, bucket_metrics = {}, {}
    bucket_preds = {}
    try:
        for bname, chunks in buckets.items():
            tot_n = 0
            loss_sum = 0.0
            agg_m = {}
            weight_m = {}
            seen_keys = set()
            b_preds = []
            for ch in chunks:
                batch_op.aspect_indices = None
                batch_op.canonical_op = ch["op"]
                p_c, a_c = forward_with_aux(eval_model, ch["x"])
                if a_c is None and force_v3_loss:
                    a_c = {}
                if force_v3_loss and a_c is not None:
                    a_c["use_v3_loss"] = True
                b_preds.append(p_c)
                closs, cstats = _call_crit(
                    crit, p_c, ch["truth"], ch["clean"], ch["z_obs"], aux=a_c, unit_id=ch["unit_id"],
                )
                n_c = ch["truth"].shape[0]
                loss_sum += float(closs) * n_c
                met = compute_metrics(
                    p_c, ch["truth"], ch["clean"], ch["z_obs"], ch["op"], ch["dz"],
                    unit_id=ch["unit_id"], detrend_modes=ch.get("detrend_modes"),
                )
                if cstats.get("loss_coherence", 0.0) > 0.0:
                    met["val_coherence"] = float(cstats["loss_coherence"])
                _record_loss_components(met, cstats, "val_")
                cal = compute_calibration_metrics(
                    p_c, ch["truth"], None if a_c is None else a_c.get("log_var"),
                    terrain=ch["terrain"], dz=ch["dz"],
                )
                met.update(cal)
                seen_keys.update(met.keys())
                for mk, mv in met.items():
                    if not np.isnan(mv):
                        agg_m[mk] = agg_m.get(mk, 0.0) + mv * n_c
                        weight_m[mk] = weight_m.get(mk, 0) + n_c
                tot_n += n_c
            bucket_losses[bname] = loss_sum / max(1, tot_n)
            bucket_metrics[bname] = {
                mk: (agg_m[mk] / weight_m[mk] if weight_m.get(mk, 0) > 0 else float("nan"))
                for mk in seen_keys
            }
            bucket_preds[bname] = b_preds

        show_part = None
        if showcase_tuple is not None:
            sx, st, sc, sz, _ = showcase_tuple
            sp, _ = forward_with_aux(eval_model, sx)
            show_part = (sx[:, :2], st, sp, sc, sz)
    finally:
        batch_op.canonical_op = orig_canon
        batch_op.aspect_indices = orig_idx
        if eval_model is model:
            model.train()

    comp_loss = sum(w_map[b] * bucket_losses[b] for b in w_map if b in bucket_losses)
    comp_loss /= max(1e-8, sum(w_map[b] for b in w_map if b in bucket_losses))
    comp_m = _merge_bucket_metrics(bucket_metrics, w_map)

    mon_parts = []
    for bname, take_n in (("ground_clean", 4), ("ground_hard_large_detrend", 4)):
        if bname in buckets and buckets[bname]:
            c0 = buckets[bname][0]
            p0 = bucket_preds[bname][0]
            k_n = min(take_n, c0["truth"].shape[0])
            mon_parts.append((c0["x"][:k_n, :2], c0["truth"][:k_n], p0[:k_n], c0["clean"][:k_n], c0["z_obs"][:k_n]))
    if show_part is not None:
        mon_parts.append(show_part)
    mon_tensors = tuple(torch.cat(seq, dim=0) for seq in zip(*mon_parts)) if mon_parts else None
    return comp_loss, comp_m, bucket_losses, bucket_metrics, mon_tensors


def _build_train_loader(train_ds, batch_size: int, num_workers: int):
    if num_workers > 0:
        try:
            return DataLoader(
                train_ds, batch_size=batch_size, num_workers=num_workers,
                persistent_workers=True, drop_last=True,
            )
        except (OSError, PermissionError, RuntimeError) as exc:
            print(f"Warning: num_workers={num_workers} unavailable ({exc}); falling back to num_workers=0", flush=True)
    return DataLoader(train_ds, batch_size=batch_size, num_workers=0, drop_last=True)


def _load_ckpt_weights_compat(target_model: torch.nn.Module, state_dict: dict, strict: bool = True) -> None:
    """Load checkpoint weights, permitting partial/5->6ch warm-start migration when ``strict=False``."""
    if strict:
        target_model.load_state_dict(state_dict, strict=True)
        return
    try:
        missing, unexpected = target_model.load_state_dict(state_dict, strict=False)
    except RuntimeError:
        cur_sd = target_model.state_dict()
        filtered = {
            k: v for k, v in state_dict.items()
            if k in cur_sd and getattr(v, "shape", None) == getattr(cur_sd[k], "shape", None)
        }
        missing, unexpected = target_model.load_state_dict(filtered, strict=False)
    if missing or unexpected:
        print(
            f"Warm-start partial load (strict=False): {len(missing)} missing, "
            f"{len(unexpected)} unexpected keys",
            flush=True,
        )


def parse_args():
    p = argparse.ArgumentParser(description="Train the 3D gravity-inversion transformer")
    p.add_argument("--epochs", type=int, default=20)
    p.add_argument("--steps", type=int, default=200, help="optimizer steps per epoch")
    p.add_argument("--batch-size", type=int, default=8)
    p.add_argument("--lr", type=float, default=5e-4)
    p.add_argument("--n-val", type=int, default=64)
    p.add_argument("--hard-prob", type=float, default=0.50,
                   help="fraction of training samples using harder/larger composite & contact scenes [0.0-1.0]")
    p.add_argument("--num-workers", type=int, default=None,
                   help="DataLoader worker processes (default: 4 for v6/v7, 0 for v1-v5)")
    p.add_argument("--lambda-data", type=float, default=None,
                   help="physics data-misfit loss weight (default: 0.50 for v6/v7, 0.25 for v1-v5)")
    p.add_argument("--lambda-ms", type=float, default=0.05)
    p.add_argument("--lambda-cls", type=float, default=0.25, help="lithology cross-entropy weight")
    p.add_argument("--lambda-coherence", type=float, default=None,
                   help="composite-safe body-lithology coherence weight (default: 0.12 for v9.1, 0.0 otherwise)")
    p.add_argument("--depth-boost", type=float, default=0.5, help="depth sensitivity ramp in loss [0.0 = uniform]")
    p.add_argument("--ema-decay", type=float, default=None,
                   help="EMA decay for eval/best weights (default: 0.997 for v6/v7, 0.995 for v1-v5; 0 disables)")
    p.add_argument("--warmup-epochs", type=int, default=10,
                   help="v6/v7 linear LR warmup epochs before CosineAnnealingLR")
    p.add_argument("--save-dir", type=str, default="./checkpoints")
    p.add_argument("--vol3d-stride", type=int, default=10,
                   help="save full 3D prediction volume snapshot every N epochs in addition to best/latest (0 = best+latest only)")
    p.add_argument("--refresh-sec", type=int, default=60,
                   help="web dashboard auto-refresh interval in seconds (0 disables auto-refresh, default: 60)")
    p.add_argument("--variant", type=str, default="v5", choices=["v9.1", "v9", "v8", "v7", "v6", "v5", "v4", "v3", "v2", "v1"],
                   help="model architecture variant (default: v5)")
    p.add_argument("--model-size", type=str, default="base", choices=["slim", "base", "heavy"],
                   help="v5/v6/v7 capacity tier: slim, base, heavy")
    p.add_argument("--curriculum-epochs", type=int, default=25,
                   help="number of initial epochs to ramp difficulty & max clearance (0 disables)")
    p.add_argument("--dropout", type=float, default=0.05, help="Dropout3d/MLP rate for v5/v6/v7")
    p.add_argument("--fault-prob", type=float, default=0.40, help="v5/v6/v7 dipping fault cut probability")
    p.add_argument("--dropout-prob", type=float, default=0.35, help="v5/v6/v7 missing-station infill prob")
    p.add_argument("--large-prob", type=float, default=LARGE_BODY_PROB_V6,
                   help="v6/v7 large-area landfill/basin/pluton scene probability")
    p.add_argument("--ground-prob", type=float, default=GROUND_PROB_V6,
                   help="v6/v7 primary ground survey probability [0.30-2.0m]")
    p.add_argument("--no-amp", action="store_true", help="(deprecated no-op; training runs strictly in FP32)")
    p.add_argument("--resume", type=str, default="", help="continue from a checkpoint")
    p.add_argument("--warm-restart", action="store_true",
                   help="start a fresh cosine warmup+decay LR schedule over --epochs when resuming")
    p.add_argument("--reset-best", action="store_true",
                   help="re-baseline best validation loss on --resume")
    p.add_argument("--total-epochs", type=int, default=0,
                   help="optional cumulative end epoch for continuous cosine decay across resumes")
    p.add_argument("--v3-data", action="store_true",
                   help="train and validate v6/v7/v8 on clean V3 ground dataset (v6_mode=False)")
    p.add_argument("--v3-loss", action="store_true",
                   help="use V3 symmetric Jaccard/supervision/NLL/data loss instead of V6 Tversky losses")
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

    if args.variant == "v9.1":
        if V9_1_SIZE_PRESETS is None:
            raise RuntimeError("--variant v9.1 requires V9_1_SIZE_PRESETS in model_swin3d.py")
        arch = dict(V9_1_SIZE_PRESETS[args.model_size])
        arch["dropout"] = float(args.dropout)
    elif args.variant == "v9":
        if V9_SIZE_PRESETS is None:
            raise RuntimeError("--variant v9 requires V9_SIZE_PRESETS in model_swin3d.py")
        arch = dict(V9_SIZE_PRESETS[args.model_size])
        arch["dropout"] = float(args.dropout)
    elif args.variant == "v8":
        if V8_SIZE_PRESETS is None:
            raise RuntimeError("--variant v8 requires V8_SIZE_PRESETS in model_swin3d.py")
        arch = dict(V8_SIZE_PRESETS[args.model_size])
        arch["dropout"] = float(args.dropout)
    elif args.variant == "v7":
        if V7_SIZE_PRESETS is None:
            raise RuntimeError("--variant v7 requires V7_SIZE_PRESETS in model_swin3d.py")
        arch = dict(V7_SIZE_PRESETS[args.model_size])
        arch["dropout"] = float(args.dropout)
    elif args.variant == "v6":
        if V6_SIZE_PRESETS is None:
            raise RuntimeError("--variant v6 requires V6_SIZE_PRESETS in model_swin3d.py")
        arch = dict(V6_SIZE_PRESETS[args.model_size])
        arch["dropout"] = float(args.dropout)
    elif args.variant == "v5":
        arch = dict(V5_SIZE_PRESETS[args.model_size])
        arch["dropout"] = float(args.dropout)
    else:
        arch = dict(ARCH_BY_VARIANT[args.variant])
    start_epoch, best, ck_resumed, migrating = 1, float("inf"), None, False
    if args.resume:
        ck_resumed = torch.load(args.resume, map_location="cpu")
        ck_arch = ck_resumed.get("arch", {"variant": "v1", "embed_dim": 96, "num_heads": 4, "depth": 4})
        ck_variant = str(ck_arch.get("variant", "v1"))
        if args.variant in ("v7", "v8", "v9", "v9.1") and ck_variant != args.variant:
            migrating = True
            print(
                f"Warm-starting {args.variant} ({args.model_size}) from {ck_variant} checkpoint {args.resume} "
                f"(checkpoint epoch {ck_resumed.get('epoch')}, starting epoch {start_epoch})"
            )
        else:
            arch = dict(ck_arch)
            if "epoch" in ck_resumed:
                start_epoch = ck_resumed["epoch"] + 1
            if "val_loss" in ck_resumed:
                best = ck_resumed["val_loss"]
            print(f"Resumed weights from {args.resume} (checkpoint epoch {ck_resumed.get('epoch')}, starting epoch {start_epoch})")

    variant = arch.get("variant", "v1")
    is_v9 = variant in ("v9", "v9.1")
    is_v8 = variant == "v8"
    is_v7 = variant in ("v7", "v8", "v9", "v9.1")
    is_v6 = variant in ("v6", "v7", "v8", "v9", "v9.1")
    is_v5 = variant == "v5"
    is_v5_or_v6 = variant in ("v9.1", "v9", "v8", "v7", "v6", "v5")
    use_v6_data = is_v6 and (not args.v3_data)
    num_workers = (4 if use_v6_data else 0) if args.num_workers is None else int(args.num_workers)
    ema_decay = (0.997 if is_v6 else 0.995) if args.ema_decay is None else float(args.ema_decay)

    fault_prob = float(args.fault_prob) if (use_v6_data or is_v5) else 0.0
    do_prob = float(args.dropout_prob) if (use_v6_data or is_v5) else 0.0
    large_prob = min(1.0, max(0.0, float(args.large_prob)))
    ground_prob = min(1.0, max(0.0, float(args.ground_prob)))
    clear_rng = CLEARANCE_AIRBORNE_V6 if use_v6_data else (CLEARANCE_MULTI if is_v5 else CLEARANCE)
    asp_list = ASPECT_MULTI if (use_v6_data or is_v5) else None

    clr_desc = (
        f"{variant} primary ground {ground_prob:.0%} (0.3-2m) + relief drape, large {large_prob:.0%}, workers {num_workers}, ema {ema_decay:.3f}"
        if use_v6_data else (
            "clearance 0.30-80m, multi-aspect"
            if is_v5 else (f"{variant} ground clearance 0.30-0.40 m (v3 data)" if is_v6 else "ground clearance 0.30-0.40 m")
        )
    )
    print(f"Device {dev} (FP32) | {args.epochs} epochs x {args.steps} steps x batch {args.batch_size} | "
          f"grid {NX}x{NY}x{NZ} @ {CELL} m | hard-prob {hard_prob:.2f} | {clr_desc}")
    print(f"Monitor: {os.path.join(args.save_dir, 'training_monitor.png')} "
          f"| HTML: {os.path.join(args.save_dir, 'dashboard.html')} | GUI window: {'ON' if args.gui else 'OFF (pass --gui)'}")

    if use_v6_data:
        op_bank = [TerrainGravityOperator(NX, NY, NZ, CELL, CELL, CELL * a, h_min=-220.0).to(dev) for a in asp_list]
        subvox_cap_bank = [SubvoxelCapOperator(NX, NY, CELL, CELL, CELL * a).to(dev) for a in asp_list]
        op = op_bank[asp_list.index(1.0)] if 1.0 in asp_list else TerrainGravityOperator(*grid, h_min=-220.0).to(dev)
        batch_op = BatchAspectOperator(op_bank, op)
    elif is_v5:
        op_bank = [TerrainGravityOperator(NX, NY, NZ, CELL, CELL, CELL * a, h_min=-85.0).to(dev) for a in asp_list]
        subvox_cap_bank = None
        op = op_bank[asp_list.index(1.0)] if 1.0 in asp_list else TerrainGravityOperator(*grid, h_min=-85.0).to(dev)
        batch_op = BatchAspectOperator(op_bank, op)
    else:
        op_bank = None
        subvox_cap_bank = None
        op = TerrainGravityOperator(*grid, h_min=-220.0 if is_v6 else -85.0).to(dev)
        batch_op = op

    monitor = TrainingMonitor(args.save_dir, gui=args.gui, vol3d_stride=args.vol3d_stride,
                              forward_op=op, refresh_sec=args.refresh_sec)
    train_ds = BodyTerrainDataset(
        args.steps * args.batch_size, *grid, hard_prob=hard_prob,
        fault_prob=fault_prob, clearance_range=clear_rng, aspect_ratios=asp_list,
        v6_mode=use_v6_data, large_prob=large_prob, ground_prob=ground_prob,
    )
    loader = _build_train_loader(train_ds, args.batch_size, num_workers)

    val_ch = int(arch.get("in_channels", 6)) if is_v7 else (5 if is_v5_or_v6 else 2)
    clr_clamp_val = CLEARANCE_CLAMP_V6 if is_v6 else 4.0
    show_tuple = make_category_showcase(
        op, dev, grid, seed=4024, in_channels=val_ch, clearance_clamp=clr_clamp_val, v6_physics=False,
    )
    show_x, show_true, show_clean, show_z, show_labels = show_tuple

    v6_buckets = None
    vx = vtrue = vclean = vz = hard_val = None
    if use_v6_data:
        eff_n_val = 128 if args.n_val == 64 else args.n_val
        n_per_bucket = max(8, eff_n_val // 4)
        op_bank_map = {i: o for i, o in enumerate(op_bank)}
        cap_bank_map = {i: c for i, c in enumerate(subvox_cap_bank)}
        v6_buckets = make_v6_val_buckets(
            n_per_bucket, 2024, op, op_bank_map, dev, grid,
            subvox_cap_bank=cap_bank_map, aspect_ratios=asp_list, in_channels=val_ch,
        )
        n_gc = min(4, v6_buckets["ground_clean"][0]["truth"].shape[0])
        n_hl = min(4, v6_buckets["ground_hard_large_detrend"][0]["truth"].shape[0])
        mon_labels = ("Ground",) * n_gc + ("HardLarge",) * n_hl + show_labels
    else:
        vx, vtrue, vclean, vz = make_fixed_set(
            args.n_val, 2024, op, dev, grid, hard_prob=0.0, in_channels=val_ch,
            fault_prob=fault_prob * 0.75, clearance_range=clear_rng, dropout_prob=do_prob * 0.75,
            clearance_clamp=clr_clamp_val, v6_mode=False,
        )
        if hard_prob > 0.0:
            n_hard = max(8, args.n_val // 2)
            hard_val = make_fixed_set(
                n_hard, 3024, op, dev, grid, hard_prob=1.0, in_channels=val_ch,
                fault_prob=fault_prob, clearance_range=clear_rng, dropout_prob=do_prob,
                clearance_clamp=clr_clamp_val, v6_mode=False,
            )
            mon_labels = ("Std",) * min(4, len(vx)) + ("Hard",) * min(4, len(hard_val[0])) + show_labels
        else:
            mon_labels = ("Std",) * min(8, len(vx)) + show_labels

    model = create_model(NX, NY, NZ, **arch).to(dev)
    if ck_resumed is not None:
        _load_ckpt_weights_compat(
            model, ck_resumed.get("raw_model_state_dict", ck_resumed["model_state_dict"]),
            strict=not migrating,
        )
    init_updates = 0 if (migrating or not args.resume) else (start_epoch - 1) * args.steps
    ema = ModelEMA(model, ema_decay, lambda: create_model(NX, NY, NZ, **arch).to(dev), initial_updates=init_updates)
    if ck_resumed is not None and ema.ema is not None and "ema_state_dict" in ck_resumed:
        _load_ckpt_weights_compat(ema.ema, ck_resumed["ema_state_dict"], strict=not migrating)
    eval_model = ema.ema if ema.ema is not None else model
    forward_with_aux = ((lambda m, inp: m(inp, return_aux=True)) if variant in ("v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v9.1")
                        else (lambda m, inp: (m(inp), None)))

    size_tag = f" ({arch.get('model_size', 'custom')})" if is_v5_or_v6 else ""
    curr_tag = f" | Curriculum epochs: {args.curriculum_epochs}" if (use_v6_data or is_v5) and args.curriculum_epochs > 0 else ""
    print(f"Architecture: {variant}{size_tag} | "
          f"Parameters: {sum(p.numel() for p in model.parameters()) / 1e6:.2f} M{curr_tag}")
    lambda_data = ((0.50 if is_v9 else (0.80 if is_v8 else 0.50)) if is_v6 else 0.25) if args.lambda_data is None else float(args.lambda_data)
    crit = CompoundInversionLoss(lambda_data=lambda_data, lambda_ms=args.lambda_ms,
                                 lambda_cls=args.lambda_cls, depth_boost=args.depth_boost,
                                 forward_op=batch_op, use_v6_losses=(is_v6 and (not is_v9) and (not args.v3_loss)),
                                 lambda_coherence=args.lambda_coherence)

    end_epoch = start_epoch + args.epochs - 1
    total_steps = max(1, (end_epoch - start_epoch + 1) * args.steps)
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    if ck_resumed is not None and not args.warm_restart and not migrating and "optimizer_state_dict" in ck_resumed:
        try:
            opt.load_state_dict(ck_resumed["optimizer_state_dict"])
            for pg in opt.param_groups:
                pg["initial_lr"] = args.lr
        except Exception:
            pass
    if is_v6:
        sched = build_resume_v6_scheduler(
            opt, None if migrating else ck_resumed, start_epoch, end_epoch, args.steps, args.warmup_epochs,
            total_epochs=args.total_epochs, warm_restart=(args.warm_restart or migrating), min_lr_frac=0.02,
        )
    elif args.resume:
        sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=total_steps, eta_min=args.lr * 0.02)
    else:
        pct_start = min(0.3, max(0.05, 2.0 / total_steps)) if total_steps > 2 else 0.5
        sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=args.lr, total_steps=total_steps,
                                                    pct_start=pct_start, div_factor=10.0, final_div_factor=50.0)

    def _eval_split(xs, ys, cs, zs):
        if use_v6_data or is_v5:
            batch_op.aspect_indices = None
        preds, aux_chunks = [], []
        with torch.no_grad():
            for i in range(0, len(xs), 8):
                p_i, a_i = forward_with_aux(eval_model, xs[i:i + 8])
                preds.append(p_i)
                if a_i is not None:
                    aux_chunks.append(a_i)
            pred = torch.cat(preds)
            if aux_chunks:
                vaux = {}
                for k, v0 in aux_chunks[0].items():
                    if isinstance(v0, torch.Tensor):
                        vaux[k] = torch.cat([c[k] for c in aux_chunks], dim=0)
                    elif isinstance(v0, (tuple, list)) and v0 and all(isinstance(t, torch.Tensor) for t in v0):
                        vaux[k] = type(v0)(
                            torch.cat([c[k][j] for c in aux_chunks], dim=0) for j in range(len(v0))
                        )
                    else:
                        vaux[k] = v0
            else:
                vaux = None
            if args.v3_loss:
                if vaux is None:
                    vaux = {}
                vaux["use_v3_loss"] = True
            sloss, sstats = _call_crit(crit, pred, ys, cs, zs, aux=vaux)
            smet = compute_metrics(pred, ys, cs, zs, op, CELL)
            if sstats.get("loss_coherence", 0.0) > 0.0:
                smet["val_coherence"] = float(sstats["loss_coherence"])
            _record_loss_components(smet, sstats, "val_")
            if vaux is not None and "log_var" in vaux:
                smet.update(compute_calibration_metrics(
                    pred, ys, vaux.get("log_var"), terrain=xs[:, 1] * depth_total, dz=CELL,
                ))
        return float(sloss), smet, pred

    def _pack_monitor(parts):
        xs, ts, ps, cs, zs = zip(*parts)
        return tuple(torch.cat(seq, dim=0) for seq in (xs, ts, ps, cs, zs))

    def validate():
        if use_v6_data:
            return validate_v6_buckets(
                eval_model, model, forward_with_aux, v6_buckets, crit,
                batch_op, op, showcase_tuple=show_tuple, force_v3_loss=args.v3_loss,
            )
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
            for k in sorted(set(std_m) | set(hard_m)):
                vs = std_m.get(k, float("nan"))
                vh = hard_m.get(k, float("nan"))
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
        init_vloss, *_ = validate()
        best_ckpt_exists = os.path.exists(os.path.join(args.save_dir, "best_swin3d.pt"))
        if args.reset_best or not best_ckpt_exists:
            best = init_vloss
        else:
            best = min(best, init_vloss)

    if use_v6_data:
        for bname in ("ground_clean", "ground_hard_large_detrend"):
            c0 = v6_buckets[bname][0]
            z0 = compute_metrics(torch.zeros_like(c0["truth"]), c0["truth"], c0["clean"], c0["z_obs"], c0["op"], c0["dz"], unit_id=c0["unit_id"])
            print(f"Zero predictor ({bname}): " + " ".join(f"{k} {v:.3f}" for k, v in z0.items() if not np.isnan(v)))
    else:
        zero = compute_metrics(torch.zeros_like(vtrue), vtrue, vclean, vz, op, CELL)
        print("Zero predictor (std): " + " ".join(f"{k} {v:.3f}" for k, v in zero.items()))
        if hard_val is not None:
            vhx, vhtrue, vhclean, vhz = hard_val
            zero_h = compute_metrics(torch.zeros_like(vhtrue), vhtrue, vhclean, vhz, op, CELL)
            print("Zero predictor (hard): " + " ".join(f"{k} {v:.3f}" for k, v in zero_h.items()))

    for epoch in range(start_epoch, end_epoch + 1):
        sim_kw = {}
        if use_v6_data:
            c_cfg = compute_v6_curriculum(
                epoch, args.curriculum_epochs, hard_prob, fault_prob, do_prob, large_prob, ground_prob,
            )
            train_ds.set_curriculum(
                hard_prob=c_cfg["hard_prob"], fault_prob=c_cfg["fault_prob"],
                large_prob=c_cfg["large_prob"], ground_prob=c_cfg["ground_prob"],
                max_airborne=c_cfg["max_airborne"],
            )
            cur_do = c_cfg["do_prob"]
            sim_kw = {
                "subvox_cap_prob": c_cfg["subvox_cap_prob"],
                "exterior_mass_prob": c_cfg["exterior_mass_prob"],
                "pregrid_blur_prob": c_cfg["pregrid_blur_prob"],
            }
        elif is_v5 and args.curriculum_epochs > 0:
            tau = min(1.0, max(0.0, (epoch - 1) / max(1, args.curriculum_epochs - 1)))
            cur_hard = min(hard_prob, 0.15 + tau * max(0.0, hard_prob - 0.15)) if hard_prob > 0 else 0.0
            cur_fault = min(fault_prob, 0.05 + tau * max(0.0, fault_prob - 0.05)) if fault_prob > 0 else 0.0
            cur_do = min(do_prob, 0.05 + tau * max(0.0, do_prob - 0.05)) if do_prob > 0 else 0.0
            cur_clr_max = 2.0 + tau * (CLEARANCE_MULTI[1] - 2.0)
            train_ds.set_curriculum(hard_prob=cur_hard, fault_prob=cur_fault, clearance_max=cur_clr_max)
        else:
            cur_do = do_prob
        t0, acc, n = time.time(), {}, 0
        for batch in loader:
            uid = None
            if use_v6_data:
                x, rho, uid, clean, z_obs = prepare_v6_batch(
                    batch, op_bank, subvox_cap_bank, asp_list, dev, batch_op, cur_do,
                    sim_kwargs=sim_kw, use_v7_input=is_v7, in_channels=val_ch,
                )
            elif is_v5:
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
                            dropout_prob=cur_do, return_mask=True,
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
                if val_ch >= 6:
                    x = build_v7_input(
                        obs, t, depth_total, valid_mask=torch.ones_like(obs), aspect=1.0,
                        clearance=c, clearance_clamp=clr_clamp_val, detrend_modes="raw",
                    )
                elif val_ch == 5:
                    x = build_v6_input(
                        obs, t, depth_total, valid_mask=torch.ones_like(obs), aspect=1.0,
                        clearance=c,
                    )
                else:
                    x = torch.cat([obs, terrain_channel(t, depth_total)], dim=1)

            pred, aux = forward_with_aux(model, x)
            if args.v3_loss:
                if aux is None:
                    aux = {}
                aux["use_v3_loss"] = True
            loss, m = _call_crit(crit, pred, rho, clean, z_obs, aux=aux, unit_id=uid)
            opt.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            ema.update(model)
            sched.step()
            for k, v in m.items():
                acc[k] = acc.get(k, 0.0) + v
            n += 1

        if use_v6_data:
            vloss, vm, b_losses, b_metrics, mon_t = validate()
            sel_score = compute_selection_score(vm, b_metrics)
            tr = " ".join(f"{k[5:]} {v / n:.3f}" for k, v in acc.items() if k.startswith("loss_"))
            gc_m = b_metrics["ground_clean"]
            hl_m = b_metrics["ground_hard_large_detrend"]
            air_m = b_metrics["airborne_relief_drape"]
            ma_m = b_metrics["multi_aspect"]
            msg = (
                f"Epoch {epoch:02d}/{end_epoch} [{time.time() - t0:.0f}s] train {tr} | "
                f"val {vloss:.3f} sel {sel_score:.4f} (GC {b_losses['ground_clean']:.3f} IoU {gc_m['iou']:.2f} | "
                f"HL {b_losses['ground_hard_large_detrend']:.3f} IoU {hl_m['iou']:.2f} dz {hl_m['centroid_dz']:.0f}m | "
                f"Air IoU {air_m['iou']:.2f} | MA IoU {ma_m['iou']:.2f} | "
                f"sedR {vm.get('sediment_recall', float('nan')):.2f} mafR {vm.get('mafic_recall', float('nan')):.2f} | "
                f"sp {vm.get('unc_spearman', float('nan')):.2f} aurc {vm.get('aurc', float('nan')):.3f} "
                f"bd {vm.get('bd_sig_err_ratio', float('nan')):.2f})"
            )
            print(msg, flush=True)
            ck = {
                "epoch": epoch, "model_state_dict": eval_model.state_dict(),
                "raw_model_state_dict": model.state_dict(), "val_loss": vloss, "selection_score": sel_score,
                "metrics": vm, "bucket_losses": b_losses, "bucket_metrics": b_metrics,
                "hard_prob": hard_prob, "large_prob": large_prob, "ground_prob": ground_prob,
                "sched_state_dict": sched.state_dict(),
                "optimizer_state_dict": opt.state_dict(),
                "grid": {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL}, "arch": arch,
            }
            if ema.ema is not None:
                ck["ema_state_dict"] = ema.ema.state_dict()
            torch.save(ck, os.path.join(args.save_dir, "last_swin3d.pt"))
            is_best = vloss < best
            if is_best:
                best = vloss
                torch.save(ck, os.path.join(args.save_dir, "best_swin3d.pt"))
            rec_mon = {
                "epoch": epoch, "train_loss": acc["loss_total"] / n, "val_loss": vloss,
                "selection_score": sel_score, **vm,
                "val_std_loss": b_losses["ground_clean"],
                "val_hard_loss": b_losses["ground_hard_large_detrend"],
                "hard_iou": hl_m["iou"],
                "hard_dz": hl_m["centroid_dz"],
                "val_gc_loss": b_losses["ground_clean"], "val_gc_iou": gc_m["iou"],
                "val_hl_loss": b_losses["ground_hard_large_detrend"], "val_hl_iou": hl_m["iou"],
                "val_air_loss": b_losses["airborne_relief_drape"], "val_air_iou": air_m["iou"],
                "val_ma_loss": b_losses["multi_aspect"], "val_ma_iou": ma_m["iou"],
            }
            if acc.get("loss_coherence", 0.0) > 0.0:
                rec_mon["train_coherence"] = acc["loss_coherence"] / n
            _record_loss_components(rec_mon, acc, "train_", denom=n)
            monitor.update(rec_mon, *mon_t, sample_labels=mon_labels, is_best=is_best)
        else:
            vloss, vm, std_loss, std_m, hard_loss, hard_m, mon_t = validate()
            tr = " ".join(f"{k[5:]} {v / n:.3f}" for k, v in acc.items() if k.startswith("loss_"))
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
            if acc.get("loss_coherence", 0.0) > 0.0:
                rec_mon["train_coherence"] = acc["loss_coherence"] / n
            _record_loss_components(rec_mon, acc, "train_", denom=n)
            if hard_m is not None:
                rec_mon["val_std_loss"] = std_loss
                rec_mon["val_hard_loss"] = hard_loss
                rec_mon["hard_iou"] = hard_m["iou"]
                rec_mon["hard_dz"] = hard_m["centroid_dz"]
            monitor.update(rec_mon, *mon_t, sample_labels=mon_labels, is_best=is_best)
    print(f"Done. Best val loss {best:.4f}. Checkpoint: {os.path.join(args.save_dir, 'best_swin3d.pt')}")


if __name__ == "__main__":
    main()
