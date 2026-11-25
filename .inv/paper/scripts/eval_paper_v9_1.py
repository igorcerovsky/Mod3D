"""Comprehensive, reproducible evaluation suite for the arXiv preprint:
"Physics-Coupled 3D Shifted-Window Transformers for Terrain-Draped Gravity Inversion"

Loads frozen archived checkpoints from ``.inv/paper/models/`` by default, evaluates on
strictly disjoint held-out test seeds (never used for training validation or checkpoint
selection), includes fair classical Tikhonov L2 (gamma=0.50) and Focusing IRLS baselines
plus an independent non-inverse-crime analytical / fine-mesh (25 m) OOD benchmark suite,
and exports all structured metrics + visual slice arrays into ``.inv/paper/results/``.

Usage:
    .inv/python/.venv/bin/python .inv/paper/scripts/eval_paper_v9_1.py
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F
from scipy.ndimage import gaussian_filter
from scipy.stats import spearmanr

N_CPU_THREADS = 4
torch.set_num_threads(N_CPU_THREADS)

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_PAPER_DIR = os.path.abspath(os.path.join(_THIS_DIR, ".."))
_MODELS_DIR = os.path.join(_PAPER_DIR, "models")
_INV_PY_DIR = os.path.abspath(os.path.join(_THIS_DIR, "..", "..", "python"))
if _INV_PY_DIR not in sys.path:
    sys.path.insert(0, _INV_PY_DIR)
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

from classical_baselines import run_adjoint_dot_product_test, solve_focusing_or_tikhonov
from dataset import (
    ASPECT_MULTI,
    CELL,
    NX,
    NY,
    NZ,
    RHO_AIR,
    RHO_LIGHT_MAX,
    RHO_LIGHT_MIN,
    RHO_WATER,
    BodyTerrainDataset,
    _rasterize_primitive,
    apply_mod3d_detrend,
    build_v7_input,
    random_bodies_v6,
    random_terrain,
    simulate_batch,
)
from evaluate import (
    compute_calibration_metrics,
    compute_metrics,
    make_fixed_set,
    make_v6_val_buckets,
)
from export_onnx import export_checkpoint_to_onnx
from forward import G_MGAL, SubvoxelCapOperator, TerrainGravityOperator, prism_gz
from losses import _dilate3d
from model_swin3d import create_model

V6_BUCKET_WEIGHTS = {
    "ground_clean": 0.35,
    "ground_hard_large_detrend": 0.35,
    "airborne_relief_drape": 0.15,
    "multi_aspect": 0.15,
}

# Metrics for which mean AND sample std across scenes are recorded
STD_KEYS = (
    "iou",
    "rmse",
    "fg_rmse",
    "ssim_3d",
    "ssim_d3",
    "centroid_dz",
    "amp_ratio",
    "data_misfit_eff",
    "data_rel_misfit",
    "fg_1sig_cov",
)


def load_checkpoint_safe(path: str, retries: int = 3, delay_s: float = 2.0) -> dict:
    """Read-only checkpoint loader with retries."""
    last_exc: Exception | None = None
    for attempt in range(retries):
        try:
            return torch.load(path, map_location="cpu", weights_only=False)
        except (EOFError, RuntimeError, OSError) as exc:
            last_exc = exc
            if attempt + 1 < retries:
                time.sleep(delay_s)
    raise RuntimeError(f"Failed to load checkpoint {path} after {retries} attempts: {last_exc}")


def load_model_from_checkpoint(ckpt_path: str, device: torch.device) -> tuple[torch.nn.Module, dict]:
    """Instantiate model from checkpoint arch/grid and load model_state_dict."""
    ck = load_checkpoint_safe(ckpt_path)
    g = ck.get("grid", {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL})
    arch = dict(ck.get("arch", {}))
    model = create_model(g["nx"], g["ny"], g["nz"], **arch)
    sd = ck["model_state_dict"]
    try:
        model.load_state_dict(sd)
    except RuntimeError:
        msd = model.state_dict()
        remapped = {
            (k.replace(".mlp.2.", ".mlp.3.") if (".mlp.2." in k and k.replace(".mlp.2.", ".mlp.3.") in msd) else k): v
            for k, v in sd.items()
        }
        model.load_state_dict(remapped)
    model.to(device).eval()
    return model, ck


def forward_model_slice(
    model: torch.nn.Module,
    x6: torch.Tensor,
    variant: str,
    in_channels: int | None = None,
) -> tuple[torch.Tensor, torch.Tensor | None]:
    """Slice 6-ch input tensor appropriately for legacy (v3/v5/v6) or 6-ch (v7..v9.1) models."""
    v = str(variant or getattr(model, "variant", "v5")).lower()
    ch = int(in_channels if in_channels is not None else getattr(model, "in_channels", 6))
    if v in ("v1", "v2", "v3", "v4"):
        x_in = x6[:, :2]
    elif v in ("v5", "v5.1", "v5.2", "v6", "v6.1") or ch == 5:
        x_in = x6[:, :5].clone()
        if v.startswith("v5"):
            x_in[:, 4] = x_in[:, 4].clamp(0.0, 4.0)
    else:
        x_in = x6[:, :ch]

    with torch.no_grad():
        if hasattr(model, "forward_uncertainty") and v not in ("v1", "v2", "v3", "v4"):
            pred, log_var = model.forward_uncertainty(x_in)
            return pred, log_var
        try:
            out = model(x_in, return_aux=True)
            if isinstance(out, tuple):
                return out[0], out[1].get("log_var") if isinstance(out[1], dict) else None
            return out, None
        except TypeError:
            return model(x_in), None


def _compute_3d_ssim_sample(
    p_np: np.ndarray,
    t_np: np.ndarray,
    sub_mask_np: np.ndarray,
    d3_mask_np: np.ndarray,
) -> tuple[float, float]:
    """Wang et al. (2004) 3D Gaussian-windowed SSIM on subsurface & 3-voxel dilated foreground shell."""
    l_range = max(2.0 * float(np.max(np.abs(t_np))), 0.1)
    c1 = (0.01 * l_range) ** 2
    c2 = (0.03 * l_range) ** 2
    kw = dict(sigma=1.5, truncate=3.0, mode="nearest")

    mu_p = gaussian_filter(p_np, **kw)
    mu_t = gaussian_filter(t_np, **kw)
    mu_p2 = mu_p * mu_p
    mu_t2 = mu_t * mu_t
    mu_pt = mu_p * mu_t

    sig_p2 = np.maximum(0.0, gaussian_filter(p_np * p_np, **kw) - mu_p2)
    sig_t2 = np.maximum(0.0, gaussian_filter(t_np * t_np, **kw) - mu_t2)
    sig_pt = gaussian_filter(p_np * t_np, **kw) - mu_pt

    num = (2.0 * mu_pt + c1) * (2.0 * sig_pt + c2)
    den = (mu_p2 + mu_t2 + c1) * (sig_p2 + sig_t2 + c2)
    ssim_map = num / np.maximum(den, 1e-12)

    s_sub = float(ssim_map[sub_mask_np].mean()) if sub_mask_np.any() else 1.0
    d3_sub = d3_mask_np & sub_mask_np
    s_d3 = float(ssim_map[d3_sub].mean()) if d3_sub.any() else s_sub
    return s_sub, s_d3


@torch.no_grad()
def compute_extended_metrics_batch(
    pred: torch.Tensor,
    truth: torch.Tensor,
    d_clean: torch.Tensor,
    z_obs: torch.Tensor,
    forward_op: TerrainGravityOperator,
    terrain: torch.Tensor,
    dz: float = 50.0,
    log_var: torch.Tensor | None = None,
    unit_id: torch.Tensor | None = None,
    detrend_modes: list[str] | None = None,
    d_obs: torch.Tensor | None = None,
    sigma_obs: torch.Tensor | float | None = None,
    bg_fields: torch.Tensor | None = None,
    valid_mask: torch.Tensor | None = None,
) -> tuple[dict[str, float], list[dict[str, float]]]:
    """Compute core, extended (RMSE, FG-RMSE, 3D SSIM, reduced chi2), and calibration metrics."""
    b, _, nz_cur, nx_cur, ny_cur = truth.shape
    per_sample: list[dict[str, float]] = []

    z_top = torch.arange(nz_cur, device=truth.device, dtype=truth.dtype).view(1, 1, nz_cur, 1, 1) * float(dz)
    sub_mask = z_top >= terrain.view(b, 1, 1, nx_cur, ny_cur)
    fg_m = (truth != 0).float()
    d3_m = _dilate3d(fg_m, 3) > 0

    dp_all = forward_op(pred, z_obs)
    if detrend_modes is not None:
        dp_dt_all = apply_mod3d_detrend(dp_all, detrend_modes)
    else:
        dp_dt_all = None

    for i in range(b):
        ti = truth[i : i + 1]
        pi = pred[i : i + 1]
        ci = d_clean[i : i + 1]
        zi = z_obs[i : i + 1]
        ui = unit_id[i : i + 1] if unit_id is not None else None
        dmi = [detrend_modes[i]] if detrend_modes is not None else None

        m_i = compute_metrics(pi, ti, ci, zi, forward_op, dz=dz, unit_id=ui, detrend_modes=dmi)
        if log_var is not None:
            cal_i = compute_calibration_metrics(pi, ti, log_var[i : i + 1], terrain=terrain[i : i + 1], dz=dz)
            m_i.update(cal_i)

        sm_i = sub_mask[i, 0]
        err_sq = (pi[0, 0] - ti[0, 0]) ** 2
        m_i["rmse"] = float(torch.sqrt(err_sq[sm_i].mean()).item()) if sm_i.any() else 0.0
        body_i = ti[0, 0] != 0
        m_i["fg_rmse"] = float(torch.sqrt(err_sq[body_i].mean()).item()) if body_i.any() else float("nan")

        s_sub, s_d3 = _compute_3d_ssim_sample(
            pi[0, 0].detach().cpu().numpy().astype(np.float64),
            ti[0, 0].detach().cpu().numpy().astype(np.float64),
            sm_i.detach().cpu().numpy(),
            d3_m[i, 0].detach().cpu().numpy(),
        )
        m_i["ssim_3d"] = s_sub
        m_i["ssim_d3"] = s_d3

        # Mass-weighted depth below terrain [m] and absolute centroid depth [m]
        ta = ti[0, 0].abs()
        if body_i.any():
            z_depth = (z_top[0, 0] + 0.5 * float(dz) - terrain[i].view(1, nx_cur, ny_cur)).clamp_min(0.0)
            m_i["body_depth_m"] = float(((ta * z_depth)[body_i].sum() / ta[body_i].sum()).item())
            m_i["true_z_cent_m"] = float(
                ((ta * (z_top[0, 0] + 0.5 * float(dz)))[body_i].sum() / ta[body_i].sum()).item()
            )
            pa = pi[0, 0].abs()
            same_sign_pos = (pi[0, 0] * ti[0, 0] > 0) | (pa >= 0.20 * max(float(pa.max().item()), 1e-6))
            p_sub = torch.where(same_sign_pos & sm_i, pa, torch.zeros_like(pa))
            if float(p_sub.sum().item()) > 1e-6:
                m_i["pred_z_cent_m"] = float(
                    ((p_sub * (z_top[0, 0] + 0.5 * float(dz))).sum() / p_sub.sum()).item()
                )
            else:
                m_i["pred_z_cent_m"] = float("nan")
        else:
            m_i["body_depth_m"] = float("nan")
            m_i["true_z_cent_m"] = float("nan")
            m_i["pred_z_cent_m"] = float("nan")

        if d_obs is not None and sigma_obs is not None:
            sig_i = float(sigma_obs[i].item()) if isinstance(sigma_obs, torch.Tensor) else float(sigma_obs)
            if sig_i > 1e-12:
                if bg_fields is not None:
                    d_pred_i = dp_all[i : i + 1] + bg_fields[i : i + 1]
                elif dp_dt_all is not None and detrend_modes is not None and detrend_modes[i] != "raw":
                    d_pred_i = dp_dt_all[i : i + 1]
                else:
                    d_pred_i = dp_all[i : i + 1]
                res_i = d_pred_i - d_obs[i : i + 1]
                if valid_mask is not None:
                    vm_i = valid_mask[i : i + 1] > 0.5
                    chi2_i = float((((res_i[vm_i]) / sig_i) ** 2).mean().item()) if vm_i.any() else float("nan")
                else:
                    chi2_i = float(((res_i / sig_i) ** 2).mean().item())
                m_i["chi2_red"] = chi2_i

        if "data_misfit_detrend" in m_i and dmi is not None and dmi[0] != "raw":
            m_i["data_misfit_eff"] = m_i["data_misfit_detrend"]
        else:
            m_i["data_misfit_eff"] = m_i["data_rel_misfit"]

        per_sample.append(m_i)

    agg = _merge_sample_list(per_sample)
    return agg, per_sample


def _merge_sample_list(samples: list[dict[str, float]]) -> dict[str, float]:
    keys = set()
    for s in samples:
        keys.update(s.keys())
    out: dict[str, float] = {}
    for k in sorted(keys):
        vals = [
            float(s[k]) for s in samples
            if k in s and isinstance(s[k], (int, float, np.number)) and not math.isnan(float(s[k]))
        ]
        if vals:
            out[k] = float(np.mean(vals))
            if k in STD_KEYS:
                out[f"{k}_std"] = float(np.std(vals, ddof=1)) if len(vals) > 1 else 0.0
    return out


def _weighted_bucket_merge(
    bucket_stats: dict[str, dict[str, float]],
    weights: dict[str, float],
    all_samples: list[dict] | None = None,
) -> dict[str, float]:
    keys = set()
    for bs in bucket_stats.values():
        keys.update(bs.keys())
    out: dict[str, float] = {}
    for k in sorted(keys):
        if k.endswith("_std"):
            continue
        num, den = 0.0, 0.0
        for bname, w in weights.items():
            val = bucket_stats.get(bname, {}).get(k, float("nan"))
            if not math.isnan(float(val)):
                num += w * float(val)
                den += w
        out[k] = num / den if den > 0.0 else float("nan")
    # Weighted pooled std across stratified samples
    if all_samples:
        for k in STD_KEYS:
            mu = out.get(k, float("nan"))
            if math.isnan(mu):
                continue
            num_v, den_w = 0.0, 0.0
            for bname, w in weights.items():
                bvals = [
                    float(s[k]) for s in all_samples
                    if s.get("bucket") == bname and k in s and not math.isnan(float(s[k]))
                ]
                if bvals:
                    w_i = w / len(bvals)
                    for v in bvals:
                        num_v += w_i * ((v - mu) ** 2)
                        den_w += w_i
            if den_w > 0.0:
                out[f"{k}_std"] = float(math.sqrt(num_v / den_w))
    return out


def eval_on_buckets(
    model: torch.nn.Module,
    buckets: dict,
    variant: str,
    in_channels: int | None = None,
    collect_calib: bool = False,
) -> tuple[dict[str, dict[str, float]], dict[str, float], list[dict], dict | None]:
    """Evaluate any model across pre-built 4-bucket validation/test chunks."""
    bucket_summaries: dict[str, dict[str, float]] = {}
    all_samples: list[dict] = []

    err_chunks, sig_chunks, fg_chunks = [], [], []
    true_zc, pred_zc, bucket_ids = [], [], []

    for b_idx, (bname, chunks) in enumerate(buckets.items()):
        b_samples = []
        cal_agg: dict[str, float] = {}
        cal_w: dict[str, int] = {}
        for ch in chunks:
            x_in = ch["x"]
            pred, log_var = forward_model_slice(model, x_in, variant=variant, in_channels=in_channels)
            _, samp_list = compute_extended_metrics_batch(
                pred=pred,
                truth=ch["truth"],
                d_clean=ch["clean"],
                z_obs=ch["z_obs"],
                forward_op=ch["op"],
                terrain=ch["terrain"],
                dz=ch["dz"],
                log_var=None,
                unit_id=ch["unit_id"],
                detrend_modes=ch.get("detrend_modes"),
            )
            if log_var is not None:
                cal_c = compute_calibration_metrics(pred, ch["truth"], log_var, terrain=ch["terrain"], dz=ch["dz"])
                n_c = ch["truth"].shape[0]
                for ck, cv in cal_c.items():
                    if not math.isnan(float(cv)):
                        cal_agg[ck] = cal_agg.get(ck, 0.0) + float(cv) * n_c
                        cal_w[ck] = cal_w.get(ck, 0) + n_c

            for s_m in samp_list:
                s_m["bucket"] = bname
                true_zc.append(s_m.get("true_z_cent_m", float("nan")))
                pred_zc.append(s_m.get("pred_z_cent_m", float("nan")))
                bucket_ids.append(b_idx)
            b_samples.extend(samp_list)
            all_samples.extend(samp_list)

            if collect_calib and log_var is not None:
                b_s, _, nz_s, nx_s, ny_s = ch["truth"].shape
                z_top = torch.arange(nz_s, dtype=torch.float32).view(1, 1, nz_s, 1, 1) * float(ch["dz"])
                sub_m = (z_top >= ch["terrain"].cpu().view(b_s, 1, 1, nx_s, ny_s)).numpy()
                err_np = (pred - ch["truth"]).abs().cpu().numpy()
                sig_np = torch.exp(0.5 * log_var.clamp(-8.0, 4.0)).cpu().numpy()
                fg_np = (ch["truth"] != 0).cpu().numpy()
                stride = 8
                err_chunks.append(err_np[sub_m][::stride].astype(np.float32))
                sig_chunks.append(sig_np[sub_m][::stride].astype(np.float32))
                fg_chunks.append(fg_np[sub_m][::stride].astype(np.uint8))

        b_sum = _merge_sample_list(b_samples)
        for ck, tot_v in cal_agg.items():
            if cal_w.get(ck, 0) > 0:
                b_sum[ck] = tot_v / float(cal_w[ck])
        bucket_summaries[bname] = b_sum

    composite = _weighted_bucket_merge(bucket_summaries, V6_BUCKET_WEIGHTS, all_samples=all_samples)
    calib_data = None
    if collect_calib and err_chunks:
        err_all = np.concatenate(err_chunks)
        sig_all = np.concatenate(sig_chunks)
        fg_all = np.concatenate(fg_chunks).astype(bool)
        tzc = np.asarray(true_zc, dtype=np.float32)
        pzc = np.asarray(pred_zc, dtype=np.float32)

        # Exact pooled calibration diagnostics matching Figure 6 plots
        ord_s = np.argsort(sig_all)
        cum_s = np.cumsum(err_all[ord_s].astype(np.float64)) / np.arange(1, err_all.size + 1)
        composite["aurc_pooled"] = float(cum_s.mean())
        if fg_all.any():
            composite["fg_unc_spearman"] = float(spearmanr(sig_all[fg_all], err_all[fg_all]).statistic)
            composite["fg_1b_cov_pooled"] = float(np.mean(err_all[fg_all] <= sig_all[fg_all]))
        valid_z = np.isfinite(tzc) & np.isfinite(pzc)
        if valid_z.any():
            dz_signed = pzc[valid_z] - tzc[valid_z]
            composite["centroid_bias_m"] = float(np.mean(dz_signed))
            composite["centroid_within_50m_pct"] = float(np.mean(np.abs(dz_signed) <= 50.0) * 100.0)

        calib_data = {
            "err": err_all,
            "sigma": sig_all,
            "is_fg": fg_all.astype(np.uint8),
            "true_zc": tzc,
            "pred_zc": pzc,
            "bucket_id": np.asarray(bucket_ids, dtype=np.int32),
        }
    return bucket_summaries, composite, all_samples, calib_data


def build_canonical_models(op: TerrainGravityOperator, device: torch.device) -> list[dict]:
    """Construct 4 deterministic canonical 3D geophysical benchmark models (64x64x32 @ 50m)."""
    nx, ny, nz, cell = NX, NY, NZ, CELL
    z_top = torch.arange(nz, device=device, dtype=torch.float32).view(1, 1, nz, 1, 1) * cell
    models = []

    # 1. Compact High-Grade Sulfide / Ore Prism under gentle relief
    rng1 = np.random.default_rng(9101)
    t1_np = random_terrain(rng1, nx, ny, nz * cell) * 0.45
    t1_np = (t1_np - t1_np.min()).astype(np.float32)
    rho1_np = np.zeros((nz, nx, ny), dtype=np.float32)
    t1_cells = t1_np / cell
    z_idx = np.arange(nz)[:, None, None]
    mask1 = np.zeros((nz, nx, ny), dtype=bool)
    mask1[5:11, 28:36, 28:36] = True
    mask1 &= (z_idx >= t1_cells[None])
    rho1_np[mask1] = 1.20
    models.append({
        "name": "block_ore",
        "title": "Compact Ore Block",
        "rho": rho1_np,
        "terrain": t1_np,
        "detrend_mode": "raw",
        "trend_int": 0,
        "noise_frac": 0.02,
        "seed": 1001,
        "min_rho": -2.67,
        "max_rho": 1.95,
        "iy_force": 32,
    })

    # 2. 45-deg Dipping Mafic Dyke under moderate undulating relief
    rng2 = np.random.default_rng(9102)
    t2_np = random_terrain(rng2, nx, ny, nz * cell) * 0.60
    t2_np = (t2_np - t2_np.min()).astype(np.float32)
    m2 = _rasterize_primitive(
        t2_np / cell, nz, cx=32.0, cy=32.0, cz=9.5,
        rx=7.5, ry=11.0, rz=1.9, th=0.0, dip=math.pi / 4.0, is_box=True, cavity=False,
    )
    rho2_np = np.zeros((nz, nx, ny), dtype=np.float32)
    if m2 is not None:
        rho2_np[m2] = 0.55
    models.append({
        "name": "dipping_dyke",
        "title": "Dipping Mafic Dyke ($45^\\circ$)",
        "rho": rho2_np,
        "terrain": t2_np,
        "detrend_mode": "raw",
        "trend_int": 0,
        "noise_frac": 0.02,
        "seed": 1002,
        "min_rho": -2.67,
        "max_rho": 1.95,
        "iy_force": 32,
    })

    # 3. Twin Opposing Targets aligned on cy=32 so a single X-Z section cleanly shows BOTH bodies
    rng3 = np.random.default_rng(9103)
    t3_np = random_terrain(rng3, nx, ny, nz * cell) * 0.50
    t3_np = (t3_np - t3_np.min()).astype(np.float32)
    t3_c = t3_np / cell
    cz_cav = float(t3_c[20, 32]) + 3.0
    m3_cav = _rasterize_primitive(
        t3_c, nz, cx=20.0, cy=32.0, cz=cz_cav,
        rx=3.2, ry=3.2, rz=1.8, th=0.0, dip=0.0, is_box=False, cavity=True,
    )
    m3_ore = _rasterize_primitive(
        t3_c, nz, cx=44.0, cy=32.0, cz=9.5,
        rx=4.5, ry=4.2, rz=3.4, th=0.0, dip=0.0, is_box=False, cavity=False,
    )
    rho3_np = np.zeros((nz, nx, ny), dtype=np.float32)
    if m3_cav is not None:
        rho3_np[m3_cav] = RHO_AIR
    if m3_ore is not None:
        rho3_np[m3_ore] = 1.35
    models.append({
        "name": "twin_opposing",
        "title": "Cavity + Deep Ore",
        "rho": rho3_np,
        "terrain": t3_np,
        "detrend_mode": "raw",
        "trend_int": 0,
        "noise_frac": 0.02,
        "seed": 1003,
        "min_rho": -2.67,
        "max_rho": 1.95,
        "iy_force": 32,
    })

    # 4. Sedimentary Basin (-0.55 g/cm^3) under rugged relief + BorderPlanar regional detrending
    xx_n = np.linspace(-1.0, 1.0, nx)[:, None]
    yy_n = np.linspace(-1.0, 1.0, ny)[None, :]
    t4_np = (
        180.0 * (0.5 + 0.5 * np.tanh(2.0 * (xx_n * xx_n + 0.6 * yy_n * yy_n - 0.35)))
        + 40.0 * np.sin(2.5 * xx_n) * np.cos(1.8 * yy_n)
    ).astype(np.float32)
    t4_np = (t4_np - t4_np.min()).astype(np.float32)
    t4_c = t4_np / cell
    xg, yg = np.meshgrid(np.arange(nx, dtype=np.float32), np.arange(ny, dtype=np.float32), indexing="ij")
    r_ell = ((xg - 32.0) / 15.0) ** 2 + ((yg - 32.0) / 13.0) ** 2
    thick_cells = np.clip(9.0 * (1.0 - r_ell), 0.0, 9.0)
    z_k = np.arange(nz, dtype=np.float32)[:, None, None]
    mask4 = (thick_cells[None] > 0.5) & (z_k >= t4_c[None]) & (z_k < (t4_c + thick_cells)[None])
    rho4_np = np.zeros((nz, nx, ny), dtype=np.float32)
    rho4_np[mask4] = -0.55
    models.append({
        "name": "sedimentary_basin",
        "title": "Sedimentary Basin (Detrended)",
        "rho": rho4_np,
        "terrain": t4_np,
        "detrend_mode": "cpp_border_eval",
        "trend_int": 3,
        "noise_frac": 0.02,
        "seed": 1004,
        "min_rho": -2.67,
        "max_rho": 1.95,
        "iy_force": 32,
    })

    for spec in models:
        rho_t = torch.from_numpy(spec["rho"]).to(device)[None, None]
        terr_t = torch.from_numpy(spec["terrain"]).to(device)[None]
        clr_t = torch.full((1,), 0.35, device=device, dtype=torch.float32)
        z_obs = terr_t - clr_t.view(1, 1, 1)
        with torch.no_grad():
            d_clean = op(rho_t, z_obs)
        g_peak = float(d_clean.abs().max().item())
        sigma_mgal = float(spec["noise_frac"]) * g_peak
        gen = torch.Generator().manual_seed(int(spec["seed"]))
        eps = torch.randn(d_clean.shape, generator=gen, dtype=torch.float32).to(device)
        d_noisy_raw = d_clean + sigma_mgal * eps
        if spec["detrend_mode"] != "raw":
            d_obs = apply_mod3d_detrend(d_noisy_raw, spec["detrend_mode"])
        else:
            d_obs = d_noisy_raw
        x6 = build_v7_input(
            d_obs, terr_t, nz * cell, valid_mask=torch.ones_like(d_obs),
            aspect=1.0, clearance=clr_t, detrend_modes=[spec["detrend_mode"]],
        )
        rho_t = torch.where(z_top >= terr_t.view(1, 1, 1, nx, ny), rho_t, torch.zeros_like(rho_t))
        spec["rho_t"] = rho_t
        spec["terr_t"] = terr_t
        spec["z_obs"] = z_obs
        spec["d_clean"] = d_clean
        spec["d_obs"] = d_obs
        spec["x6"] = x6
        spec["sigma_mgal"] = sigma_mgal

    return models


def _direct_fine_nagy_gravity(
    rho_fine: np.ndarray,
    dx_f: float,
    dy_f: float,
    dz_f: float,
    st_x: np.ndarray,
    st_y: np.ndarray,
    st_z: np.ndarray,
    chunk_prisms: int = 256,
) -> np.ndarray:
    """Exact direct Nagy prism summation from a fine (nz_f, nx_f, ny_f) grid at exact station coords.

    Bypasses TerrainGravityOperator, 2D FFT, and horizontal-level interpolation completely
    to provide an independent non-inverse-crime forward response.
    """
    nz_f, nx_f, ny_f = rho_fine.shape
    nz_idx, ix_idx, iy_idx = np.nonzero(np.abs(rho_fine) > 1e-8)
    vals = rho_fine[nz_idx, ix_idx, iy_idx].astype(np.float64)
    n_p = vals.size
    sx = st_x.reshape(-1, 1).astype(np.float64)  # (N_st, 1)
    sy = st_y.reshape(-1, 1).astype(np.float64)
    sz = st_z.reshape(-1, 1).astype(np.float64)
    out = np.zeros(sx.shape[0], dtype=np.float64)

    x1_all = ix_idx.astype(np.float64) * dx_f
    y1_all = iy_idx.astype(np.float64) * dy_f
    z1_all = nz_idx.astype(np.float64) * dz_f

    with np.errstate(divide="ignore", invalid="ignore"):
        for s0 in range(0, n_p, chunk_prisms):
            s1 = min(n_p, s0 + chunk_prisms)
            u1 = x1_all[None, s0:s1] - sx
            v1 = y1_all[None, s0:s1] - sy
            w1 = z1_all[None, s0:s1] - sz
            k = prism_gz(u1, u1 + dx_f, v1, v1 + dy_f, w1, w1 + dz_f)
            k = np.nan_to_num(k, nan=0.0, posinf=0.0, neginf=0.0)
            out += k @ vals[s0:s1]
    return out.reshape(st_x.shape).astype(np.float32)


def build_ood_noninversecrime_models(device: torch.device) -> list[dict]:
    """Build 3 independent non-inverse-crime OOD benchmarks (grid-free sphere + 25m fine-mesh exact Nagy)."""
    nx, ny, nz, cell = NX, NY, NZ, CELL
    dx_f = dy_f = dz_f = cell / 2.0  # 25 m fine discretization (2x finer per axis = 8x finer volume)
    nx_f, ny_f, nz_f = 2 * nx, 2 * ny, 2 * nz

    st_xc = (np.arange(nx, dtype=np.float64) + 0.5) * cell
    st_yc = (np.arange(ny, dtype=np.float64) + 0.5) * cell
    st_x, st_y = np.meshgrid(st_xc, st_yc, indexing="ij")

    fx_c = (np.arange(nx_f, dtype=np.float64) + 0.5) * dx_f
    fy_c = (np.arange(ny_f, dtype=np.float64) + 0.5) * dy_f
    fz_c = (np.arange(nz_f, dtype=np.float64) + 0.5) * dz_f
    fz3, fx3, fy3 = np.meshgrid(fz_c, fx_c, fy_c, indexing="ij")

    z_top_50 = torch.arange(nz, device=device, dtype=torch.float32).view(1, 1, nz, 1, 1) * cell
    models = []

    # OOD-1: Exact Closed-Form Analytical Buried Sphere under continuous sinusoidal topography
    terr1 = (75.0 * (0.5 + 0.5 * np.sin(2.0 * math.pi * st_x / 3200.0) * np.cos(2.0 * math.pi * st_y / 3200.0))).astype(np.float32)
    z_obs1 = terr1.astype(np.float64) - 0.35
    r_sph = 260.0
    rho_sph = 1.10
    scx, scy, scz = 1600.0, 1600.0, 520.0
    vol_sph = (4.0 / 3.0) * math.pi * (r_sph ** 3)
    dx_s, dy_s, dz_s = st_x - scx, st_y - scy, scz - z_obs1
    dist_s = np.sqrt(dx_s ** 2 + dy_s ** 2 + dz_s ** 2)
    d_clean1 = (G_MGAL * rho_sph * vol_sph * dz_s / (dist_s ** 3)).astype(np.float32)

    dist3_1 = np.sqrt((fz3 - scz) ** 2 + (fx3 - scx) ** 2 + (fy3 - scy) ** 2)
    rho_f1 = np.where(dist3_1 <= r_sph, rho_sph, 0.0).astype(np.float32)
    rho_50_1 = rho_f1.reshape(nz, 2, nx, 2, ny, 2).mean(axis=(1, 3, 5)).astype(np.float32)
    models.append({
        "name": "ood_sphere",
        "title": "Analytical Sphere (Grid-Free)",
        "rho_50": rho_50_1,
        "terrain": terr1,
        "d_clean_np": d_clean1,
        "noise_frac": 0.02,
        "seed": 7701,
        "iy_force": 32,
        "min_rho": -2.67,
        "max_rho": 1.95,
    })

    # OOD-2: Fine-Mesh (25m) Smoothly Depth-Graded Antiform/Dome (continuous density gradient, non-piecewise)
    terr2 = (
        95.0 * (0.5 + 0.5 * np.sin(1.8 * math.pi * (st_x - 1600.0) / 3200.0) * np.exp(-((st_y - 1600.0) / 1400.0) ** 2))
        + 25.0 * (0.5 + 0.5 * np.cos(2.0 * math.pi * st_y / 3200.0))
    ).astype(np.float32)
    terr2 = (terr2 - terr2.min()).astype(np.float32)
    z_obs2 = terr2.astype(np.float64) - 0.35
    # Fine terrain for exact fine-grid subsurface clipping
    terr2_f = (
        95.0 * (0.5 + 0.5 * np.sin(1.8 * math.pi * (fx3[0] - 1600.0) / 3200.0) * np.exp(-((fy3[0] - 1600.0) / 1400.0) ** 2))
        + 25.0 * (0.5 + 0.5 * np.cos(2.0 * math.pi * fy3[0] / 3200.0))
    )
    terr2_f = terr2_f - terr2_f.min()
    # Curved antiform roof + smooth depth/radial density gradient [+0.35, +1.05] g/cm^3
    r_horiz2 = ((fx3 - 1600.0) / 620.0) ** 2 + ((fy3 - 1600.0) / 480.0) ** 2
    z_roof = 260.0 + 210.0 * r_horiz2
    z_base = 700.0 + 60.0 * r_horiz2
    in_dome = (r_horiz2 <= 1.0) & (fz3 >= z_roof) & (fz3 <= z_base) & (fz3 >= terr2_f[None] + 25.0)
    depth_frac = np.clip((fz3 - z_roof) / np.maximum(z_base - z_roof, 1.0), 0.0, 1.0)
    rho_grad = (0.40 + 0.65 * (1.0 - 0.55 * r_horiz2) * (0.45 + 0.55 * depth_frac)).astype(np.float32)
    rho_f2 = np.where(in_dome, rho_grad, 0.0).astype(np.float32)
    d_clean2 = _direct_fine_nagy_gravity(rho_f2, dx_f, dy_f, dz_f, st_x, st_y, z_obs2)
    rho_50_2 = rho_f2.reshape(nz, 2, nx, 2, ny, 2).mean(axis=(1, 3, 5)).astype(np.float32)
    models.append({
        "name": "ood_graded_dome",
        "title": "Graded Antiform (25 m Exact)",
        "rho_50": rho_50_2,
        "terrain": terr2,
        "d_clean_np": d_clean2,
        "noise_frac": 0.02,
        "seed": 7702,
        "iy_force": 32,
        "min_rho": -2.67,
        "max_rho": 1.95,
    })

    # OOD-3: Fine-Mesh (25m) Curved Bipolar Pair (Shallow Ellipsoidal Trough -0.75 + Deep Inclined Ellipsoid +1.25)
    terr3 = (
        110.0 * (0.5 + 0.5 * np.tanh((st_x - 1500.0) / 700.0))
        + 25.0 * np.sin(2.0 * math.pi * st_y / 3200.0)
    ).astype(np.float32)
    terr3 = (terr3 - terr3.min()).astype(np.float32)
    z_obs3 = terr3.astype(np.float64) - 0.35
    terr3_f = (
        110.0 * (0.5 + 0.5 * np.tanh((fx3[0] - 1500.0) / 700.0))
        + 25.0 * np.sin(2.0 * math.pi * fy3[0] / 3200.0)
    )
    terr3_f = terr3_f - terr3_f.min()
    # Shallow curved negative trough centered at (1100, 1600, 310)
    e_neg = ((fx3 - 1100.0) / 360.0) ** 2 + ((fy3 - 1600.0) / 320.0) ** 2 + ((fz3 - 310.0) / 135.0) ** 2
    m_neg = (e_neg <= 1.0) & (fz3 >= terr3_f[None] + 25.0)
    # Deep rotated positive ellipsoid centered at (2150, 1600, 600), tilted 30 deg in X-Z
    ang = math.radians(30.0)
    xr = (fx3 - 2150.0) * math.cos(ang) + (fz3 - 600.0) * math.sin(ang)
    zr = -(fx3 - 2150.0) * math.sin(ang) + (fz3 - 600.0) * math.cos(ang)
    e_pos = (xr / 340.0) ** 2 + ((fy3 - 1600.0) / 280.0) ** 2 + (zr / 185.0) ** 2
    m_pos = (e_pos <= 1.0) & (fz3 >= terr3_f[None] + 25.0)
    rho_f3 = np.zeros((nz_f, nx_f, ny_f), dtype=np.float32)
    rho_f3[m_neg] = -0.75
    rho_f3[m_pos] = 1.25
    d_clean3 = _direct_fine_nagy_gravity(rho_f3, dx_f, dy_f, dz_f, st_x, st_y, z_obs3)
    rho_50_3 = rho_f3.reshape(nz, 2, nx, 2, ny, 2).mean(axis=(1, 3, 5)).astype(np.float32)
    models.append({
        "name": "ood_bipolar_curved",
        "title": "Curved Bipolar (25 m Exact)",
        "rho_50": rho_50_3,
        "terrain": terr3,
        "d_clean_np": d_clean3,
        "noise_frac": 0.02,
        "seed": 7703,
        "iy_force": 32,
        "min_rho": -2.67,
        "max_rho": 1.95,
    })

    for spec in models:
        rho_t = torch.from_numpy(spec["rho_50"]).to(device)[None, None]
        terr_t = torch.from_numpy(spec["terrain"]).to(device)[None]
        clr_t = torch.full((1,), 0.35, device=device, dtype=torch.float32)
        z_obs = terr_t - clr_t.view(1, 1, 1)
        d_clean = torch.from_numpy(spec["d_clean_np"]).to(device)[None, None]
        g_peak = float(d_clean.abs().max().item())
        sigma_mgal = float(spec["noise_frac"]) * g_peak
        gen = torch.Generator().manual_seed(int(spec["seed"]))
        eps = torch.randn(d_clean.shape, generator=gen, dtype=torch.float32).to(device)
        d_obs = d_clean + sigma_mgal * eps
        x6 = build_v7_input(
            d_obs, terr_t, nz * cell, valid_mask=torch.ones_like(d_obs),
            aspect=1.0, clearance=clr_t, detrend_modes=["raw"],
        )
        rho_t = torch.where(z_top_50 >= terr_t.view(1, 1, 1, nx, ny), rho_t, torch.zeros_like(rho_t))
        spec["rho_t"] = rho_t
        spec["terr_t"] = terr_t
        spec["z_obs"] = z_obs
        spec["d_clean"] = d_clean
        spec["d_obs"] = d_obs
        spec["x6"] = x6
        spec["sigma_mgal"] = sigma_mgal
        spec["detrend_mode"] = "raw"
        spec["trend_int"] = 0

    return models


def _eval_3way_benchmark_list(
    specs: list[dict],
    model_v91: torch.nn.Module,
    op: TerrainGravityOperator,
    device: torch.device,
    slice_path: Path,
    tag: str,
) -> dict:
    """Run Tikhonov L2 (gamma=0.50), Focusing IRLS (gamma=0.75), and standalone Swin3D v9.1."""
    results: dict[str, dict] = {}
    slice_dump: dict[str, np.ndarray] = {}

    for spec in specs:
        name = spec["name"]
        rho_t = spec["rho_t"]
        terr_t = spec["terr_t"]
        z_obs = spec["z_obs"]
        d_clean = spec["d_clean"]
        d_obs = spec["d_obs"]
        x6 = spec["x6"]
        sigma_mgal = spec["sigma_mgal"]
        d_mode = spec["detrend_mode"]
        trend_int = spec["trend_int"]
        d_scale = max(0.5, float(rho_t.abs().max().item()))

        truth_np = rho_t[0, 0].cpu().numpy()
        iy_pk = int(spec.get("iy_force", np.unravel_index(np.argmax(np.abs(truth_np)), truth_np.shape)[2]))

        # 1. Swin3D v9.1 (standalone zero-shot)
        t0 = time.perf_counter()
        pred_swin, lv_swin = forward_model_slice(model_v91, x6, variant="v9.1", in_channels=6)
        t_swin = time.perf_counter() - t0

        # 2. Fair Depth-Weighted Tikhonov L2 (Zhdanov standard gamma=0.50, chi2_red ~ 1)
        m_tik, st_tik = solve_focusing_or_tikhonov(
            op, d_obs, z_obs, terr_t, sigma_mgal=sigma_mgal, focusing=False,
            background_trend_mode=trend_int, depth_exponent=0.50, mgs_weight=0.25,
            irls_iterations=10, cg_max_iterations=50, chi_band=0.05,
            min_density=spec["min_rho"], max_density=spec["max_rho"], density_scale=d_scale,
        )

        # 3. Portniaguine-Zhdanov Focusing IRLS (gamma=0.75, MS+MGS, chi2_red ~ 1)
        m_foc, st_foc = solve_focusing_or_tikhonov(
            op, d_obs, z_obs, terr_t, sigma_mgal=sigma_mgal, focusing=True,
            background_trend_mode=trend_int, depth_exponent=0.75,
            irls_iterations=12, cg_max_iterations=50, chi_band=0.06,
            e_start_rel=1.0, e_end_rel=0.01,
            min_density=spec["min_rho"], max_density=spec["max_rho"], density_scale=d_scale,
        )

        method_map = {
            "tikhonov_l2": (m_tik, st_tik["wall_time_s"], st_tik["bg_field"].to(device)),
            "focusing_irls": (m_foc, st_foc["wall_time_s"], st_foc["bg_field"].to(device)),
            "swin3d_v9_1": (pred_swin, t_swin, None),
        }

        results[name] = {"title": spec["title"], "iy_pk": iy_pk, "sigma_mgal": sigma_mgal, "methods": {}}
        slice_dump[f"{name}__truth_xz"] = truth_np[:, :, iy_pk]
        slice_dump[f"{name}__terr_xy"] = terr_t[0].cpu().numpy()
        slice_dump[f"{name}__terr_x"] = terr_t[0, :, iy_pk].cpu().numpy()
        slice_dump[f"{name}__d_obs"] = d_obs[0, 0].cpu().numpy()
        slice_dump[f"{name}__d_clean"] = d_clean[0, 0].cpu().numpy()

        for mname, (m_vol, w_time, bg_f) in method_map.items():
            agg_m, _ = compute_extended_metrics_batch(
                pred=m_vol,
                truth=rho_t,
                d_clean=d_clean,
                z_obs=z_obs,
                forward_op=op,
                terrain=terr_t,
                dz=CELL,
                log_var=lv_swin if mname == "swin3d_v9_1" else None,
                detrend_modes=[d_mode],
                d_obs=d_obs,
                sigma_obs=sigma_mgal,
                bg_fields=bg_f,
            )
            agg_m["wall_time_s"] = float(w_time)
            results[name]["methods"][mname] = agg_m
            slice_dump[f"{name}__{mname}_xz"] = m_vol[0, 0, :, :, iy_pk].cpu().numpy()
            with torch.no_grad():
                gm = op(m_vol, z_obs)
                if bg_f is not None:
                    gm = gm + bg_f
                elif d_mode != "raw":
                    gm = apply_mod3d_detrend(gm, [d_mode])
            slice_dump[f"{name}__{mname}_field"] = gm[0, 0].cpu().numpy()

        print(
            f"  [{tag}: {name:18s}] "
            + " | ".join(
                f"{mn}: IoU={results[name]['methods'][mn]['iou']:.3f}, "
                f"dz={results[name]['methods'][mn]['centroid_dz']:.1f}m, "
                f"chi2={results[name]['methods'][mn]['chi2_red']:.2f}, "
                f"t={results[name]['methods'][mn]['wall_time_s']:.2f}s"
                for mn in ("tikhonov_l2", "focusing_irls", "swin3d_v9_1")
            ),
            flush=True,
        )

    np.savez_compressed(slice_path, **slice_dump)
    return results


def run_ablation_suite(
    models_dir: Path,
    v6_buckets: dict,
    device: torch.device,
    op_clean: TerrainGravityOperator,
    n_clean_std: int,
    n_clean_hard: int,
    seed: int,
) -> dict:
    """Evaluate chronological progression Panel A (realistic 4-bucket) & Panel B (clean ground)."""
    grid = (NX, NY, NZ, CELL, CELL, CELL)

    panel_a_specs = [
        ("v6", "v6_best.pt", "5-ch + 2-Stage Unified"),
        ("v6.1", "v6.1_best.pt", "5-ch + GVB + Smooth IoU"),
        ("v7", "v7_best.pt", "6-ch + 3-Tier Slab"),
        ("v8", "v8_best.pt", "6-ch + 3-Tier Unrolled"),
        ("v9.1_hard", "v9.1_hard_best.pt", "6-ch + Unified + Coh."),
    ]
    panel_a = {}
    for label, fname, desc in panel_a_specs:
        full_p = models_dir / fname
        if not full_p.is_file():
            continue
        m_i, ck_i = load_model_from_checkpoint(str(full_p), device)
        n_params = sum(p.numel() for p in m_i.parameters())
        var = str(ck_i.get("arch", {}).get("variant", label))
        in_ch = int(getattr(m_i, "in_channels", 5 if "v6" in label else 6))
        _, comp, _, _ = eval_on_buckets(m_i, v6_buckets, variant=var, in_channels=in_ch, collect_calib=False)
        panel_a[label] = {
            "desc": desc,
            "epoch": int(ck_i.get("epoch", -1)),
            "params": int(n_params),
            "in_channels": in_ch,
            "metrics": comp,
        }
        print(
            f"  [Progression A | {label:10s}] ep={panel_a[label]['epoch']:3d} params={n_params:,} "
            f"IoU={comp['iou']:.4f}±{comp.get('iou_std',0):.3f} dz={comp['centroid_dz']:.1f}m "
            f"amp={comp['amp_ratio']:.4f} misfit={comp['data_misfit_eff']:.4f} RMSE={comp['rmse']:.4f}",
            flush=True,
        )

    # Extract matched-epoch (~97) validation snapshot of v9.1_hard from archived training history
    hist_p = models_dir / "training_history_v9.1_hard.json"
    matched_ep97 = {}
    if hist_p.is_file():
        hh = json.loads(hist_p.read_text(encoding="utf-8"))["epochs"]
        by_ep = {int(e["epoch"]): e for e in hh}
        ep_m = min(by_ep.keys(), key=lambda x: abs(x - 97))
        matched_ep97 = {
            "epoch": int(ep_m),
            "iou": float(by_ep[ep_m].get("iou", float("nan"))),
            "centroid_dz": float(by_ep[ep_m].get("centroid_dz", float("nan"))),
            "val_loss": float(by_ep[ep_m].get("val_loss", float("nan"))),
        }

    # Panel B: Clean ground regime on disjoint held-out seeds
    sx, st_rho, sc, sz = make_fixed_set(
        n_clean_std, seed, op_clean, device, grid, hard_prob=0.0, in_channels=6, v6_mode=False
    )
    hx, ht_rho, hc, hz = make_fixed_set(
        n_clean_hard, seed + 1000, op_clean, device, grid, hard_prob=1.0, in_channels=6, v6_mode=False
    )
    all_x = torch.cat([sx, hx], dim=0)
    all_t = torch.cat([st_rho, ht_rho], dim=0)
    all_c = torch.cat([sc, hc], dim=0)
    all_z = torch.cat([sz, hz], dim=0)
    all_terr = all_x[:, 1] * float(NZ * CELL)

    panel_b_specs = [
        ("v3", "v3_clean_best.pt", "2-ch + Prototypes"),
        ("v5.2", "v5.2_clean_best.pt", "5-ch + GVB + NLL"),
        ("v8_v3data", "v8_clean_best.pt", "6-ch + 3-Tier Unrolled"),
        ("v9_v3data", "v9_clean_best.pt", "6-ch + Unified Decoder"),
        ("v9.1_v3data", "v9.1_clean_best.pt", "6-ch + Unified + Coh."),
        ("v9.1_hard_xfer", "v9.1_hard_best.pt", "6-ch Hard Zero-Shot"),
    ]
    panel_b = {}
    for label, fname, desc in panel_b_specs:
        full_p = models_dir / fname
        if not full_p.is_file():
            continue
        m_i, ck_i = load_model_from_checkpoint(str(full_p), device)
        n_params = sum(p.numel() for p in m_i.parameters())
        var = str(ck_i.get("arch", {}).get("variant", label.split("_")[0]))
        in_ch = int(getattr(m_i, "in_channels", 2 if label == "v3" else (5 if label == "v5.2" else 6)))
        samp_all = []
        for s0 in range(0, len(all_x), 8):
            sl = slice(s0, s0 + 8)
            p_b, lv_b = forward_model_slice(m_i, all_x[sl], variant=var, in_channels=in_ch)
            _, s_list = compute_extended_metrics_batch(
                pred=p_b, truth=all_t[sl], d_clean=all_c[sl], z_obs=all_z[sl],
                forward_op=op_clean, terrain=all_terr[sl], dz=CELL, log_var=lv_b,
            )
            samp_all.extend(s_list)
        std_m = _merge_sample_list(samp_all[:n_clean_std])
        hard_m = _merge_sample_list(samp_all[n_clean_std:])
        comb_m = _merge_sample_list(samp_all)
        panel_b[label] = {
            "desc": desc,
            "epoch": int(ck_i.get("epoch", -1)),
            "params": int(n_params),
            "in_channels": in_ch,
            "metrics": comb_m,
            "std_metrics": std_m,
            "hard_metrics": hard_m,
        }
        print(
            f"  [Progression B | {label:14s}] ep={panel_b[label]['epoch']:3d} "
            f"IoU={comb_m['iou']:.4f}±{comb_m.get('iou_std',0):.3f} dz={comb_m['centroid_dz']:.1f}m "
            f"amp={comb_m['amp_ratio']:.4f} misfit={comb_m['data_rel_misfit']:.4f} RMSE={comb_m['rmse']:.4f}",
            flush=True,
        )

    return {
        "panel_a_realistic": panel_a,
        "panel_b_clean_ground": panel_b,
        "v91_hard_matched_ep97": matched_ep97,
    }


def run_noise_and_clearance_sweeps(
    model_v91: torch.nn.Module,
    op: TerrainGravityOperator,
    device: torch.device,
    n_samples: int,
    seed_sweep: int = 70260,
    n_irls_noise: int = 6,
) -> tuple[dict, dict]:
    """Paired white vs. colored noise sweep (Swin3D + classical Focusing IRLS comparison) and clearance sweep.

    Uses the EXACT same N=24 v6_mode ground scenes (zero 3D clutter, raw mode) for both sweeps so
    the 0%..2% white noise regime reconciles directly with h=0.35 m at 1.5% noise.
    """
    grid = (NX, NY, NZ, CELL, CELL, CELL)

    ds_shared = BodyTerrainDataset(n_samples, *grid, seed=seed_sweep, hard_prob=0.35, v6_mode=True)
    items = [ds_shared[i] for i in range(n_samples)]
    rho_c = torch.stack([it[0] for it in items]).to(device)
    uid_c = torch.stack([it[1] for it in items]).to(device)
    terr_c = torch.stack([it[2] for it in items]).to(device)
    clr_ground = torch.full((n_samples,), 0.35, device=device, dtype=torch.float32)
    z_obs_ground = terr_c - clr_ground.view(-1, 1, 1)
    with torch.no_grad():
        clean_ground = op(rho_c, z_obs_ground)
    peak_ground = clean_ground.abs().amax(dim=(1, 2, 3), keepdim=True).clamp_min(1e-6)

    rng_noise = np.random.default_rng(seed_sweep + 1)
    base_white_np = rng_noise.standard_normal(size=(n_samples, 1, NX, NY)).astype(np.float32)
    base_corr_np = np.empty_like(base_white_np)
    for i in range(n_samples):
        c_i = gaussian_filter(base_white_np[i, 0], sigma=1.5, mode="reflect")
        c_i = c_i / max(float(np.std(c_i)), 1e-6)
        base_corr_np[i, 0] = c_i
    base_white = torch.from_numpy(base_white_np).to(device)
    base_corr = torch.from_numpy(base_corr_np).to(device)

    noise_results: dict = {"white": {}, "colored": {}, "irls_colored": {}, "irls_white": {}, "n_irls_paired": int(n_irls_noise)}
    levels_white = (0.0, 0.01, 0.02, 0.05, 0.10)
    levels_colored = (0.01, 0.02, 0.05, 0.10)

    for kind, base_eps, lvls in (
        ("white", base_white, levels_white),
        ("colored", base_corr, levels_colored),
    ):
        for frac in lvls:
            d_obs_k = clean_ground + float(frac) * peak_ground * base_eps
            x6_k = build_v7_input(
                d_obs_k, terr_c, NZ * CELL, aspect=1.0, clearance=clr_ground, detrend_modes=["raw"] * n_samples
            )
            samp_k = []
            for s0 in range(0, n_samples, 8):
                sl = slice(s0, s0 + 8)
                p_k, lv_k = forward_model_slice(model_v91, x6_k[sl], variant="v9.1", in_channels=6)
                sig_vec = (float(frac) * peak_ground[sl, 0, 0, 0]) if frac > 0 else None
                _, slist = compute_extended_metrics_batch(
                    pred=p_k, truth=rho_c[sl], d_clean=clean_ground[sl], z_obs=z_obs_ground[sl],
                    forward_op=op, terrain=terr_c[sl], dz=CELL, log_var=lv_k, unit_id=uid_c[sl],
                    d_obs=d_obs_k[sl], sigma_obs=sig_vec,
                )
                samp_k.extend(slist)
            agg_k = _merge_sample_list(samp_k)
            key = f"{int(round(frac * 100))}pct"
            noise_results[kind][key] = agg_k
            print(
                f"  [Noise Swin3D {kind:7s} {key:4s}] IoU={agg_k['iou']:.4f}±{agg_k.get('iou_std',0):.3f} "
                f"dz={agg_k['centroid_dz']:.1f}m RMSE={agg_k['rmse']:.4f} misfit={agg_k['data_rel_misfit']:.4f}",
                flush=True,
            )

            # Paired classical Focusing IRLS comparison on first n_irls_noise scenes for non-zero noise
            if frac > 0.0 and n_irls_noise > 0:
                irls_samples = []
                for idx in range(min(n_irls_noise, n_samples)):
                    sig_i = float(frac * peak_ground[idx, 0, 0, 0].item())
                    d_sc = max(0.5, float(rho_c[idx].abs().max().item()))
                    m_foc, _ = solve_focusing_or_tikhonov(
                        op, d_obs_k[idx : idx + 1], z_obs_ground[idx : idx + 1], terr_c[idx : idx + 1],
                        sigma_mgal=sig_i, focusing=True, background_trend_mode=0,
                        depth_exponent=0.75, irls_iterations=10, cg_max_iterations=45, chi_band=0.08,
                        density_scale=d_sc,
                    )
                    _, sl_f = compute_extended_metrics_batch(
                        pred=m_foc, truth=rho_c[idx : idx + 1], d_clean=clean_ground[idx : idx + 1],
                        z_obs=z_obs_ground[idx : idx + 1], forward_op=op, terrain=terr_c[idx : idx + 1],
                        dz=CELL, unit_id=uid_c[idx : idx + 1], d_obs=d_obs_k[idx : idx + 1], sigma_obs=sig_i,
                    )
                    irls_samples.extend(sl_f)
                agg_f = _merge_sample_list(irls_samples)
                noise_results[f"irls_{kind}"][key] = agg_f
                print(
                    f"  [Noise IRLS   {kind:7s} {key:4s}] IoU={agg_f['iou']:.4f}±{agg_f.get('iou_std',0):.3f} "
                    f"dz={agg_f['centroid_dz']:.1f}m misfit={agg_f['data_rel_misfit']:.4f}",
                    flush=True,
                )

    # 2. Paired Survey Clearance Sweep on the EXACT same N=24 scenes with fixed 1.5% white noise
    clearance_results = {}
    for h_m in (0.35, 5.0, 20.0, 60.0, 120.0, 160.0):
        clr_h = torch.full((n_samples,), float(h_m), device=device, dtype=torch.float32)
        z_obs_h = terr_c - clr_h.view(-1, 1, 1)
        with torch.no_grad():
            clean_h = op(rho_c, z_obs_h)
        peak_h = clean_h.abs().amax(dim=(1, 2, 3), keepdim=True).clamp_min(1e-6)
        obs_h = clean_h + 0.015 * peak_h * base_white
        x6_h = build_v7_input(
            obs_h, terr_c, NZ * CELL, aspect=1.0, clearance=clr_h, detrend_modes=["raw"] * n_samples
        )
        samp_h = []
        for s0 in range(0, n_samples, 8):
            sl = slice(s0, s0 + 8)
            p_h, lv_h = forward_model_slice(model_v91, x6_h[sl], variant="v9.1", in_channels=6)
            _, slist = compute_extended_metrics_batch(
                pred=p_h, truth=rho_c[sl], d_clean=clean_h[sl], z_obs=z_obs_h[sl],
                forward_op=op, terrain=terr_c[sl], dz=CELL, log_var=lv_h, unit_id=uid_c[sl],
            )
            samp_h.extend(slist)
        agg_h = _merge_sample_list(samp_h)
        key_h = f"{h_m:g}m"
        clearance_results[key_h] = agg_h
        print(
            f"  [Clearance {key_h:5s}] IoU={agg_h['iou']:.4f}±{agg_h.get('iou_std',0):.3f} "
            f"dz={agg_h['centroid_dz']:.1f}m amp={agg_h['amp_ratio']:.4f} misfit={agg_h['data_rel_misfit']:.4f}",
            flush=True,
        )

    return noise_results, clearance_results


def export_showcase_slices(
    model_v91: torch.nn.Module,
    op: TerrainGravityOperator,
    device: torch.device,
    out_dir: Path,
    seed_show: int = 40260,
) -> list[dict]:
    """Select 6 diverse composite geological scenes at clean fixed 2% white noise (raw mode)."""
    nx, ny, nz, dx, dy, dz = NX, NY, NZ, CELL, CELL, CELL
    rng = np.random.default_rng(seed_show)

    def _interior_peak_iy(rho_np: np.ndarray) -> int:
        # Prefer interior slice [4, ny-4) so boundary edge iy=0 is not accidentally selected
        sub = np.abs(rho_np[:, :, 4 : ny - 4])
        if float(sub.max()) > 0.0:
            _, _, iy_rel = np.unravel_index(np.argmax(sub), sub.shape)
            return int(iy_rel + 4)
        _, _, iy = np.unravel_index(np.argmax(np.abs(rho_np)), rho_np.shape)
        return int(iy)

    def _peak_slice(rho_np: np.ndarray) -> np.ndarray:
        return rho_np[:, :, _interior_peak_iy(rho_np)]

    def _n_distinct(sl: np.ndarray) -> int:
        vals = sl[sl != 0]
        return len(np.unique(np.round(vals, 2))) if vals.size else 0

    specs = (
        ("Cavities", None, lambda sl: ((np.isclose(sl, RHO_AIR, atol=0.05) | np.isclose(sl, RHO_WATER, atol=0.05)).any() and sl.max() <= 0.05)),
        ("Light / Coal", None, lambda sl: (((sl <= RHO_LIGHT_MAX + 0.02) & (sl >= RHO_LIGHT_MIN - 0.02)).any() and sl.max() <= 0.05)),
        ("Nested Halo", "nested_halo", lambda sl: _n_distinct(sl) >= 2),
        ("Abutting Contact", "abutting_contact", lambda sl: _n_distinct(sl) >= 2),
        ("Vertical Stack", "vertical_stack", lambda sl: _n_distinct(sl) >= 2),
        ("Folded Horizons", "folded_horizons", lambda sl: np.count_nonzero(sl) >= 20),
    )

    rhos, uids, terrains, clearances, labels = [], [], [], [], []
    for name, stype, pred_fn in specs:
        chosen = None
        for _ in range(160):
            t = random_terrain(rng, nx, ny, nz * dz)
            rho, uid = random_bodies_v6(rng, t, nz, dz, dx, hard_prob=0.0, large_prob=0.0, scene_type=stype or "isolated")
            c = np.float32(0.35)
            chosen = (rho, uid, t, c)
            if pred_fn(_peak_slice(rho)):
                break
        rho, uid, t, c = chosen
        rhos.append(torch.from_numpy(rho)[None])
        uids.append(torch.from_numpy(uid)[None])
        terrains.append(torch.from_numpy(t))
        clearances.append(torch.tensor(c))
        labels.append(name)

    st = torch.stack(rhos).to(device)
    suid = torch.stack(uids).to(device)
    sterr = torch.stack(terrains).to(device)
    sclr = torch.stack(clearances).to(device)
    sz = sterr - sclr.view(-1, 1, 1)
    with torch.no_grad():
        sc = op(st, sz)
    g_pk = sc.abs().amax(dim=(1, 2, 3), keepdim=True).clamp_min(1e-6)
    gen_sh = torch.Generator().manual_seed(seed_show + 7)
    obs = sc + 0.02 * g_pk * torch.randn(sc.shape, generator=gen_sh, dtype=torch.float32).to(device)
    dmodes = ["raw"] * len(labels)
    sx = build_v7_input(obs, sterr, nz * dz, aspect=dz / dx, clearance=sclr, detrend_modes=dmodes)

    sp, slv = forward_model_slice(model_v91, sx, variant="v9.1", in_channels=6)
    with torch.no_grad():
        pf = op(sp, sz)

    # Zero out air cells in displayed scale map b_rho = exp(0.5*log_var) * sub_mask (eliminates exp(0)=1 air-cell artifact)
    z_top = torch.arange(nz, device=device, dtype=torch.float32).view(1, 1, nz, 1, 1) * dz
    sub_mask = (z_top >= sterr.view(-1, 1, 1, nx, ny)).to(sp.dtype)
    ssig = (torch.exp(0.5 * slv.clamp(-8.0, 4.0)) * sub_mask) if slv is not None else torch.zeros_like(sp)

    _, per_s = compute_extended_metrics_batch(
        pred=sp, truth=st, d_clean=sc, z_obs=sz, forward_op=op, terrain=sterr, dz=CELL,
        log_var=slv, unit_id=suid, detrend_modes=dmodes, d_obs=obs, sigma_obs=(0.02 * g_pk[:, 0, 0, 0]),
    )

    dump = {}
    meta_list = []
    for j, lbl in enumerate(labels):
        t_np = st[j, 0].cpu().numpy()
        p_np = sp[j, 0].cpu().numpy()
        s_np = ssig[j, 0].cpu().numpy()
        iy_pk = _interior_peak_iy(t_np)

        dump[f"s{j}__truth_xz"] = t_np[:, :, iy_pk]
        dump[f"s{j}__pred_xz"] = p_np[:, :, iy_pk]
        dump[f"s{j}__sigma_xz"] = s_np[:, :, iy_pk]
        dump[f"s{j}__terr_x"] = sterr[j, :, iy_pk].cpu().numpy()
        dump[f"s{j}__obs_field"] = sx[j, 0].cpu().numpy()
        dump[f"s{j}__clean_field"] = sc[j, 0].cpu().numpy()
        dump[f"s{j}__pred_field"] = pf[j, 0].cpu().numpy()
        dump[f"s{j}__obs_x_prof"] = sx[j, 0, :, iy_pk].cpu().numpy()
        dump[f"s{j}__pred_x_prof"] = pf[j, 0, :, iy_pk].cpu().numpy()

        m_j = per_s[j]
        m_j["label"] = lbl
        m_j["iy_pk"] = int(iy_pk)
        meta_list.append(m_j)
        print(
            f"  [Showcase {j}: {lbl:18s}] iy={iy_pk:2d} IoU={m_j['iou']:.3f} dz={m_j['centroid_dz']:.1f}m "
            f"RMSE={m_j['rmse']:.4f} misfit={m_j['data_rel_misfit']:.4f}",
            flush=True,
        )

    np.savez_compressed(out_dir / "showcase_slices.npz", **dump)
    return meta_list


def benchmark_runtimes(
    ckpt_hard_path: str,
    model_v91: torch.nn.Module,
    canonical_res: dict,
    out_dir: Path,
) -> dict:
    """Measure matched-thread (4 CPU threads) PyTorch CPU, ONNX CPU, CoreML ANE/CPU, and Python FFT solvers."""
    x1 = torch.zeros(1, 6, NX, NY)
    xx, yy = np.meshgrid(np.arange(NX) - 0.5 * NX, np.arange(NY) - 0.5 * NY, indexing="ij")
    x1[0, 0] = torch.from_numpy(1.5 * np.exp(-(xx ** 2 + yy ** 2) / 60.0)).float()
    x1[0, 1] = torch.from_numpy(0.05 * (1.0 + np.sin(xx / 9.0))).float()
    x1[0, 2] = 1.0
    x1[0, 3] = 0.0
    x1[0, 4] = 0.35 / CELL
    x1[0, 5] = 0.0

    n_reps = 10
    with torch.no_grad():
        y_pt_d, _ = model_v91.forward_uncertainty(x1)
        t_pt = []
        for _ in range(n_reps):
            t0 = time.perf_counter()
            model_v91.forward_uncertainty(x1)
            t_pt.append((time.perf_counter() - t0) * 1000.0)

    onnx_path = str(out_dir / "v9.1_hard_eval.onnx")
    export_checkpoint_to_onnx(ckpt_hard_path, onnx_path, opset=17, verify=False, verify_mps=False)

    ort_cpu_ms = float("nan")
    ort_cpu_rel = float("nan")
    ort_cm_ms = float("nan")
    ort_cm_rel = float("nan")
    onnx_num_nodes = None
    try:
        import onnx
        onnx_model = onnx.load(onnx_path)
        onnx_num_nodes = int(len(onnx_model.graph.node))
    except Exception:
        pass

    try:
        import onnxruntime as ort

        so_cpu = ort.SessionOptions()
        so_cpu.intra_op_num_threads = N_CPU_THREADS
        so_cpu.inter_op_num_threads = 1
        sess_cpu = ort.InferenceSession(onnx_path, sess_options=so_cpu, providers=["CPUExecutionProvider"])
        x_np = x1.numpy()
        y_cpu = sess_cpu.run(None, {"observation_field": x_np})
        t_oc = []
        for _ in range(n_reps):
            t0 = time.perf_counter()
            sess_cpu.run(None, {"observation_field": x_np})
            t_oc.append((time.perf_counter() - t0) * 1000.0)
        ort_cpu_ms = float(np.median(t_oc))
        pt_d_np = y_pt_d.numpy()
        ort_cpu_rel = float(np.abs(y_cpu[0] - pt_d_np).max() / (np.abs(pt_d_np).max() + 1e-12))

        if "CoreMLExecutionProvider" in ort.get_available_providers():
            try:
                with tempfile.TemporaryDirectory(prefix="inv_paper_coreml_") as cdir:
                    opts = {
                        "ModelFormat": "MLProgram",
                        "MLComputeUnits": "CPUAndNeuralEngine",
                        "RequireStaticInputShapes": "1",
                        "ModelCacheDirectory": cdir,
                    }
                    so_cm = ort.SessionOptions()
                    so_cm.intra_op_num_threads = N_CPU_THREADS
                    so_cm.inter_op_num_threads = 1
                    sess_cm = ort.InferenceSession(
                        onnx_path,
                        sess_options=so_cm,
                        providers=[("CoreMLExecutionProvider", opts), "CPUExecutionProvider"],
                    )
                    if "CoreMLExecutionProvider" in sess_cm.get_providers():
                        y_cm = sess_cm.run(None, {"observation_field": x_np})
                        t_cm = []
                        for _ in range(n_reps):
                            t0 = time.perf_counter()
                            sess_cm.run(None, {"observation_field": x_np})
                            t_cm.append((time.perf_counter() - t0) * 1000.0)
                        ort_cm_ms = float(np.median(t_cm))
                        ort_cm_rel = float(np.abs(y_cm[0] - pt_d_np).max() / (np.abs(pt_d_np).max() + 1e-12))
            except Exception as exc:
                print(f"  [Timing] CoreML CPUAndNeuralEngine skipped ({exc})", flush=True)
    finally:
        for fdel in (onnx_path, onnx_path + ".data"):
            if os.path.exists(fdel):
                os.remove(fdel)

    def _med_canon(mkey: str) -> float:
        vals = [canonical_res[c]["methods"][mkey]["wall_time_s"] for c in canonical_res]
        return float(np.median(vals))

    timing = {
        "cpu_threads": int(N_CPU_THREADS),
        "n_repeats": int(n_reps),
        "onnx_num_nodes": onnx_num_nodes,
        "pytorch_cpu_ms": float(np.median(t_pt)),
        "onnx_cpu_ms": ort_cpu_ms,
        "onnx_cpu_max_rel_diff": ort_cpu_rel,
        "onnx_coreml_ne_ms": ort_cm_ms,
        "onnx_coreml_ne_max_rel_diff": ort_cm_rel,
        "tikhonov_l2_s": _med_canon("tikhonov_l2"),
        "focusing_irls_s": _med_canon("focusing_irls"),
    }
    print(f"  [Timing] {timing}", flush=True)
    return timing


def compute_depth_quartiles(samples: list[dict]) -> list[dict]:
    """Split N=128 4-bucket samples into Q1..Q4 by mass-weighted body depth below terrain."""
    valid = [s for s in samples if not math.isnan(float(s.get("body_depth_m", float("nan"))))]
    depths = np.array([s["body_depth_m"] for s in valid], dtype=np.float64)
    qs = np.quantile(depths, [0.0, 0.25, 0.50, 0.75, 1.0])
    out = []
    for idx in range(4):
        lo, hi = float(qs[idx]), float(qs[idx + 1])
        if idx < 3:
            sub = [s for s in valid if lo <= s["body_depth_m"] < hi]
        else:
            sub = [s for s in valid if lo <= s["body_depth_m"] <= hi]
        m = _merge_sample_list(sub)
        out.append({
            "quartile": f"Q{idx + 1}",
            "n": len(sub),
            "depth_lo_m": lo,
            "depth_hi_m": hi,
            "metrics": m,
        })
        print(
            f"  [Depth Q{idx+1} ({lo:.0f}-{hi:.0f}m, n={len(sub)})] "
            f"IoU={m['iou']:.4f}±{m.get('iou_std',0):.3f} dz={m['centroid_dz']:.1f}m "
            f"amp={m['amp_ratio']:.4f} RMSE={m['rmse']:.4f}",
            flush=True,
        )
    return out


def measure_physics_diagnostics(model_v91: torch.nn.Module, op: TerrainGravityOperator, device: torch.device) -> dict:
    """Measure exact adjoint dot-product error and OddPyramidGravity approximation error dynamically."""
    adj = run_adjoint_dot_product_test(seed=0)
    # OddPyramidGravity vs exact Nagy operator on 8 held-out draped test scenes
    ds_p = BodyTerrainDataset(8, NX, NY, NZ, CELL, CELL, CELL, seed=99001, hard_prob=0.35, v6_mode=True)
    items = [ds_p[i] for i in range(8)]
    rho_p = torch.stack([it[0] for it in items]).to(device)
    terr_p = torch.stack([it[2] for it in items]).to(device)
    clr_p = torch.full((8,), 0.35, device=device, dtype=torch.float32)
    z_obs_p = terr_p - clr_p.view(-1, 1, 1)
    with torch.no_grad():
        g_exact = op(rho_p, z_obs_p)
        g_pyr = model_v91.g_64(rho_p.squeeze(1), z_obs_p.unsqueeze(1), torch.ones(8, 1, 1, 1, device=device))
        rel_errs = [
            float(torch.linalg.norm(g_pyr[i] - g_exact[i]) / torch.linalg.norm(g_exact[i]).clamp_min(1e-9))
            for i in range(8)
        ]
    diag = {
        "adjoint_dot_rel_err": float(adj["rel_err"]),
        "odd_pyramid_rel_err_mean": float(np.mean(rel_errs)),
        "odd_pyramid_rel_err_std": float(np.std(rel_errs, ddof=1)),
    }
    print(
        f"  [Physics Diagnostics] Adjoint rel_err={diag['adjoint_dot_rel_err']:.2e} | "
        f"OddPyramid vs Exact Nagy rel_err={diag['odd_pyramid_rel_err_mean']*100:.2f}% ± {diag['odd_pyramid_rel_err_std']*100:.2f}%",
        flush=True,
    )
    return diag


def main() -> None:
    ap = argparse.ArgumentParser(description="Run full reproducible v9.1 evaluation suite for arXiv.")
    ap.add_argument("--models-dir", default=_MODELS_DIR)
    ap.add_argument("--checkpoint-hard", default=os.path.join(_MODELS_DIR, "v9.1_hard_best.pt"))
    ap.add_argument("--device", default="cpu")
    ap.add_argument("--n-per-bucket", type=int, default=32)
    ap.add_argument("--ablation-n-per-bucket", type=int, default=16)
    ap.add_argument("--clean-n", type=int, default=48)
    ap.add_argument("--clean-hard-n", type=int, default=32)
    ap.add_argument("--noise-n", type=int, default=24)
    ap.add_argument("--n-irls-noise", type=int, default=6)
    ap.add_argument("--seed", type=int, default=20260, help="Disjoint held-out test seed (train val used 2024)")
    ap.add_argument("--output-dir", default=os.path.join(_PAPER_DIR, "results"))
    args = ap.parse_args()

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    models_dir = Path(args.models_dir)
    dev = torch.device(args.device)
    grid = (NX, NY, NZ, CELL, CELL, CELL)

    print("=== 1. Initializing CPU Operators & Loading Archived Checkpoints ===", flush=True)
    op_canon = TerrainGravityOperator(*grid, h_min=-220.0).to(dev)
    op_bank = {
        i: TerrainGravityOperator(NX, NY, NZ, CELL, CELL, CELL * a, h_min=-220.0).to(dev)
        for i, a in enumerate(ASPECT_MULTI)
    }
    cap_bank = {
        i: SubvoxelCapOperator(NX, NY, CELL, CELL, CELL * a).to(dev)
        for i, a in enumerate(ASPECT_MULTI)
    }

    model_hard, ck_hard = load_model_from_checkpoint(args.checkpoint_hard, dev)
    n_params_v91 = sum(p.numel() for p in model_hard.parameters())
    print(f"Loaded {Path(args.checkpoint_hard).name} ep={ck_hard.get('epoch')} ({n_params_v91:,} params)", flush=True)

    print("\n=== 1b. Physics Operator Diagnostics (Adjoint & OddPyramid Approximation) ===", flush=True)
    phys_diag = measure_physics_diagnostics(model_hard, op_canon, dev)

    print(f"\n=== 2. Flagship v9.1_hard 4-Bucket Held-Out Test (N={4 * args.n_per_bucket}, seed={args.seed}) ===", flush=True)
    buckets_main = make_v6_val_buckets(
        args.n_per_bucket, args.seed, op_canon, op_bank, dev, grid,
        subvox_cap_bank=cap_bank, in_channels=6,
    )
    b_stats, comp_stats, all_samples, calib_data = eval_on_buckets(
        model_hard, buckets_main, variant="v9.1", in_channels=6, collect_calib=True
    )
    for bname, bm in b_stats.items():
        print(
            f"  [{bname:26s}] IoU={bm['iou']:.4f}±{bm.get('iou_std',0):.3f} dz={bm['centroid_dz']:.1f}m "
            f"RMSE={bm['rmse']:.4f} SSIM_d3={bm['ssim_d3']:.4f} amp={bm['amp_ratio']:.4f} "
            f"misfit={bm['data_misfit_eff']:.4f} cov={bm.get('fg_1sig_cov', float('nan')):.4f}",
            flush=True,
        )
    print(
        f"  [COMPOSITE WEIGHTED       ] IoU={comp_stats['iou']:.4f}±{comp_stats.get('iou_std',0):.3f} "
        f"dz={comp_stats['centroid_dz']:.1f}m RMSE={comp_stats['rmse']:.4f} SSIM_d3={comp_stats['ssim_d3']:.4f} "
        f"amp={comp_stats['amp_ratio']:.4f} misfit={comp_stats['data_misfit_eff']:.4f} "
        f"fg_sp={comp_stats.get('fg_unc_spearman', float('nan')):.4f} aurc_pool={comp_stats.get('aurc_pooled', float('nan')):.5f} "
        f"bias={comp_stats.get('centroid_bias_m', float('nan')):.1f}m",
        flush=True,
    )
    if calib_data is not None:
        np.savez_compressed(out_dir / "calib_arrays.npz", **calib_data)

    print("\n=== 3. Depth-Quartile Breakdown (from Held-Out 4-Bucket Suite) ===", flush=True)
    depth_quartiles = compute_depth_quartiles(all_samples)

    print(f"\n=== 4. Chronological Architecture Progression (v3..v9.1, seed={args.seed}) ===", flush=True)
    if args.ablation_n_per_bucket == args.n_per_bucket:
        buckets_abl = buckets_main
    else:
        buckets_abl = make_v6_val_buckets(
            args.ablation_n_per_bucket, args.seed, op_canon, op_bank, dev, grid,
            subvox_cap_bank=cap_bank, in_channels=6,
        )
    ablation_res = run_ablation_suite(
        models_dir, buckets_abl, dev, op_canon,
        args.clean_n, args.clean_hard_n, args.seed,
    )

    print("\n=== 5. 4 Canonical Benchmarks vs Classical Inversion (No Hybrid) ===", flush=True)
    canon_specs = build_canonical_models(op_canon, dev)
    canonical_res = _eval_3way_benchmark_list(
        canon_specs, model_hard, op_canon, dev, out_dir / "canonical_slices.npz", "Canonical"
    )

    print("\n=== 5b. 3 Independent Non-Inverse-Crime OOD Benchmarks (Analytical / 25m Exact Nagy) ===", flush=True)
    ood_specs = build_ood_noninversecrime_models(dev)
    ood_res = _eval_3way_benchmark_list(
        ood_specs, model_hard, op_canon, dev, out_dir / "ood_slices.npz", "OOD-NonInvCrime"
    )

    print(f"\n=== 6. Unified Noise & Clearance Sweeps (N={args.noise_n} + Paired IRLS N={args.n_irls_noise}) ===", flush=True)
    noise_res, clearance_res = run_noise_and_clearance_sweeps(
        model_hard, op_canon, dev, args.noise_n, seed_sweep=70260, n_irls_noise=args.n_irls_noise
    )

    print("\n=== 7. Curated 6-Archetype Composite Showcase (Clean Fixed 2% Noise, Raw Mode) ===", flush=True)
    showcase_meta = export_showcase_slices(model_hard, op_canon, dev, out_dir, seed_show=40260)

    print("\n=== 8. Matched-Thread Runtime & ONNX/CoreML Parity Benchmark ===", flush=True)
    timing_res = benchmark_runtimes(args.checkpoint_hard, model_hard, canonical_res, out_dir)

    summary = {
        "meta": {
            "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "checkpoint_hard": str(args.checkpoint_hard),
            "checkpoint_hard_epoch": int(ck_hard.get("epoch", -1)),
            "checkpoint_hard_params": int(n_params_v91),
            "models_dir": str(models_dir),
            "n_per_bucket": int(args.n_per_bucket),
            "ablation_n_per_bucket": int(args.ablation_n_per_bucket),
            "clean_n": int(args.clean_n),
            "clean_hard_n": int(args.clean_hard_n),
            "noise_n": int(args.noise_n),
            "n_irls_noise": int(args.n_irls_noise),
            "test_seed": int(args.seed),
            "train_val_seed": 2024,
            "cpu_threads": int(N_CPU_THREADS),
            "bucket_weights": V6_BUCKET_WEIGHTS,
        },
        "physics_diagnostics": phys_diag,
        "v91_buckets": b_stats,
        "v91_composite": comp_stats,
        "depth_quartiles": depth_quartiles,
        "ablation": ablation_res,
        "canonical": canonical_res,
        "ood_noninversecrime": ood_res,
        "noise_sweep": noise_res,
        "clearance_sweep": clearance_res,
        "showcase": showcase_meta,
        "timing": timing_res,
    }
    out_json = out_dir / "eval_results.json"
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    print(f"\nSaved complete evaluation results to {out_json}", flush=True)


if __name__ == "__main__":
    main()
