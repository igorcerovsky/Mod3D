"""Comprehensive, reproducible evaluation suite for the arXiv preprint:
"Transformers Are All You Need for Potential Field Inversion"

Runs strictly on CPU (--device cpu default) so any ongoing MPS GPU training
in ``.inv/python/checkpoints_v9.1_hard`` is completely untouched. Reads checkpoints
with safe retries and exports all structured metrics + visual slice arrays into
``.inv/paper/results/``.

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

torch.set_num_threads(4)

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_PAPER_DIR = os.path.abspath(os.path.join(_THIS_DIR, ".."))
_INV_PY_DIR = os.path.abspath(os.path.join(_THIS_DIR, "..", "..", "python"))
if _INV_PY_DIR not in sys.path:
    sys.path.insert(0, _INV_PY_DIR)
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

from classical_baselines import solve_focusing_or_tikhonov
from dataset import (
    ASPECT_MULTI,
    CELL,
    NX,
    NY,
    NZ,
    RHO_AIR,
    BodyTerrainDataset,
    _rasterize_primitive,
    apply_mod3d_detrend,
    build_v7_input,
    random_terrain,
    simulate_batch,
)
from evaluate import (
    compute_calibration_metrics,
    compute_metrics,
    make_category_showcase,
    make_fixed_set,
    make_v6_val_buckets,
)
from export_onnx import export_checkpoint_to_onnx
from forward import SubvoxelCapOperator, TerrainGravityOperator
from losses import _dilate3d
from model_swin3d import create_model

V6_BUCKET_WEIGHTS = {
    "ground_clean": 0.35,
    "ground_hard_large_detrend": 0.35,
    "airborne_relief_drape": 0.15,
    "multi_aspect": 0.15,
}


def load_checkpoint_safe(path: str, retries: int = 3, delay_s: float = 2.0) -> dict:
    """Read-only checkpoint loader with retries in case training is overwriting best_swin3d.pt."""
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
    """Instantiate model from checkpoint arch/grid and load model_state_dict on CPU."""
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
    """Wang et al. (2004) 3D Gaussian-windowed SSIM on subsurface & d3 foreground shell."""
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

        # Mass-weighted depth below terrain [m] for depth-quartile analysis
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

        # Unify detrended vs raw misfit into effective misfit when detrended
        if "data_misfit_detrend" in m_i and dmi is not None and dmi[0] != "raw":
            m_i["data_misfit_eff"] = m_i["data_misfit_detrend"]
        else:
            m_i["data_misfit_eff"] = m_i["data_rel_misfit"]

        per_sample.append(m_i)

    # Aggregate across batch
    all_keys = set()
    for d_i in per_sample:
        all_keys.update(d_i.keys())
    agg: dict[str, float] = {}
    for k in sorted(all_keys):
        vals = [d_i[k] for d_i in per_sample if k in d_i and not math.isnan(float(d_i[k]))]
        agg[k] = float(np.mean(vals)) if vals else float("nan")
    return agg, per_sample


def _merge_sample_list(samples: list[dict[str, float]]) -> dict[str, float]:
    keys = set()
    for s in samples:
        keys.update(s.keys())
    out = {}
    for k in sorted(keys):
        vals = [
            float(s[k]) for s in samples
            if k in s and isinstance(s[k], (int, float, np.number)) and not math.isnan(float(s[k]))
        ]
        if vals:
            out[k] = float(np.mean(vals))
    return out


def _weighted_bucket_merge(bucket_stats: dict[str, dict[str, float]], weights: dict[str, float]) -> dict[str, float]:
    keys = set()
    for bs in bucket_stats.values():
        keys.update(bs.keys())
    out = {}
    for k in sorted(keys):
        num, den = 0.0, 0.0
        for bname, w in weights.items():
            val = bucket_stats.get(bname, {}).get(k, float("nan"))
            if not math.isnan(float(val)):
                num += w * float(val)
                den += w
        out[k] = num / den if den > 0.0 else float("nan")
    return out


def eval_on_buckets(
    model: torch.nn.Module,
    buckets: dict,
    variant: str,
    in_channels: int | None = None,
    collect_calib: bool = False,
) -> tuple[dict[str, dict[str, float]], dict[str, float], list[dict], dict | None]:
    """Evaluate any model across pre-built 4-bucket validation chunks."""
    bucket_summaries: dict[str, dict[str, float]] = {}
    all_samples: list[dict] = []

    err_chunks, sig_chunks, fg_chunks = [], [], []
    true_zc, pred_zc, bucket_ids = [], [], []

    for b_idx, (bname, chunks) in enumerate(buckets.items()):
        b_samples = []
        cal_agg = {}
        cal_w = {}
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
                # Subsample deterministically to keep calib_arrays.npz compact (~200k voxels)
                stride = 8
                err_chunks.append(err_np[sub_m][::stride].astype(np.float32))
                sig_chunks.append(sig_np[sub_m][::stride].astype(np.float32))
                fg_chunks.append(fg_np[sub_m][::stride].astype(np.uint8))

        b_sum = _merge_sample_list(b_samples)
        for ck, tot_v in cal_agg.items():
            if cal_w.get(ck, 0) > 0:
                b_sum[ck] = tot_v / float(cal_w[ck])
        bucket_summaries[bname] = b_sum

    composite = _weighted_bucket_merge(bucket_summaries, V6_BUCKET_WEIGHTS)
    calib_data = None
    if collect_calib and err_chunks:
        calib_data = {
            "err": np.concatenate(err_chunks),
            "sigma": np.concatenate(sig_chunks),
            "is_fg": np.concatenate(fg_chunks),
            "true_zc": np.asarray(true_zc, dtype=np.float32),
            "pred_zc": np.asarray(pred_zc, dtype=np.float32),
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
    t1_np = random_terrain(rng1, nx, ny, nz * cell) * 0.45  # max ~80 m relief
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
    })

    # 2. 45-deg Dipping Mafic Dyke under moderate undulating relief
    rng2 = np.random.default_rng(9102)
    t2_np = random_terrain(rng2, nx, ny, nz * cell) * 0.60  # max ~120 m relief
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
    })

    # 3. Twin Opposing Targets: Shallow Air Cavity (-2.67) + Deeper Dense Orebody (+1.35)
    rng3 = np.random.default_rng(9103)
    t3_np = random_terrain(rng3, nx, ny, nz * cell) * 0.50
    t3_np = (t3_np - t3_np.min()).astype(np.float32)
    t3_c = t3_np / cell
    cz_cav = float(t3_c[22, 24]) + 3.0
    m3_cav = _rasterize_primitive(
        t3_c, nz, cx=22.0, cy=24.0, cz=cz_cav,
        rx=3.2, ry=3.2, rz=1.8, th=0.2, dip=0.0, is_box=False, cavity=True,
    )
    m3_ore = _rasterize_primitive(
        t3_c, nz, cx=42.0, cy=40.0, cz=9.5,
        rx=4.5, ry=4.2, rz=3.4, th=-0.3, dip=0.0, is_box=False, cavity=False,
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
    })

    # 4. Sedimentary Basin (-0.55 g/cm^3) under rugged relief + BorderPlanar regional detrending
    rng4 = np.random.default_rng(9104)
    xx_n = np.linspace(-1.0, 1.0, nx)[:, None]
    yy_n = np.linspace(-1.0, 1.0, ny)[None, :]
    # Concave valley-overlain basin profile
    t4_np = (180.0 * (0.5 + 0.5 * np.tanh(2.0 * (xx_n * xx_n + 0.6 * yy_n * yy_n - 0.35)))
             + 40.0 * np.sin(2.5 * xx_n) * np.cos(1.8 * yy_n)).astype(np.float32)
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
    })

    # Pre-simulate clean and noisy observations for each canonical model
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
        # Ensure above-terrain is zeroed in truth
        rho_t = torch.where(z_top >= terr_t.view(1, 1, 1, nx, ny), rho_t, torch.zeros_like(rho_t))
        spec["rho_t"] = rho_t
        spec["terr_t"] = terr_t
        spec["z_obs"] = z_obs
        spec["d_clean"] = d_clean
        spec["d_obs"] = d_obs
        spec["x6"] = x6
        spec["sigma_mgal"] = sigma_mgal

    return models


def run_canonical_benchmarks(
    model_v91: torch.nn.Module,
    op: TerrainGravityOperator,
    device: torch.device,
    out_dir: Path,
) -> dict:
    """Run Tikhonov L2, Focusing IRLS, Swin3D v9.1, and Hybrid v9.1+Focus on 4 canonical models."""
    specs = build_canonical_models(op, device)
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

        # Peak X-Z cross-section index iy
        truth_np = rho_t[0, 0].cpu().numpy()
        _, _, iy_pk = np.unravel_index(np.argmax(np.abs(truth_np)), truth_np.shape)

        # 1. Swin3D v9.1 (zero-shot)
        t0 = time.perf_counter()
        pred_swin, lv_swin = forward_model_slice(model_v91, x6, variant="v9.1", in_channels=6)
        t_swin = time.perf_counter() - t0
        sigma_prior = torch.exp(0.5 * lv_swin.clamp(-8.0, 4.0)) if lv_swin is not None else None

        # 2. Depth-weighted Tikhonov L2
        m_tik, st_tik = solve_focusing_or_tikhonov(
            op, d_obs, z_obs, terr_t, sigma_mgal=sigma_mgal, focusing=False,
            background_trend_mode=trend_int, irls_iterations=6, cg_max_iterations=50,
            min_density=spec["min_rho"], max_density=spec["max_rho"], density_scale=d_scale,
        )

        # 3. Portniaguine-Zhdanov Focusing IRLS
        m_foc, st_foc = solve_focusing_or_tikhonov(
            op, d_obs, z_obs, terr_t, sigma_mgal=sigma_mgal, focusing=True,
            background_trend_mode=trend_int, irls_iterations=12, cg_max_iterations=50,
            e_start_rel=1.0, e_end_rel=0.01,
            min_density=spec["min_rho"], max_density=spec["max_rho"], density_scale=d_scale,
        )

        # 4. Hybrid Swin3D v9.1 + Uncertainty-Weighted Focusing IRLS
        m_hyb, st_hyb = solve_focusing_or_tikhonov(
            op, d_obs, z_obs, terr_t, sigma_mgal=sigma_mgal, focusing=True,
            m_ref=pred_swin, m_ref_sigma=sigma_prior,
            background_trend_mode=trend_int, irls_iterations=6, cg_max_iterations=40,
            e_start_rel=1.0, e_end_rel=0.02, rho_start=0.5,
            min_density=spec["min_rho"], max_density=spec["max_rho"], density_scale=d_scale,
        )

        method_map = {
            "tikhonov_l2": (m_tik, st_tik["wall_time_s"], st_tik["bg_field"].to(device)),
            "focusing_irls": (m_foc, st_foc["wall_time_s"], st_foc["bg_field"].to(device)),
            "swin3d_v9_1": (pred_swin, t_swin, None),
            "hybrid_v9_1_focus": (m_hyb, t_swin + st_hyb["wall_time_s"], st_hyb["bg_field"].to(device)),
        }

        results[name] = {"title": spec["title"], "iy_pk": int(iy_pk), "sigma_mgal": sigma_mgal, "methods": {}}
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
            f"  [Canonical: {name}] "
            + " | ".join(
                f"{mn}: IoU={results[name]['methods'][mn]['iou']:.3f}, "
                f"dz={results[name]['methods'][mn]['centroid_dz']:.1f}m, "
                f"t={results[name]['methods'][mn]['wall_time_s']:.2f}s"
                for mn in ("tikhonov_l2", "focusing_irls", "swin3d_v9_1", "hybrid_v9_1_focus")
            ),
            flush=True,
        )

    np.savez_compressed(out_dir / "canonical_slices.npz", **slice_dump)
    return results


def run_ablation_suite(
    ckpt_root: Path,
    v6_buckets: dict,
    device: torch.device,
    op_clean: TerrainGravityOperator,
    n_clean_std: int,
    n_clean_hard: int,
    seed: int,
) -> dict:
    """Evaluate Panel A (4-bucket realistic) and Panel B (clean V3 ground) across v3..v9.1."""
    grid = (NX, NY, NZ, CELL, CELL, CELL)

    # Panel A: 4-bucket realistic regime
    panel_a_specs = [
        ("v6", "checkpoints_v6/best_swin3d.pt", "5-ch + 2-Stage Unified"),
        ("v6.1", "checkpoints_v6.1/best_swin3d.pt", "5-ch + GVB + Smooth IoU"),
        ("v7", "checkpoints_v7/best_swin3d.pt", "6-ch + 3-Tier Slab"),
        ("v8", "checkpoints_v8/best_swin3d.pt", "6-ch + 3-Tier Unrolled"),
        ("v9.1_hard", "checkpoints_v9.1_hard/best_swin3d.pt", "6-ch + Unified + Coh."),
    ]
    panel_a = {}
    for label, rel_p, desc in panel_a_specs:
        full_p = ckpt_root / rel_p
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
            f"  [Ablation A | {label:10s}] ep={panel_a[label]['epoch']:3d} params={n_params:,} "
            f"IoU={comp['iou']:.4f} dz={comp['centroid_dz']:.1f}m amp={comp['amp_ratio']:.4f} "
            f"misfit={comp['data_misfit_eff']:.4f} RMSE={comp['rmse']:.4f}",
            flush=True,
        )

    # Panel B: Clean V3 ground regime (identical std + hard seeded sets)
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
        ("v3", "checkpoints_v3/best_swin3d.pt", "2-ch + Prototypes"),
        ("v5.2", "checkpoints_v5.2/best_swin3d.pt", "5-ch + GVB + NLL"),
        ("v8_v3data", "checkpoints_v8_v3data/best_swin3d.pt", "6-ch + 3-Tier Unrolled"),
        ("v9_v3data", "checkpoints_v9_v3data/best_swin3d.pt", "6-ch + Unified Decoder"),
        ("v9.1_v3data", "checkpoints_v9.1_v3data/best_swin3d.pt", "6-ch + Unified + Coh."),
        ("v9.1_hard_xfer", "checkpoints_v9.1_hard/best_swin3d.pt", "6-ch Hard Zero-Shot"),
    ]
    panel_b = {}
    for label, rel_p, desc in panel_b_specs:
        full_p = ckpt_root / rel_p
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
            f"  [Ablation B | {label:14s}] ep={panel_b[label]['epoch']:3d} "
            f"IoU={comb_m['iou']:.4f} dz={comb_m['centroid_dz']:.1f}m amp={comb_m['amp_ratio']:.4f} "
            f"misfit={comb_m['data_rel_misfit']:.4f} RMSE={comb_m['rmse']:.4f}",
            flush=True,
        )

    return {"panel_a_realistic": panel_a, "panel_b_clean_ground": panel_b}


def run_noise_and_clearance_sweeps(
    model_v91: torch.nn.Module,
    op: TerrainGravityOperator,
    device: torch.device,
    n_samples: int,
) -> tuple[dict, dict]:
    """Controlled white vs spatially correlated noise sweep and paired clearance sweep."""
    grid = (NX, NY, NZ, CELL, CELL, CELL)

    # 1. Noise sweep on clean ground scenes (zero 3D clutter so surface noise effect is isolated)
    ds_n = BodyTerrainDataset(n_samples, *grid, seed=8024, hard_prob=0.35, v6_mode=False)
    items_n = [ds_n[i] for i in range(n_samples)]
    rho_n = torch.stack([it[0] for it in items_n]).to(device)
    terr_n = torch.stack([it[1] for it in items_n]).to(device)
    clr_n = torch.stack([it[2] for it in items_n]).to(device)
    z_obs_n = terr_n - clr_n.view(-1, 1, 1)
    with torch.no_grad():
        clean_n = op(rho_n, z_obs_n)
    peak_n = clean_n.abs().amax(dim=(1, 2, 3), keepdim=True).clamp_min(1e-6)

    rng_noise = np.random.default_rng(8025)
    base_white_np = rng_noise.standard_normal(size=(n_samples, 1, NX, NY)).astype(np.float32)
    base_corr_np = np.empty_like(base_white_np)
    for i in range(n_samples):
        c_i = gaussian_filter(base_white_np[i, 0], sigma=1.5, mode="reflect")
        c_i = c_i / max(float(np.std(c_i)), 1e-6)
        base_corr_np[i, 0] = c_i
    base_white = torch.from_numpy(base_white_np).to(device)
    base_corr = torch.from_numpy(base_corr_np).to(device)

    noise_results = {"white": {}, "colored": {}}
    levels_white = (0.0, 0.01, 0.02, 0.05, 0.10)
    levels_colored = (0.01, 0.02, 0.05, 0.10)

    for kind, base_eps, lvls in (
        ("white", base_white, levels_white),
        ("colored", base_corr, levels_colored),
    ):
        for frac in lvls:
            d_obs_k = clean_n + float(frac) * peak_n * base_eps
            x6_k = build_v7_input(
                d_obs_k, terr_n, NZ * CELL, aspect=1.0, clearance=clr_n, detrend_modes=["raw"] * n_samples
            )
            samp_k = []
            for s0 in range(0, n_samples, 8):
                sl = slice(s0, s0 + 8)
                p_k, lv_k = forward_model_slice(model_v91, x6_k[sl], variant="v9.1", in_channels=6)
                sig_vec = (float(frac) * peak_n[sl, 0, 0, 0]) if frac > 0 else None
                _, slist = compute_extended_metrics_batch(
                    pred=p_k, truth=rho_n[sl], d_clean=clean_n[sl], z_obs=z_obs_n[sl],
                    forward_op=op, terrain=terr_n[sl], dz=CELL, log_var=lv_k,
                    d_obs=d_obs_k[sl], sigma_obs=sig_vec,
                )
                samp_k.extend(slist)
            agg_k = _merge_sample_list(samp_k)
            key = f"{int(round(frac * 100))}pct"
            noise_results[kind][key] = agg_k
            print(
                f"  [Noise {kind:7s} {key:4s}] IoU={agg_k['iou']:.4f} dz={agg_k['centroid_dz']:.1f}m "
                f"RMSE={agg_k['rmse']:.4f} misfit={agg_k['data_rel_misfit']:.4f}",
                flush=True,
            )

    # 2. Paired Survey Clearance Sweep on same N=24 scenes
    ds_c = BodyTerrainDataset(n_samples, *grid, seed=7024, hard_prob=0.35, v6_mode=True)
    items_c = [ds_c[i] for i in range(n_samples)]
    rho_c = torch.stack([it[0] for it in items_c]).to(device)
    uid_c = torch.stack([it[1] for it in items_c]).to(device)
    terr_c = torch.stack([it[2] for it in items_c]).to(device)
    nf_c = torch.full((n_samples,), 0.015, device=device, dtype=torch.float32)
    ns_c = torch.stack([it[5] for it in items_c])

    clearance_results = {}
    for h_m in (0.35, 5.0, 20.0, 60.0, 120.0, 160.0):
        clr_h = torch.full((n_samples,), float(h_m), device=device, dtype=torch.float32)
        obs_h, clean_h, z_obs_h, vm_h, dmodes_h = simulate_batch(
            op, rho_c, terr_c, clr_h, nf_c, ns_c,
            body_vox_noise=0.0, bg_vox_scale=0.0, dropout_prob=0.0,
            return_mask=True, v6_physics=True, detrend_probs=(0.0, 0.0, 0.0, 1.0),
            exterior_mass_prob=0.0, pregrid_blur_prob=0.0, line_drift_prob=0.0,
            return_detrend_modes=True,
        )
        x6_h = build_v7_input(
            obs_h, terr_c, NZ * CELL, valid_mask=vm_h, aspect=1.0, clearance=clr_h, detrend_modes=dmodes_h
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
            f"  [Clearance {key_h:5s}] IoU={agg_h['iou']:.4f} dz={agg_h['centroid_dz']:.1f}m "
            f"amp={agg_h['amp_ratio']:.4f} misfit={agg_h['data_rel_misfit']:.4f}",
            flush=True,
        )

    return noise_results, clearance_results


def export_showcase_slices(
    model_v91: torch.nn.Module,
    op: TerrainGravityOperator,
    device: torch.device,
    out_dir: Path,
) -> list[dict]:
    """Select 6 diverse composite geological scenes and save Truth/Pred/Sigma X-Z slices."""
    from dataset import NOISE_FRAC, RHO_AIR, RHO_LIGHT_MAX, RHO_LIGHT_MIN, RHO_WATER, random_bodies_v6

    nx, ny, nz, dx, dy, dz = NX, NY, NZ, CELL, CELL, CELL
    rng = np.random.default_rng(4024)

    def _peak_slice(rho_np):
        _, _, iy = np.unravel_index(np.argmax(np.abs(rho_np)), rho_np.shape)
        return rho_np[:, :, iy]

    def _n_distinct(sl):
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

    rhos, uids, terrains, clearances, nfracs, nseeds, labels = [], [], [], [], [], [], []
    for name, stype, pred_fn in specs:
        chosen = None
        for _ in range(120):
            t = random_terrain(rng, nx, ny, nz * dz)
            rho, uid = random_bodies_v6(rng, t, nz, dz, dx, hard_prob=0.0, large_prob=0.0, scene_type=stype or "isolated")
            c = np.float32(rng.uniform(0.30, 1.50))
            nf = np.float32(rng.uniform(*NOISE_FRAC))
            ns = int(rng.integers(0, 2**31 - 1))
            chosen = (rho, uid, t, c, nf, ns)
            if pred_fn(_peak_slice(rho)):
                break
        rho, uid, t, c, nf, ns = chosen
        rhos.append(torch.from_numpy(rho)[None])
        uids.append(torch.from_numpy(uid)[None])
        terrains.append(torch.from_numpy(t))
        clearances.append(torch.tensor(c))
        nfracs.append(torch.tensor(nf))
        nseeds.append(torch.tensor(ns))
        labels.append(name)

    st = torch.stack(rhos).to(device)
    suid = torch.stack(uids).to(device)
    sterr = torch.stack(terrains).to(device)
    sclr = torch.stack(clearances).to(device)
    snf = torch.stack(nfracs).to(device)
    sns = torch.stack(nseeds)

    obs, sc, sz, vm, dmodes = simulate_batch(
        op, st, sterr, sclr, snf, sns, return_mask=True, v6_physics=True,
        detrend_probs=(0.0, 0.0, 0.0, 1.0), dropout_prob=0.15,
        exterior_mass_prob=0.0, line_drift_prob=0.0, return_detrend_modes=True,
    )
    sx = build_v7_input(obs, sterr, nz * dz, valid_mask=vm, aspect=dz / dx, clearance=sclr, detrend_modes=dmodes)

    sp, slv = forward_model_slice(model_v91, sx, variant="v9.1", in_channels=6)
    with torch.no_grad():
        pf = op(sp, sz)
    ssig = torch.exp(0.5 * slv.clamp(-8.0, 4.0)) if slv is not None else torch.zeros_like(sp)

    _, per_s = compute_extended_metrics_batch(
        pred=sp, truth=st, d_clean=sc, z_obs=sz, forward_op=op, terrain=sterr, dz=CELL,
        log_var=slv, unit_id=suid, detrend_modes=dmodes,
    )

    dump = {}
    meta_list = []
    for j, lbl in enumerate(labels):
        t_np = st[j, 0].cpu().numpy()
        p_np = sp[j, 0].cpu().numpy()
        s_np = ssig[j, 0].cpu().numpy()
        _, _, iy_pk = np.unravel_index(np.argmax(np.abs(t_np)), t_np.shape)

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
            f"  [Showcase {j}: {lbl:18s}] IoU={m_j['iou']:.3f} dz={m_j['centroid_dz']:.1f}m "
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
    """Measure PyTorch CPU, ONNX CPU, ONNX CoreML (CPU_AND_NE), and classical solver latencies."""
    x1 = torch.zeros(1, 6, NX, NY)
    xx, yy = np.meshgrid(np.arange(NX) - 0.5 * NX, np.arange(NY) - 0.5 * NY, indexing="ij")
    x1[0, 0] = torch.from_numpy(1.5 * np.exp(-(xx ** 2 + yy ** 2) / 60.0)).float()
    x1[0, 1] = torch.from_numpy(0.05 * (1.0 + np.sin(xx / 9.0))).float()
    x1[0, 2] = 1.0
    x1[0, 3] = 0.0
    x1[0, 4] = 0.35 / CELL
    x1[0, 5] = 0.0

    # PyTorch CPU B=1
    with torch.no_grad():
        y_pt_d, y_pt_lv = model_v91.forward_uncertainty(x1)
        t_pt = []
        for _ in range(5):
            t0 = time.perf_counter()
            model_v91.forward_uncertainty(x1)
            t_pt.append((time.perf_counter() - t0) * 1000.0)

    onnx_path = str(out_dir / "v9.1_hard_eval.onnx")
    export_checkpoint_to_onnx(ckpt_hard_path, onnx_path, opset=17, verify=False, verify_mps=False)

    ort_cpu_ms = float("nan")
    ort_cpu_rel = float("nan")
    ort_cm_ms = float("nan")
    ort_cm_rel = float("nan")
    try:
        import onnxruntime as ort

        sess_cpu = ort.InferenceSession(onnx_path, providers=["CPUExecutionProvider"])
        x_np = x1.numpy()
        y_cpu = sess_cpu.run(None, {"observation_field": x_np})
        t_oc = []
        for _ in range(8):
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
                    sess_cm = ort.InferenceSession(
                        onnx_path, providers=[("CoreMLExecutionProvider", opts), "CPUExecutionProvider"]
                    )
                    if "CoreMLExecutionProvider" in sess_cm.get_providers():
                        y_cm = sess_cm.run(None, {"observation_field": x_np})
                        t_cm = []
                        for _ in range(8):
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
        "pytorch_cpu_ms": float(np.median(t_pt)),
        "onnx_cpu_ms": ort_cpu_ms,
        "onnx_cpu_max_rel_diff": ort_cpu_rel,
        "onnx_coreml_ne_ms": ort_cm_ms,
        "onnx_coreml_ne_max_rel_diff": ort_cm_rel,
        "hybrid_v9_1_focus_s": _med_canon("hybrid_v9_1_focus"),
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
            f"IoU={m['iou']:.4f} dz={m['centroid_dz']:.1f}m amp={m['amp_ratio']:.4f} RMSE={m['rmse']:.4f}",
            flush=True,
        )
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description="Run full v9.1 evaluation suite for the arXiv preprint.")
    ap.add_argument("--checkpoint-hard", default=os.path.join(_INV_PY_DIR, "checkpoints_v9.1_hard", "best_swin3d.pt"))
    ap.add_argument("--checkpoint-v3data", default=os.path.join(_INV_PY_DIR, "checkpoints_v9.1_v3data", "best_swin3d.pt"))
    ap.add_argument("--checkpoints-root", default=_INV_PY_DIR)
    ap.add_argument("--device", default="cpu")
    ap.add_argument("--n-per-bucket", type=int, default=32)
    ap.add_argument("--ablation-n-per-bucket", type=int, default=16)
    ap.add_argument("--clean-n", type=int, default=48)
    ap.add_argument("--clean-hard-n", type=int, default=32)
    ap.add_argument("--noise-n", type=int, default=24)
    ap.add_argument("--seed", type=int, default=2024)
    ap.add_argument("--output-dir", default=os.path.join(_PAPER_DIR, "results"))
    args = ap.parse_args()

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    dev = torch.device(args.device)
    grid = (NX, NY, NZ, CELL, CELL, CELL)

    print("=== 1. Initializing CPU Operators & Loading v9.1 Checkpoints ===", flush=True)
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
    print(f"Loaded v9.1_hard ep={ck_hard.get('epoch')} ({n_params_v91:,} params)", flush=True)

    print(f"\n=== 2. Flagship v9.1_hard 4-Bucket Evaluation (N={4 * args.n_per_bucket}) ===", flush=True)
    buckets_main = make_v6_val_buckets(
        args.n_per_bucket, args.seed, op_canon, op_bank, dev, grid,
        subvox_cap_bank=cap_bank, in_channels=6,
    )
    b_stats, comp_stats, all_samples, calib_data = eval_on_buckets(
        model_hard, buckets_main, variant="v9.1", in_channels=6, collect_calib=True
    )
    for bname, bm in b_stats.items():
        print(
            f"  [{bname:26s}] IoU={bm['iou']:.4f} dz={bm['centroid_dz']:.1f}m "
            f"RMSE={bm['rmse']:.4f} SSIM_d3={bm['ssim_d3']:.4f} amp={bm['amp_ratio']:.4f} "
            f"misfit={bm['data_misfit_eff']:.4f} cov={bm.get('fg_1sig_cov', float('nan')):.4f}",
            flush=True,
        )
    print(
        f"  [COMPOSITE WEIGHTED       ] IoU={comp_stats['iou']:.4f} dz={comp_stats['centroid_dz']:.1f}m "
        f"RMSE={comp_stats['rmse']:.4f} SSIM_d3={comp_stats['ssim_d3']:.4f} amp={comp_stats['amp_ratio']:.4f} "
        f"misfit={comp_stats['data_misfit_eff']:.4f} unc_sp={comp_stats.get('unc_spearman', float('nan')):.4f}",
        flush=True,
    )
    if calib_data is not None:
        np.savez_compressed(out_dir / "calib_arrays.npz", **calib_data)

    print("\n=== 3. Depth-Quartile Breakdown (from N=128 4-Bucket Samples) ===", flush=True)
    depth_quartiles = compute_depth_quartiles(all_samples)

    print(f"\n=== 4. Controlled Architectural Ablation (v3..v9.1) ===", flush=True)
    if args.ablation_n_per_bucket == args.n_per_bucket:
        buckets_abl = buckets_main
    else:
        buckets_abl = make_v6_val_buckets(
            args.ablation_n_per_bucket, args.seed, op_canon, op_bank, dev, grid,
            subvox_cap_bank=cap_bank, in_channels=6,
        )
    ablation_res = run_ablation_suite(
        Path(args.checkpoints_root), buckets_abl, dev, op_canon,
        args.clean_n, args.clean_hard_n, args.seed,
    )

    print("\n=== 5. 4 Canonical Geophysical Benchmarks vs Classical Inversion ===", flush=True)
    canonical_res = run_canonical_benchmarks(model_hard, op_canon, dev, out_dir)

    print(f"\n=== 6. Noise Sensitivity & Clearance Sweeps (N={args.noise_n}) ===", flush=True)
    noise_res, clearance_res = run_noise_and_clearance_sweeps(model_hard, op_canon, dev, args.noise_n)

    print("\n=== 7. Curated 6-Archetype Composite Showcase ===", flush=True)
    showcase_meta = export_showcase_slices(model_hard, op_canon, dev, out_dir)

    print("\n=== 8. Runtime & ONNX/CoreML Parity Benchmark ===", flush=True)
    timing_res = benchmark_runtimes(args.checkpoint_hard, model_hard, canonical_res, out_dir)

    summary = {
        "meta": {
            "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "checkpoint_hard": str(args.checkpoint_hard),
            "checkpoint_hard_epoch": int(ck_hard.get("epoch", -1)),
            "checkpoint_hard_params": int(n_params_v91),
            "checkpoint_v3data": str(args.checkpoint_v3data),
            "n_per_bucket": int(args.n_per_bucket),
            "ablation_n_per_bucket": int(args.ablation_n_per_bucket),
            "clean_n": int(args.clean_n),
            "clean_hard_n": int(args.clean_hard_n),
            "noise_n": int(args.noise_n),
            "seed": int(args.seed),
            "bucket_weights": V6_BUCKET_WEIGHTS,
        },
        "v91_buckets": b_stats,
        "v91_composite": comp_stats,
        "depth_quartiles": depth_quartiles,
        "ablation": ablation_res,
        "canonical": canonical_res,
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
