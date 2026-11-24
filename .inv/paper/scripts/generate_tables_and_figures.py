"""Generate auto-updated LaTeX macros, booktabs tables, and vector PDF figures
from ``.inv/paper/results/eval_results.json`` and training logs.

Outputs (flat inside ``.inv/paper/``):
  - ``macros_results.tex``
  - ``table_v91_buckets.tex``
  - ``table_ablation.tex``
  - ``table_canonical.tex``
  - ``table_noise.tex``
  - ``table_depth_clearance.tex``
  - ``table_timing.tex``
  - ``figures/fig1_architecture.pdf`` .. ``figures/fig6_uncertainty_calib.pdf``
  - Preview PNGs in ``results/preview_png/`` for visual inspection.
"""

from __future__ import annotations

import json
import math
import os
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import numpy as np

_THIS_DIR = Path(__file__).resolve().parent
_PAPER_DIR = _THIS_DIR.parent
_RESULTS_DIR = _PAPER_DIR / "results"
_FIG_DIR = _PAPER_DIR / "figures"
_PREVIEW_DIR = _RESULTS_DIR / "preview_png"
_INV_PY_DIR = _PAPER_DIR.parent / "python"

# Publication typography settings
plt.rcParams.update({
    "font.family": "serif",
    "font.serif": ["DejaVu Serif", "Times New Roman", "Times"],
    "mathtext.fontset": "stix",
    "font.size": 8.5,
    "axes.titlesize": 9.5,
    "axes.labelsize": 8.5,
    "xtick.labelsize": 7.5,
    "ytick.labelsize": 7.5,
    "legend.fontsize": 7.5,
    "axes.linewidth": 0.75,
    "pdf.fonttype": 42,
    "ps.fonttype": 42,
})


def _f(val, nd: int = 3, scale: float = 1.0) -> str:
    if val is None:
        return "---"
    try:
        fv = float(val) * scale
    except (TypeError, ValueError):
        return "---"
    if math.isnan(fv) or math.isinf(fv):
        return "---"
    return f"{fv:.{nd}f}"


def _fi(val) -> str:
    if val is None:
        return "---"
    try:
        fv = float(val)
    except (TypeError, ValueError):
        return "---"
    if math.isnan(fv) or math.isinf(fv):
        return "---"
    return f"{int(round(fv)):,}"


def _tex_escape(s: str) -> str:
    return (
        str(s)
        .replace("\\", r"\textbackslash{}")
        .replace("&", r"\&")
        .replace("%", r"\%")
        .replace("_", r"\_")
        .replace("#", r"\#")
    )


def _bold_best(vals: list[float], higher_is_better: bool = True, nd: int = 3, scale: float = 1.0) -> list[str]:
    clean = [float(v) for v in vals if v is not None and not math.isnan(float(v))]
    best = (max(clean) if higher_is_better else min(clean)) if clean else None
    out = []
    for v in vals:
        s = _f(v, nd=nd, scale=scale)
        if s != "---" and best is not None and abs(float(v) - best) <= 1e-12 + 1e-6 * max(1.0, abs(best)):
            out.append(rf"\textbf{{{s}}}")
        else:
            out.append(s)
    return out


def _save_fig(fig: plt.Figure, name: str) -> None:
    _FIG_DIR.mkdir(parents=True, exist_ok=True)
    _PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    pdf_p = _FIG_DIR / f"{name}.pdf"
    png_p = _PREVIEW_DIR / f"{name}.png"
    fig.savefig(pdf_p, bbox_inches="tight")
    fig.savefig(png_p, dpi=200, bbox_inches="tight")
    plt.close(fig)


def write_macros(res: dict) -> None:
    meta = res["meta"]
    comp = res["v91_composite"]
    bk = res["v91_buckets"]
    abl_a = res["ablation"]["panel_a_realistic"]
    abl_b = res["ablation"]["panel_b_clean_ground"]
    ns = res["noise_sweep"]
    dq = res["depth_quartiles"]
    clr = res["clearance_sweep"]
    can = res["canonical"]
    tm = res["timing"]

    def _mean_canon(mkey: str, metric: str) -> float:
        vals = [float(can[c]["methods"][mkey][metric]) for c in can if metric in can[c]["methods"][mkey]]
        return float(np.mean(vals)) if vals else float("nan")

    foc_s = tm.get("focusing_irls_s", float("nan"))
    pt_ms = tm.get("pytorch_cpu_ms", float("nan"))
    onnx_ms = tm.get("onnx_cpu_ms", float("nan"))
    cm_ms = tm.get("onnx_coreml_ne_ms", float("nan"))
    speed_onnx = (foc_s * 1000.0 / onnx_ms) if (not math.isnan(foc_s) and not math.isnan(onnx_ms) and onnx_ms > 0) else float("nan")
    speed_pt = (foc_s * 1000.0 / pt_ms) if (not math.isnan(foc_s) and not math.isnan(pt_ms) and pt_ms > 0) else float("nan")
    speed_cm = (foc_s * 1000.0 / cm_ms) if (not math.isnan(foc_s) and not math.isnan(cm_ms) and cm_ms > 0) else float("nan")

    # V9 vs V9.1 clean delta
    v9_m = abl_b.get("v9_v3data", {}).get("metrics", {})
    v91_m = abl_b.get("v9.1_v3data", {}).get("metrics", {})

    macros = {
        "valVNineOneEp": str(meta["checkpoint_hard_epoch"]),
        "valVNineOneParams": f"{meta['checkpoint_hard_params'] / 1e6:.2f}M",
        "valVNineOneParamsExact": _fi(meta["checkpoint_hard_params"]),
        "valVNineOneSlimParams": "1.36M",
        "valVNineOneHeavyParams": "6.90M",
        "valNBucketTotal": str(4 * meta["n_per_bucket"]),
        "valNAblBucket": str(4 * meta["ablation_n_per_bucket"]),
        "valNCleanStd": str(meta["clean_n"]),
        "valNCleanHard": str(meta["clean_hard_n"]),
        "valNNoise": str(meta["noise_n"]),

        "valCompIoU": _f(comp.get("iou"), 3),
        "valCompDz": _f(comp.get("centroid_dz"), 1),
        "valCompRMSE": _f(comp.get("rmse"), 3),
        "valCompFGRMSE": _f(comp.get("fg_rmse"), 3),
        "valCompSSIM": _f(comp.get("ssim_3d"), 3),
        "valCompSSIMd": _f(comp.get("ssim_d3"), 3),
        "valCompAmp": _f(comp.get("amp_ratio"), 3),
        "valCompMisfit": _f(comp.get("data_misfit_eff"), 2, 100.0),
        "valCompBodyLOne": _f(comp.get("body_l1_rel"), 3),
        "valCompHalo": _f(comp.get("halo_opp"), 2, 1e4),
        "valCompUncSp": _f(comp.get("unc_spearman"), 3),
        "valCompFgCov": _f(comp.get("fg_1sig_cov"), 1, 100.0),
        "valCompAURC": _f(comp.get("aurc"), 4),
        "valCompBdRatio": _f(comp.get("bd_sig_err_ratio"), 2),

        "valGCIoU": _f(bk["ground_clean"].get("iou"), 3),
        "valHLIoU": _f(bk["ground_hard_large_detrend"].get("iou"), 3),
        "valAirIoU": _f(bk["airborne_relief_drape"].get("iou"), 3),
        "valMAIoU": _f(bk["multi_aspect"].get("iou"), 3),
        "valGCDz": _f(bk["ground_clean"].get("centroid_dz"), 1),
        "valHLDz": _f(bk["ground_hard_large_detrend"].get("centroid_dz"), 1),
        "valAirDz": _f(bk["airborne_relief_drape"].get("centroid_dz"), 1),
        "valMADz": _f(bk["multi_aspect"].get("centroid_dz"), 1),

        "valAblVSixIoU": _f(abl_a.get("v6", {}).get("metrics", {}).get("iou"), 3),
        "valAblVSixDz": _f(abl_a.get("v6", {}).get("metrics", {}).get("centroid_dz"), 1),
        "valAblVSixOneIoU": _f(abl_a.get("v6.1", {}).get("metrics", {}).get("iou"), 3),
        "valAblVSixOneDz": _f(abl_a.get("v6.1", {}).get("metrics", {}).get("centroid_dz"), 1),
        "valAblVSevenIoU": _f(abl_a.get("v7", {}).get("metrics", {}).get("iou"), 3),
        "valAblVSevenDz": _f(abl_a.get("v7", {}).get("metrics", {}).get("centroid_dz"), 1),
        "valAblVEightIoU": _f(abl_a.get("v8", {}).get("metrics", {}).get("iou"), 3),
        "valAblVEightDz": _f(abl_a.get("v8", {}).get("metrics", {}).get("centroid_dz"), 1),
        "valAblVNineOneIoU": _f(abl_a.get("v9.1_hard", {}).get("metrics", {}).get("iou"), 3),
        "valAblVNineOneDz": _f(abl_a.get("v9.1_hard", {}).get("metrics", {}).get("centroid_dz"), 1),

        "valCleanVThreeIoU": _f(abl_b.get("v3", {}).get("metrics", {}).get("iou"), 3),
        "valCleanVThreeDz": _f(abl_b.get("v3", {}).get("metrics", {}).get("centroid_dz"), 1),
        "valCleanVFiveTwoIoU": _f(abl_b.get("v5.2", {}).get("metrics", {}).get("iou"), 3),
        "valCleanVFiveTwoDz": _f(abl_b.get("v5.2", {}).get("metrics", {}).get("centroid_dz"), 1),
        "valCleanVEightIoU": _f(abl_b.get("v8_v3data", {}).get("metrics", {}).get("iou"), 3),
        "valCleanVEightDz": _f(abl_b.get("v8_v3data", {}).get("metrics", {}).get("centroid_dz"), 1),
        "valCleanVNineIoU": _f(v9_m.get("iou"), 3),
        "valCleanVNineDz": _f(v9_m.get("centroid_dz"), 1),
        "valCleanVNineOneIoU": _f(v91_m.get("iou"), 3),
        "valCleanVNineOneDz": _f(v91_m.get("centroid_dz"), 1),
        "valCleanVNineOneMisfit": _f(v91_m.get("data_rel_misfit"), 2, 100.0),
        "valCleanHardXferIoU": _f(abl_b.get("v9.1_hard_xfer", {}).get("metrics", {}).get("iou"), 3),
        "valCleanHardXferDz": _f(abl_b.get("v9.1_hard_xfer", {}).get("metrics", {}).get("centroid_dz"), 1),

        "valNoiseZeroIoU": _f(ns["white"]["0pct"].get("iou"), 3),
        "valNoiseFiveWhiteIoU": _f(ns["white"]["5pct"].get("iou"), 3),
        "valNoiseTenWhiteIoU": _f(ns["white"]["10pct"].get("iou"), 3),
        "valNoiseTenWhiteDz": _f(ns["white"]["10pct"].get("centroid_dz"), 1),
        "valNoiseTenColoredIoU": _f(ns["colored"]["10pct"].get("iou"), 3),
        "valNoiseTenColoredDz": _f(ns["colored"]["10pct"].get("centroid_dz"), 1),

        "valQOneIoU": _f(dq[0]["metrics"].get("iou"), 3),
        "valQOneDz": _f(dq[0]["metrics"].get("centroid_dz"), 1),
        "valQOneRange": f"{dq[0]['depth_lo_m']:.0f}--{dq[0]['depth_hi_m']:.0f}",
        "valQFourIoU": _f(dq[3]["metrics"].get("iou"), 3),
        "valQFourDz": _f(dq[3]["metrics"].get("centroid_dz"), 1),
        "valQFourAmp": _f(dq[3]["metrics"].get("amp_ratio"), 3),
        "valQFourRange": f"{dq[3]['depth_lo_m']:.0f}--{dq[3]['depth_hi_m']:.0f}",

        "valClrGroundIoU": _f(clr["0.35m"].get("iou"), 3),
        "valClrHighIoU": _f(clr["160m"].get("iou"), 3),
        "valClrGroundDz": _f(clr["0.35m"].get("centroid_dz"), 1),
        "valClrHighDz": _f(clr["160m"].get("centroid_dz"), 1),

        "valCanonMeanTikIoU": _f(_mean_canon("tikhonov_l2", "iou"), 3),
        "valCanonMeanFocIoU": _f(_mean_canon("focusing_irls", "iou"), 3),
        "valCanonMeanSwinIoU": _f(_mean_canon("swin3d_v9_1", "iou"), 3),
        "valCanonMeanHybIoU": _f(_mean_canon("hybrid_v9_1_focus", "iou"), 3),
        "valCanonMeanTikDz": _f(_mean_canon("tikhonov_l2", "centroid_dz"), 1),
        "valCanonMeanFocDz": _f(_mean_canon("focusing_irls", "centroid_dz"), 1),
        "valCanonMeanSwinDz": _f(_mean_canon("swin3d_v9_1", "centroid_dz"), 1),
        "valCanonMeanHybDz": _f(_mean_canon("hybrid_v9_1_focus", "centroid_dz"), 1),
        "valCanonMeanTikRMSE": _f(_mean_canon("tikhonov_l2", "rmse"), 3),
        "valCanonMeanFocRMSE": _f(_mean_canon("focusing_irls", "rmse"), 3),
        "valCanonMeanSwinRMSE": _f(_mean_canon("swin3d_v9_1", "rmse"), 3),
        "valCanonMeanHybRMSE": _f(_mean_canon("hybrid_v9_1_focus", "rmse"), 3),
        "valCanonMeanTikSSIM": _f(_mean_canon("tikhonov_l2", "ssim_d3"), 3),
        "valCanonMeanFocSSIM": _f(_mean_canon("focusing_irls", "ssim_d3"), 3),
        "valCanonMeanSwinSSIM": _f(_mean_canon("swin3d_v9_1", "ssim_d3"), 3),
        "valCanonMeanHybSSIM": _f(_mean_canon("hybrid_v9_1_focus", "ssim_d3"), 3),
        "valCanonMeanHybMisfit": _f(_mean_canon("hybrid_v9_1_focus", "data_misfit_eff"), 2, 100.0),

        "valTimePtMs": _f(pt_ms, 1),
        "valTimeOnnxCpuMs": _f(onnx_ms, 1),
        "valTimeCoremlMs": _f(cm_ms, 1),
        "valTimeOnnxCpuDiff": f"{tm['onnx_cpu_max_rel_diff']:.2e}" if not math.isnan(tm.get("onnx_cpu_max_rel_diff", float("nan"))) else "---",
        "valTimeCoremlDiff": f"{tm['onnx_coreml_ne_max_rel_diff']:.2e}" if not math.isnan(tm.get("onnx_coreml_ne_max_rel_diff", float("nan"))) else "---",
        "valTimeHybridS": _f(tm.get("hybrid_v9_1_focus_s"), 2),
        "valTimeTikS": _f(tm.get("tikhonov_l2_s"), 2),
        "valTimeFocS": _f(foc_s, 2),
        "valSpeedupOnnxVsFoc": f"{speed_onnx:.0f}$\\times$" if not math.isnan(speed_onnx) else "---",
        "valSpeedupPtVsFoc": f"{speed_pt:.0f}$\\times$" if not math.isnan(speed_pt) else "---",
        "valSpeedupCoremlVsFoc": f"{speed_cm:.0f}$\\times$" if not math.isnan(speed_cm) else "---",
    }

    lines = ["% Auto-generated by scripts/generate_tables_and_figures.py — do not edit manually.\n"]
    for k, v in macros.items():
        lines.append(rf"\newcommand{{\{k}}}{{{v}}}" + "\n")
    (_PAPER_DIR / "macros_results.tex").write_text("".join(lines), encoding="utf-8")


def write_tables(res: dict) -> None:
    # 1. table_v91_buckets.tex
    bk = res["v91_buckets"]
    comp = res["v91_composite"]
    weights = res["meta"]["bucket_weights"]
    rows_b = [
        ("ground_clean", "Ground Clean", f"{weights['ground_clean']:.2f}"),
        ("ground_hard_large_detrend", "Hard-Large Detrended", f"{weights['ground_hard_large_detrend']:.2f}"),
        ("airborne_relief_drape", "Airborne Relief Drape", f"{weights['airborne_relief_drape']:.2f}"),
        ("multi_aspect", "Multi-Aspect ($dz/dx$)", f"{weights['multi_aspect']:.2f}"),
    ]
    t1 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Multi-regime validation performance of \texttt{Swin3D v9.1} (\valVNineOneParams{} parameters, epoch \valVNineOneEp{}) across the four stratified benchmarks ($N=\valNBucketTotal{}$ total scenes).}",
        r"\label{tab:v91_buckets}",
        r"\small",
        r"\setlength{\tabcolsep}{4.5pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{lccccccccc}",
        r"\toprule",
        r"Regime Bucket & Wt. & IoU $\uparrow$ & RMSE $\downarrow$ & FG-RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & 1-$\sigma$ Cov. \\",
        r"\midrule",
    ]
    for key, label, wt in rows_b:
        m = bk[key]
        t1.append(
            f"{label} & {wt} & {_f(m.get('iou'),3)} & {_f(m.get('rmse'),3)} & {_f(m.get('fg_rmse'),3)} & "
            f"{_f(m.get('ssim_d3'),3)} & {_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & "
            f"{_f(m.get('data_misfit_eff'),2,100.0)} & {_f(m.get('fg_1sig_cov'),1,100.0)}\\% \\\\"
        )
    t1.append(r"\midrule")
    t1.append(
        rf"\textbf{{Weighted Composite}} & 1.00 & \textbf{{{_f(comp.get('iou'),3)}}} & \textbf{{{_f(comp.get('rmse'),3)}}} & "
        rf"\textbf{{{_f(comp.get('fg_rmse'),3)}}} & \textbf{{{_f(comp.get('ssim_d3'),3)}}} & "
        rf"\textbf{{{_f(comp.get('centroid_dz'),1)}}} & \textbf{{{_f(comp.get('amp_ratio'),3)}}} & "
        rf"\textbf{{{_f(comp.get('data_misfit_eff'),2,100.0)}}} & \textbf{{{_f(comp.get('fg_1sig_cov'),1,100.0)}\%}} \\"
    )
    t1.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_v91_buckets.tex").write_text("\n".join(t1), encoding="utf-8")

    # 2. table_ablation.tex
    abl_a = res["ablation"]["panel_a_realistic"]
    abl_b = res["ablation"]["panel_b_clean_ground"]

    def _emit_ablation_panel(specs: list[tuple[str, str]], panel_dict: dict) -> list[str]:
        keys = [k for k, _ in specs if k in panel_dict]
        ious = _bold_best([panel_dict[k]["metrics"]["iou"] for k in keys], True, 3)
        dzs = _bold_best([panel_dict[k]["metrics"]["centroid_dz"] for k in keys], False, 1)
        amps = _bold_best([1.0 - abs(1.0 - panel_dict[k]["metrics"]["amp_ratio"]) for k in keys], True, 3)
        # Render actual amp_ratio with best closest to 1
        amp_raw = [panel_dict[k]["metrics"]["amp_ratio"] for k in keys]
        best_amp_idx = int(np.argmin([abs(1.0 - a) for a in amp_raw])) if amp_raw else -1
        amps_str = [
            (rf"\textbf{{{_f(a,3)}}}" if i == best_amp_idx else _f(a, 3))
            for i, a in enumerate(amp_raw)
        ]
        misfits = _bold_best([panel_dict[k]["metrics"]["data_misfit_eff"] for k in keys], False, 2, 100.0)
        rmses = _bold_best([panel_dict[k]["metrics"]["rmse"] for k in keys], False, 3)
        ssims = _bold_best([panel_dict[k]["metrics"]["ssim_d3"] for k in keys], True, 3)
        out = []
        for idx, k in enumerate(keys):
            d = panel_dict[k]
            lbl = dict(specs)[k]
            p_m = f"{d['params'] / 1e6:.2f}M"
            out.append(
                f"{lbl} & {p_m} & {d['in_channels']} & {_tex_escape(d['desc'])} & "
                f"{ious[idx]} & {dzs[idx]} & {amps_str[idx]} & {misfits[idx]} & {rmses[idx]} & {ssims[idx]} \\\\"
            )
        return out

    specs_a = [
        ("v6", r"\texttt{v6}"),
        ("v6.1", r"\texttt{v6.1}"),
        ("v7", r"\texttt{v7}"),
        ("v8", r"\texttt{v8}"),
        ("v9.1_hard", r"\textbf{\texttt{v9.1}} (ours)"),
    ]
    specs_b = [
        ("v3", r"\texttt{v3}"),
        ("v5.2", r"\texttt{v5.2}"),
        ("v8_v3data", r"\texttt{v8} (clean)"),
        ("v9_v3data", r"\texttt{v9} (clean)"),
        ("v9.1_v3data", r"\textbf{\texttt{v9.1}} (clean)"),
        ("v9.1_hard_xfer", r"\texttt{v9.1} (zero-shot xfer)"),
    ]
    t2 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Controlled architectural ablation across identical seeded validation sets. Panel~A evaluates the 4-bucket realistic survey suite ($N=\valNAblBucket{}$); Panel~B evaluates the clean ground benchmark ($N=\valNCleanStd{}$ standard + $\valNCleanHard{}$ hard scenes). Bold marks best per panel.}",
        r"\label{tab:ablation}",
        r"\small",
        r"\setlength{\tabcolsep}{4pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{lcllcccccc}",
        r"\toprule",
        r"Model & Params & Ch. & Key Configuration & IoU $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ \\",
        r"\midrule",
        r"\multicolumn{10}{l}{\textit{Panel A: Multi-Regime Realistic Survey Suite (ground, hard-detrended, airborne drape, multi-aspect)}} \\",
        *_emit_ablation_panel(specs_a, abl_a),
        r"\midrule",
        r"\multicolumn{10}{l}{\textit{Panel B: Clean Ground Survey Benchmark (standard + hard composite targets)}} \\",
        *_emit_ablation_panel(specs_b, abl_b),
        r"\bottomrule",
        r"\end{tabular}%",
        r"}",
        r"\end{table}",
        "",
    ]
    (_PAPER_DIR / "table_ablation.tex").write_text("\n".join(t2), encoding="utf-8")

    # 3. table_canonical.tex
    can = res["canonical"]
    canon_order = ("block_ore", "dipping_dyke", "twin_opposing", "sedimentary_basin")
    method_order = (
        ("tikhonov_l2", r"Tikhonov $L_2$"),
        ("focusing_irls", r"Focusing IRLS"),
        ("swin3d_v9_1", r"\texttt{Swin3D v9.1}"),
        ("hybrid_v9_1_focus", r"Hybrid \texttt{v9.1}+Focus"),
    )
    t3 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Head-to-head comparison against classical Tikhonov $L_2$ and Portniaguine--Zhdanov focusing IRLS inversion on four canonical 3D benchmarks (2\% surface noise). Bold marks best method per benchmark.}",
        r"\label{tab:canonical}",
        r"\small",
        r"\setlength{\tabcolsep}{4pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{llcccccccc}",
        r"\toprule",
        r"Benchmark & Method & IoU $\uparrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & $\chi^2_{\mathrm{red}}$ $\to 1$ & Time [s] $\downarrow$ \\",
        r"\midrule",
    ]
    for ci, cname in enumerate(canon_order):
        cobj = can[cname]
        mkeys = [mk for mk, _ in method_order]
        ms = [cobj["methods"][mk] for mk in mkeys]
        ious = _bold_best([m["iou"] for m in ms], True, 3)
        rmses = _bold_best([m["rmse"] for m in ms], False, 3)
        ssims = _bold_best([m["ssim_d3"] for m in ms], True, 3)
        dzs = _bold_best([m["centroid_dz"] for m in ms], False, 1)
        best_amp_i = int(np.argmin([abs(1.0 - m["amp_ratio"]) for m in ms]))
        amps = [rf"\textbf{{{_f(m['amp_ratio'],3)}}}" if idx == best_amp_i else _f(m["amp_ratio"], 3) for idx, m in enumerate(ms)]
        misfits = _bold_best([m["data_misfit_eff"] for m in ms], False, 2, 100.0)
        times = _bold_best([m["wall_time_s"] for m in ms], False, 2)
        for idx, (mk, mlbl) in enumerate(method_order):
            b_cell = rf"\multirow{{4}}{{*}}{{{cobj['title']}}}" if idx == 0 else ""
            t3.append(
                f"{b_cell} & {mlbl} & {ious[idx]} & {rmses[idx]} & {ssims[idx]} & "
                f"{dzs[idx]} & {amps[idx]} & {misfits[idx]} & {_f(ms[idx].get('chi2_red'),2)} & {times[idx]} \\\\"
            )
        t3.append(r"\midrule")

    # Mean across the 4 benchmarks
    mean_ms = []
    for mk, _ in method_order:
        mean_ms.append({
            k: float(np.mean([can[c]["methods"][mk][k] for c in canon_order]))
            for k in ("iou", "rmse", "ssim_d3", "centroid_dz", "amp_ratio", "data_misfit_eff", "chi2_red", "wall_time_s")
        })
    ious = _bold_best([m["iou"] for m in mean_ms], True, 3)
    rmses = _bold_best([m["rmse"] for m in mean_ms], False, 3)
    ssims = _bold_best([m["ssim_d3"] for m in mean_ms], True, 3)
    dzs = _bold_best([m["centroid_dz"] for m in mean_ms], False, 1)
    best_amp_i = int(np.argmin([abs(1.0 - m["amp_ratio"]) for m in mean_ms]))
    amps = [rf"\textbf{{{_f(m['amp_ratio'],3)}}}" if idx == best_amp_i else _f(m["amp_ratio"], 3) for idx, m in enumerate(mean_ms)]
    misfits = _bold_best([m["data_misfit_eff"] for m in mean_ms], False, 2, 100.0)
    times = _bold_best([m["wall_time_s"] for m in mean_ms], False, 2)
    for idx, (mk, mlbl) in enumerate(method_order):
        b_cell = r"\multirow{4}{*}{\textbf{Mean (4 Benchmarks)}}" if idx == 0 else ""
        t3.append(
            f"{b_cell} & {mlbl} & {ious[idx]} & {rmses[idx]} & {ssims[idx]} & "
            f"{dzs[idx]} & {amps[idx]} & {misfits[idx]} & {_f(mean_ms[idx]['chi2_red'],2)} & {times[idx]} \\\\"
        )
    t3.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_canonical.tex").write_text("\n".join(t3), encoding="utf-8")

    # 4. table_noise.tex
    ns = res["noise_sweep"]
    t4 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Sensitivity of \texttt{Swin3D v9.1} to surface measurement noise level and spatial correlation ($N=\valNNoise{}$ ground-survey scenes).}",
        r"\label{tab:noise}",
        r"\small",
        r"\setlength{\tabcolsep}{5pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{lcccccccc}",
        r"\toprule",
        r"Noise Type & $\sigma / \max|d|$ & IoU $\uparrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & $\chi^2_{\mathrm{red}}$ \\",
        r"\midrule",
    ]
    for kind, klbl, keys in (
        ("white", "White Gaussian", ("0pct", "1pct", "2pct", "5pct", "10pct")),
        ("colored", r"Correlated ($\sigma_c=1.5\Delta x$)", ("1pct", "2pct", "5pct", "10pct")),
    ):
        for idx, k in enumerate(keys):
            m = ns[kind][k]
            pct_str = k.replace("pct", r"\%")
            type_cell = rf"\multirow{{{len(keys)}}}{{*}}{{{klbl}}}" if idx == 0 else ""
            t4.append(
                f"{type_cell} & {pct_str} & {_f(m.get('iou'),3)} & {_f(m.get('rmse'),3)} & "
                f"{_f(m.get('ssim_d3'),3)} & {_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & "
                f"{_f(m.get('data_rel_misfit'),2,100.0)} & {_f(m.get('chi2_red'),2)} \\\\"
            )
        if kind == "white":
            t4.append(r"\midrule")
    t4.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_noise.tex").write_text("\n".join(t4), encoding="utf-8")

    # 5. table_depth_clearance.tex
    dq = res["depth_quartiles"]
    clr = res["clearance_sweep"]
    t5 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Depth-quartile resolution across the $N=\valNBucketTotal{}$ validation suite (Panel~A) and paired sensor-clearance sweep at fixed 1.5\% noise ($N=\valNNoise{}$, Panel~B).}",
        r"\label{tab:depth_clearance}",
        r"\small",
        r"\setlength{\tabcolsep}{5.5pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{lccccccc}",
        r"\toprule",
        r"Sub-group / Regime & $N$ & IoU $\uparrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ \\",
        r"\midrule",
        r"\multicolumn{8}{l}{\textit{Panel A: Source Depth Quartiles (mass-weighted depth below terrain)}} \\",
    ]
    for q in dq:
        m = q["metrics"]
        rng_s = f"{q['quartile']} ({q['depth_lo_m']:.0f}--{q['depth_hi_m']:.0f}\\,m)"
        t5.append(
            f"{rng_s} & {q['n']} & {_f(m.get('iou'),3)} & {_f(m.get('rmse'),3)} & {_f(m.get('ssim_d3'),3)} & "
            f"{_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & {_f(m.get('data_misfit_eff'),2,100.0)} \\\\"
        )
    t5.append(r"\midrule")
    t5.append(r"\multicolumn{8}{l}{\textit{Panel B: Paired Survey Clearance Height Above Terrain}} \\")
    for ck in ("0.35m", "5m", "20m", "60m", "120m", "160m"):
        m = clr[ck]
        t5.append(
            f"Clearance $h = {ck[:-1]}$\\,m & {res['meta']['noise_n']} & {_f(m.get('iou'),3)} & {_f(m.get('rmse'),3)} & "
            f"{_f(m.get('ssim_d3'),3)} & {_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & {_f(m.get('data_rel_misfit'),2,100.0)} \\\\"
        )
    t5.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_depth_clearance.tex").write_text("\n".join(t5), encoding="utf-8")

    # 6. table_timing.tex
    tm = res["timing"]
    foc_s = tm.get("focusing_irls_s", float("nan"))
    rows_t = []
    if not math.isnan(tm.get("onnx_coreml_ne_ms", float("nan"))):
        ms = tm["onnx_coreml_ne_ms"]
        rows_t.append((
            r"\texttt{Swin3D v9.1} (ONNX CoreML ANE/CPU)",
            f"{ms:.1f}\\,ms",
            f"{foc_s * 1000.0 / ms:.0f}$\\times$",
            f"{tm['onnx_coreml_ne_max_rel_diff']:.2e}",
        ))
    rows_t.append((
        r"\texttt{Swin3D v9.1} (ONNX Runtime CPU)",
        f"{tm['onnx_cpu_ms']:.1f}\\,ms",
        f"{foc_s * 1000.0 / tm['onnx_cpu_ms']:.0f}$\\times$",
        f"{tm['onnx_cpu_max_rel_diff']:.2e}",
    ))
    rows_t.append((
        r"\texttt{Swin3D v9.1} (PyTorch CPU)",
        f"{tm['pytorch_cpu_ms']:.1f}\\,ms",
        f"{foc_s * 1000.0 / tm['pytorch_cpu_ms']:.0f}$\\times$",
        "Reference",
    ))
    rows_t.append((
        r"Hybrid \texttt{v9.1} + Focusing IRLS (6 iters)",
        f"{tm['hybrid_v9_1_focus_s']:.2f}\\,s",
        f"{foc_s / tm['hybrid_v9_1_focus_s']:.1f}$\\times$",
        "---",
    ))
    rows_t.append((
        r"Classical Tikhonov $L_2$ (6 iters)",
        f"{tm['tikhonov_l2_s']:.2f}\\,s",
        f"{foc_s / tm['tikhonov_l2_s']:.1f}$\\times$",
        "---",
    ))
    rows_t.append((
        r"Classical Focusing IRLS (12 iters)",
        f"{foc_s:.2f}\\,s",
        "1.0$\\times$",
        "---",
    ))
    t6 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Single-volume ($64\times 64\times 32$) wall-clock runtime and ONNX numerical parity on a MacBook Air (Apple~M4, 32\,GB unified memory) CPU/Neural Engine.}",
        r"\label{tab:timing}",
        r"\small",
        r"\setlength{\tabcolsep}{4pt}",
        r"\begin{tabular}{@{}lccc@{}}",
        r"\toprule",
        r"Solver / Execution Backend & Median Runtime & Speedup vs.\ IRLS & Max Rel.\ Parity Diff \\",
        r"\midrule",
    ]
    for r_lbl, r_t, r_sp, r_df in rows_t:
        t6.append(f"{r_lbl} & {r_t} & {r_sp} & {r_df} \\\\")
    t6.extend([r"\bottomrule", r"\end{tabular}", r"\end{table}", ""])
    (_PAPER_DIR / "table_timing.tex").write_text("\n".join(t6), encoding="utf-8")


def draw_fig1_architecture(res: dict) -> None:
    """Clean schematic architecture diagram of PotentialFieldTransformer3DV9 (v9.1)."""
    fig, ax = plt.subplots(figsize=(13.5, 5.8))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 44)
    ax.axis("off")

    def box(x, y, w, h, title, sub="", fc="#eef2ff", ec="#3730a3", lw=1.2, ls="-", fs=8.0):
        p = FancyBboxPatch(
            (x, y), w, h, boxstyle="round,pad=0.35,rounding_size=0.8",
            facecolor=fc, edgecolor=ec, linewidth=lw, linestyle=ls,
        )
        ax.add_patch(p)
        if sub:
            ax.text(x + w / 2, y + h * 0.65, title, ha="center", va="center", fontsize=fs, fontweight="bold", color="#111827")
            ax.text(x + w / 2, y + h * 0.28, sub, ha="center", va="center", fontsize=fs - 1.2, color="#374151")
        else:
            ax.text(x + w / 2, y + h / 2, title, ha="center", va="center", fontsize=fs, fontweight="bold", color="#111827")

    def arrow(x0, y0, x1, y1, color="#1f2937", lw=1.2, ls="-", rad=0.0):
        arr = FancyArrowPatch(
            (x0, y0), (x1, y1),
            arrowstyle="-|>,head_length=4.5,head_width=2.8",
            color=color, linewidth=lw, linestyle=ls,
            connectionstyle=f"arc3,rad={rad}",
        )
        ax.add_patch(arr)

    # Row 1 (y=32..40): Input -> Stem -> 2D U-Net -> Lifter
    box(1.5, 32, 14.5, 8, "6-Ch Survey Input", "(1, 6, 64, 64)\n" + r"$g_z$, terr, mask, asp, clr, dt", fc="#eff6ff", ec="#1d4ed8")
    box(19.0, 32, 16.5, 8, "19-Ch Physics Stem", "asinh, multi-scale LP,\n" + r"$g_{zz}$, Euler, struct. tensor", fc="#eff6ff", ec="#1d4ed8")
    box(38.5, 32, 16.5, 8, "2D U-Net + Global Attn", r"enc0 ($64^2$) $\rightarrow$ enc2 ($16^2$)" + "\n+ 9-D scale MLP cond.", fc="#f0fdf4", ec="#15803d")
    box(58.0, 32, 18.0, 8, "2D-to-3D Depth Lifter", r"$(8\times 16\times 16\times 128)$ tokens" + "\n" + r"+ $1/d^2$ sensitivity cond.", fc="#f0fdf4", ec="#15803d")
    box(79.0, 32, 19.5, 8, "3D Swin + Axial Blocks", r"$6\times$ alternating W/SW-MSA" + "\n" + r"+ Green's Vertical Bias $B_z$", fc="#fef3c7", ec="#b45309")

    arrow(16.3, 36, 18.7, 36)
    arrow(35.8, 36, 38.2, 36)
    arrow(55.3, 36, 57.7, 36)
    arrow(76.3, 36, 78.7, 36)

    # Row 2 (y=17..26): Prototypes -> Stage 1 -> OddPyramid Mid -> Stage 2
    box(79.0, 18, 19.5, 8, "7-Class Lithology Attn", r"voxels $\leftrightarrow$ 21 prototypes" + "\n(bg, air, water, 4 rock bands)", fc="#fef3c7", ec="#b45309")
    box(54.0, 18, 21.0, 8, "Stage-1 Decoder (16x32x32)", "SepSubpixelUp + DepthSkip\n" + r"$\rightarrow$ occ $\cdot$ coarse $\rho$", fc="#f3e8ff", ec="#6b21a8")
    box(29.0, 18, 21.0, 8, "Stage-1 Physics Residual", "Frozen OddPyramidGravity\n" + r"+ BorderProj + $G^T r_{\mathrm{mid}}$", fc="#ffe4e6", ec="#be123c")
    box(2.5, 18, 22.5, 8, "Stage-2 Decoder (32x64x64)", "Unified full-res SepSubpixelUp\n" + r"+ DepthSkip $\rightarrow \rho_{\mathrm{init}}$", fc="#f3e8ff", ec="#6b21a8")

    arrow(88.7, 31.7, 88.7, 26.3)
    arrow(78.7, 22, 75.3, 22)
    arrow(53.7, 22, 50.3, 22)
    arrow(28.7, 22, 25.3, 22)

    # Row 3 (y=2.5..11.5): Stage 2 Physics Feedback -> Final Heads -> Hybrid IRLS
    box(2.5, 3, 24.0, 8.5, "Stage-2 Physics Feedback", "Frozen OddPyramidGravity ($64^3$)\n" + r"$D_1^{-1} G^T(r_{64})\cdot w_{\mathrm{bp}}(z)$" + "\n+ 2D residual gate", fc="#ffe4e6", ec="#be123c")
    box(30.5, 3, 21.0, 8.5, "Refinement & Mixture Head", "SepConv25D + depth prior\n+ occ-gated 7-class bounds", fc="#ecfeff", ec="#0e7490")
    box(55.5, 3, 20.5, 8.5, "Dual 3D Volume Outputs", r"Density $\Delta\hat{\rho}$ ($32\times 64\times 64$)" + "\n" + r"+ Aleatoric $\hat{\sigma}_\rho = \exp(s/2)$", fc="#ecfeff", ec="#0e7490")
    box(79.5, 3, 19.0, 8.5, "Optional Hybrid IRLS", r"C++ FocusingSolver ($S_{MS}+S_{MGS}$)" + "\n" + r"weighted by $1/\hat{\sigma}_\rho$ prior", fc="#f1f5f9", ec="#334155", ls="--")

    arrow(13.7, 17.7, 13.7, 11.8)
    arrow(26.8, 7.2, 30.2, 7.2)
    arrow(51.8, 7.2, 55.2, 7.2)
    arrow(76.3, 7.2, 79.2, 7.2, ls="--")

    # Skip connections
    arrow(51.0, 31.7, 64.5, 26.3, color="#15803d", lw=1.0, ls=":", rad=-0.08)
    ax.text(58.8, 29.2, "f1 skip", fontsize=7.0, color="#15803d", style="italic",
            bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none", alpha=0.85))
    arrow(42.5, 31.7, 13.7, 26.3, color="#15803d", lw=1.0, ls=":", rad=0.08)
    ax.text(26.2, 29.2, "f0 skip", fontsize=7.0, color="#15803d", style="italic",
            bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none", alpha=0.85))

    ax.text(50.0, 42.3, f"PotentialFieldTransformer3DV9 (v9.1 Base, {res['meta']['checkpoint_hard_params']:,} Params) & Hybrid Focusing Pipeline",
            ha="center", va="center", fontsize=10.5, fontweight="bold", color="#0f172a")
    _save_fig(fig, "fig1_architecture")


def draw_fig2_training_ablation(res: dict) -> None:
    """3-panel convergence, weighted loss share, and architectural ablation plot."""
    hist_p = _INV_PY_DIR / "checkpoints_v9.1_hard" / "training_history.json"
    with open(hist_p, encoding="utf-8") as f:
        hist_raw = json.load(f)["epochs"]
    # Keep latest entry per epoch and sort monotonically
    by_ep = {int(e["epoch"]): e for e in hist_raw}
    hist = [by_ep[k] for k in sorted(by_ep.keys())]

    ep = np.array([e["epoch"] for e in hist])
    val_loss = np.array([e["val_loss"] for e in hist])
    iou = np.array([e["iou"] for e in hist])
    dz = np.array([e["centroid_dz"] for e in hist])

    best_idx = int(np.argmin(val_loss))
    best_e = hist[best_idx]

    fig, axes = plt.subplots(1, 3, figsize=(15.0, 4.3), gridspec_kw={"width_ratios": [1.18, 0.90, 1.18], "wspace": 0.68})

    # Panel (a): Training trajectory
    ax = axes[0]
    ax2 = ax.twinx()
    l1 = ax.plot(ep, iou, color="#1d4ed8", lw=1.6, label="Smooth IoU (left)")
    l2 = ax.plot(ep, val_loss, color="#64748b", lw=1.2, ls="--", label="Val Loss (left)")
    l3 = ax2.plot(ep, dz, color="#b91c1c", lw=1.4, label=r"$|\Delta z|$ [m] (right)")
    ax.axvline(best_e["epoch"], color="#0f172a", ls=":", lw=1.0)
    ax.text(best_e["epoch"] - 10, 0.39, f"Best ep {best_e['epoch']}", ha="right", va="bottom", fontsize=7.0,
            bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="#cbd5e1", alpha=0.9))
    ax.set_xlabel("Training Epoch")
    ax.set_ylabel("Smooth IoU / Val Loss")
    ax2.set_ylabel(r"Depth Centroid Error $|\Delta z|$ [m]", color="#b91c1c")
    ax.set_ylim(0.35, 1.10)
    ax2.set_ylim(25, 76)
    ax.grid(alpha=0.25, lw=0.5)
    lines = l1 + l2 + l3
    ax.legend(lines, [ln.get_label() for ln in lines], loc="upper right", fontsize=7.0, frameon=True, framealpha=0.92)
    ax.set_title("(a) Swin3D v9.1 Multi-Regime Convergence")

    # Panel (b): Weighted loss breakdown at best epoch
    ax = axes[1]
    comp_keys = [
        ("val_wloss_sup", "Structural Sup."),
        ("val_wloss_cls", "Lithology CE/Dice"),
        ("val_wloss_nll", r"$\beta$-NLL Uncert."),
        ("val_wloss_coarse", "Stage-1 Coarse"),
        ("val_wloss_rho_init", r"Stage-2 $\rho_{\mathrm{init}}$"),
        ("val_wloss_data", "Gravity Misfit"),
        ("val_wloss_coherence", "Lith. Coherence"),
        ("val_wloss_edt", "Interior EDT"),
    ]
    vals = [float(best_e.get(k, 0.0)) for k, _ in comp_keys]
    tot_w = max(sum(vals), 1e-8)
    shares = [100.0 * v / tot_w for v in vals]
    lbls = [lbl for _, lbl in comp_keys]
    y_pos = np.arange(len(lbls))
    colors = ["#1d4ed8", "#0284c7", "#0d9488", "#7c3aed", "#9333ea", "#dc2626", "#d97706", "#64748b"]
    bars = ax.barh(y_pos, shares, color=colors, edgecolor="#1e293b", linewidth=0.5)
    ax.set_yticks(y_pos)
    ax.set_yticklabels(lbls)
    ax.invert_yaxis()
    ax.set_xlabel("Validation Loss Share [%]")
    ax.set_xlim(0, max(shares) * 1.25)
    ax.grid(axis="x", alpha=0.25, lw=0.5)
    for b, sh in zip(bars, shares):
        ax.text(sh + 1.0, b.get_y() + b.get_height() / 2, f"{sh:.1f}%", va="center", fontsize=7.0)
    ax.set_title("(b) Compound Loss Breakdown")

    # Panel (c): Controlled Architectural Ablation
    ax = axes[2]
    abl_a = res["ablation"]["panel_a_realistic"]
    order_a = [("v6", "v6"), ("v6.1", "v6.1"), ("v7", "v7\n(slab)"), ("v8", "v8\n(slab)"), ("v9.1_hard", "v9.1\n(unified)")]
    keys_a = [k for k, _ in order_a if k in abl_a]
    x_pos = np.arange(len(keys_a))
    ious_a = [abl_a[k]["metrics"]["iou"] for k in keys_a]
    dzs_a = [abl_a[k]["metrics"]["centroid_dz"] for k in keys_a]

    ax2_c = ax.twinx()
    bar_cols = ["#94a3b8", "#64748b", "#f59e0b", "#d97706", "#1d4ed8"][: len(keys_a)]
    ax.bar(x_pos, ious_a, width=0.55, color=bar_cols, edgecolor="#1e293b", lw=0.6, label="IoU (left)")
    ax2_c.plot(x_pos, dzs_a, "o-", color="#b91c1c", lw=1.6, ms=5.5, label=r"$|\Delta z|$ [m] (right)")
    ax.set_xticks(x_pos)
    ax.set_xticklabels([dict(order_a)[k] for k in keys_a])
    ymin_a = max(0.0, min(ious_a) - 0.09)
    ymax_a = min(1.0, max(ious_a) + 0.07)
    ax.set_ylim(ymin_a, ymax_a)
    ax2_c.set_ylim(max(0.0, min(dzs_a) - 10.0), max(dzs_a) + 12.0)
    ax.set_ylabel("Smooth 3D IoU")
    ax2_c.set_ylabel(r"$|\Delta z|$ [m]", color="#b91c1c")
    ax.grid(axis="y", alpha=0.25, lw=0.5)
    for idx, (bv, dv) in enumerate(zip(ious_a, dzs_a)):
        ax.text(idx, bv + 0.006, f"{bv:.3f}", ha="center", va="bottom", fontsize=6.8, color="#0f172a")
    ax.set_title("(c) Ablation: 3-Tier Slab vs. Unified v9.1")

    _save_fig(fig, "fig2_training_ablation")


def _plot_xz_section(ax, sec, terr_x, vmax, cmap="coolwarm", show_cbar=False, badge="", title="", is_sigma=False):
    nz, nx = sec.shape
    extent = [0.0, nx * 0.05, nz * 0.05, 0.0]
    terr_km = np.asarray(terr_x, dtype=np.float64) * 0.001
    if is_sigma:
        im = ax.imshow(sec, origin="upper", extent=extent, cmap=cmap, vmin=0.0, vmax=vmax, aspect="auto")
    else:
        im = ax.imshow(sec, origin="upper", extent=extent, cmap=cmap, vmin=-vmax, vmax=vmax, aspect="auto")
    x_km = (np.arange(nx) + 0.5) * 0.05
    ax.fill_between(x_km, 0.0, terr_km, color="#cbd5e1", zorder=3)
    ax.plot(x_km, terr_km, color="#0f172a", lw=1.0, zorder=4)
    ax.set_yticks([0.0, 0.5, 1.0, 1.5])
    if badge:
        ax.text(
            0.97, 0.05, badge, transform=ax.transAxes, ha="right", va="bottom",
            fontsize=6.5, color="#0f172a",
            bbox=dict(boxstyle="round,pad=0.2", facecolor="white", edgecolor="#94a3b8", alpha=0.88),
            zorder=5,
        )
    if title:
        ax.set_title(title, fontsize=8.5, pad=3)
    return im


def draw_fig3_canonical_comparison(res: dict) -> None:
    """4 canonical models x 6 columns (Obs gz, Truth, Tikhonov L2, Focusing IRLS, Swin3D v9.1, Hybrid)."""
    slices = np.load(_RESULTS_DIR / "canonical_slices.npz")
    can = res["canonical"]
    canon_order = ("block_ore", "dipping_dyke", "twin_opposing", "sedimentary_basin")
    col_methods = (
        ("truth", "Ground Truth"),
        ("tikhonov_l2", r"Tikhonov $L_2$"),
        ("focusing_irls", "Focusing IRLS"),
        ("swin3d_v9_1", "Swin3D v9.1"),
        ("hybrid_v9_1_focus", "Hybrid v9.1+Focus"),
    )

    fig, axes = plt.subplots(4, 6, figsize=(15.6, 9.6), gridspec_kw={"wspace": 0.24, "hspace": 0.24})

    for row, cname in enumerate(canon_order):
        cobj = can[cname]
        d_obs = slices[f"{cname}__d_obs"]
        terr_x = slices[f"{cname}__terr_x"]
        truth_xz = slices[f"{cname}__truth_xz"]
        iy_pk = cobj["iy_pk"]

        # Col 0: Observed gravity map + profile slice dashed line
        ax0 = axes[row, 0]
        gmax = max(float(np.abs(d_obs).max()), 0.1)
        im0 = ax0.imshow(d_obs.T, origin="lower", extent=[0, 3.2, 0, 3.2], cmap="RdBu_r", vmin=-gmax, vmax=gmax, aspect="equal")
        ax0.axhline((iy_pk + 0.5) * 0.05, color="#0f172a", ls="--", lw=1.0)
        ax0.set_ylabel(cobj["title"].replace(" ($45^\\circ$)", "\n($45^\\circ$)") + "\nY [km]", fontsize=8.0)
        if row == 0:
            ax0.set_title(r"Surface $\Delta g_z$ [mGal]", fontsize=8.5, pad=3)
        if row == 3:
            ax0.set_xlabel("X [km]")
        else:
            ax0.tick_params(labelbottom=False)
        ax0.text(0.04, 0.94, f"±{gmax:.1f} mGal", transform=ax0.transAxes, va="top", fontsize=6.5,
                 bbox=dict(boxstyle="round,pad=0.15", fc="white", alpha=0.85, lw=0.4))

        # Cols 1..5: X-Z density sections
        all_secs = [truth_xz] + [slices[f"{cname}__{mk}_xz"] for mk, _ in col_methods[1:]]
        vmax = max(0.35, float(np.abs(truth_xz).max()) * 1.05)

        for ci, (mk, clbl) in enumerate(col_methods, start=1):
            ax = axes[row, ci]
            sec = slices[f"{cname}__{mk}_xz"] if mk != "truth" else truth_xz
            badge = ""
            if mk != "truth":
                mm = cobj["methods"][mk]
                badge = f"IoU {mm['iou']:.2f}\n|dz| {mm['centroid_dz']:.0f}m"
            im = _plot_xz_section(ax, sec, terr_x, vmax=vmax, cmap="coolwarm", badge=badge, title=(clbl if row == 0 else ""))
            if ci == 1:
                ax.set_ylabel("Z [km]", fontsize=7.5, labelpad=1)
                ax.tick_params(labelsize=6.8)
            else:
                ax.tick_params(labelleft=False)
            if row == 3:
                ax.set_xlabel("X [km]")
            else:
                ax.tick_params(labelbottom=False)
            if ci == 5:
                cb = fig.colorbar(im, ax=axes[row, 1:].ravel().tolist(), fraction=0.015, pad=0.015)
                cb.set_label(r"$\Delta\rho$ [g/cm$^3$]", fontsize=7.5)
                cb.ax.tick_params(labelsize=6.8)

    _save_fig(fig, "fig3_canonical_comparison")


def draw_fig4_showcase_composite(res: dict) -> None:
    """6 diverse geological archetypes x 4 columns (Profile gz, Truth, v9.1 Pred, Aleatoric Sigma)."""
    slices = np.load(_RESULTS_DIR / "showcase_slices.npz")
    meta = res["showcase"]
    n_rows = min(6, len(meta))

    fig, axes = plt.subplots(n_rows, 4, figsize=(13.8, 12.6), gridspec_kw={"wspace": 0.26, "hspace": 0.28})
    x_km = (np.arange(64) + 0.5) * 0.05

    for r in range(n_rows):
        m = meta[r]
        obs_p = slices[f"s{r}__obs_x_prof"]
        pred_p = slices[f"s{r}__pred_x_prof"]
        truth_xz = slices[f"s{r}__truth_xz"]
        pred_xz = slices[f"s{r}__pred_xz"]
        sigma_xz = slices[f"s{r}__sigma_xz"]
        terr_x = slices[f"s{r}__terr_x"]

        # Col 0: Draped gravity profile obs vs pred
        ax0 = axes[r, 0]
        ax0.plot(x_km, obs_p, "o", ms=2.2, color="#dc2626", alpha=0.85, label="Observed")
        ax0.plot(x_km, pred_p, "-", lw=1.4, color="#1d4ed8", label="Predicted")
        ax0.set_ylabel(f"{m['label']}\n$\\Delta g_z$ [mGal]", fontsize=8.0)
        ax0.grid(alpha=0.25, lw=0.5)
        if r == 0:
            ax0.set_title("Draped Profile Fit", fontsize=8.5, pad=3)
            ax0.legend(loc="best", fontsize=6.8, frameon=True)
        if r == n_rows - 1:
            ax0.set_xlabel("X [km]")
        else:
            ax0.tick_params(labelbottom=False)

        vmax = max(0.35, float(max(np.abs(truth_xz).max(), np.abs(pred_xz).max())))
        _plot_xz_section(axes[r, 1], truth_xz, terr_x, vmax=vmax, cmap="coolwarm", title=("Ground Truth (X–Z)" if r == 0 else ""))
        badge_p = f"IoU {m['iou']:.2f} | |dz| {m['centroid_dz']:.0f}m"
        im_p = _plot_xz_section(axes[r, 2], pred_xz, terr_x, vmax=vmax, cmap="coolwarm", badge=badge_p, title=("Swin3D v9.1 (X–Z)" if r == 0 else ""))

        smax = max(0.15, float(np.percentile(sigma_xz, 99.5)))
        im_s = _plot_xz_section(axes[r, 3], sigma_xz, terr_x, vmax=smax, cmap="viridis", is_sigma=True, title=(r"Aleatoric $\hat{\sigma}_\rho$ [g/cm$^3$]" if r == 0 else ""))

        for c in (1, 2, 3):
            if c == 1:
                axes[r, c].set_ylabel("Z [km]", fontsize=7.5, labelpad=1)
                axes[r, c].tick_params(labelsize=6.8)
            else:
                axes[r, c].tick_params(labelleft=False)
            if r == n_rows - 1:
                axes[r, c].set_xlabel("X [km]")
            else:
                axes[r, c].tick_params(labelbottom=False)
        cb_p = fig.colorbar(im_p, ax=[axes[r, 1], axes[r, 2]], fraction=0.025, pad=0.02)
        cb_p.ax.tick_params(labelsize=6.8)
        cb_s = fig.colorbar(im_s, ax=axes[r, 3], fraction=0.045, pad=0.03)
        cb_s.ax.tick_params(labelsize=6.8)

    _save_fig(fig, "fig4_showcase_composite")


def draw_fig5_sensitivity_depth(res: dict) -> None:
    """3-panel Noise, Depth-Quartile, and Clearance sensitivity plot."""
    ns = res["noise_sweep"]
    dq = res["depth_quartiles"]
    clr = res["clearance_sweep"]

    fig, axes = plt.subplots(1, 3, figsize=(14.8, 4.2), gridspec_kw={"wspace": 0.54})

    # (a) Noise sensitivity
    ax = axes[0]
    w_lvls = [0, 1, 2, 5, 10]
    c_lvls = [1, 2, 5, 10]
    w_iou = [ns["white"][f"{k}pct"]["iou"] for k in w_lvls]
    c_iou = [ns["colored"][f"{k}pct"]["iou"] for k in c_lvls]
    w_dz = [ns["white"][f"{k}pct"]["centroid_dz"] for k in w_lvls]
    c_dz = [ns["colored"][f"{k}pct"]["centroid_dz"] for k in c_lvls]

    ax2 = ax.twinx()
    l1 = ax.plot(w_lvls, w_iou, "o-", color="#1d4ed8", lw=1.6, label="White IoU (L)")
    l2 = ax.plot(c_lvls, c_iou, "s--", color="#0284c7", lw=1.5, label="Colored IoU (L)")
    l3 = ax2.plot(w_lvls, w_dz, "o:", color="#b91c1c", lw=1.3, label=r"White $|\Delta z|$ (R)")
    l4 = ax2.plot(c_lvls, c_dz, "s:", color="#ea580c", lw=1.3, label=r"Colored $|\Delta z|$ (R)")
    ax.set_xlabel(r"Noise Level $\sigma / \max|d|$ [%]")
    ax.set_ylabel("Smooth 3D IoU")
    ax2.set_ylabel(r"$|\Delta z|$ [m]", color="#b91c1c")
    ax.set_ylim(0.0, 0.72)
    ax2.set_ylim(0.0, 290.0)
    ax.grid(alpha=0.25, lw=0.5)
    lines = l1 + l2 + l3 + l4
    ax.legend(lines, [ln.get_label() for ln in lines], loc="upper center", ncol=2, fontsize=6.4, frameon=True, framealpha=0.92)
    ax.set_title("(a) Noise Level & Correlation Sweep")

    # (b) Depth quartiles
    ax = axes[1]
    q_x = np.arange(len(dq))
    q_iou = [q["metrics"]["iou"] for q in dq]
    q_amp = [q["metrics"]["amp_ratio"] for q in dq]
    q_dz = [q["metrics"]["centroid_dz"] for q in dq]
    ax2 = ax.twinx()
    ax.bar(q_x - 0.18, q_iou, width=0.35, color="#1d4ed8", label="IoU (L)")
    ax.bar(q_x + 0.18, q_amp, width=0.35, color="#0d9488", label="Amp. Ratio (L)")
    ax2.plot(q_x, q_dz, "D-", color="#b91c1c", lw=1.5, ms=5, label=r"$|\Delta z|$ (R)")
    ax.set_xticks(q_x)
    ax.set_xticklabels([f"{q['quartile']}\n({q['depth_lo_m']:.0f}–{q['depth_hi_m']:.0f}m)" for q in dq], fontsize=6.8)
    ax.set_ylabel("IoU / Amplitude Ratio")
    ax2.set_ylabel(r"$|\Delta z|$ [m]", color="#b91c1c")
    ax.set_ylim(0.0, 1.08)
    ax2.set_ylim(0, max(q_dz) * 1.45)
    ax.grid(axis="y", alpha=0.25, lw=0.5)
    h1, lbl1 = ax.get_legend_handles_labels()
    h2, lbl2 = ax2.get_legend_handles_labels()
    ax.legend(h1 + h2, lbl1 + lbl2, loc="upper right", fontsize=6.6, frameon=True, framealpha=0.92)
    ax.set_title("(b) Depth-Quartile Resolution")

    # (c) Clearance sweep
    ax = axes[2]
    h_keys = ("0.35m", "5m", "20m", "60m", "120m", "160m")
    h_vals = [0.35, 5.0, 20.0, 60.0, 120.0, 160.0]
    h_iou = [clr[k]["iou"] for k in h_keys]
    h_dz = [clr[k]["centroid_dz"] for k in h_keys]
    ax2 = ax.twinx()
    l1 = ax.plot(range(len(h_vals)), h_iou, "o-", color="#1d4ed8", lw=1.6, label="IoU (L)")
    l2 = ax2.plot(range(len(h_vals)), h_dz, "s-", color="#b91c1c", lw=1.5, label=r"$|\Delta z|$ (R)")
    ax.set_xticks(range(len(h_vals)))
    ax.set_xticklabels(["0.35", "5", "20", "60", "120", "160"])
    ax.set_xlabel("Sensor Clearance $h$ [m]")
    ax.set_ylabel("Smooth 3D IoU")
    ax2.set_ylabel(r"$|\Delta z|$ [m]", color="#b91c1c")
    ax.set_ylim(max(0.0, min(h_iou) - 0.08), min(1.0, max(h_iou) + 0.08))
    ax2.set_ylim(max(0.0, min(h_dz) - 8.0), max(h_dz) + 12.0)
    ax.grid(alpha=0.25, lw=0.5)
    lines = l1 + l2
    ax.legend(lines, [ln.get_label() for ln in lines], loc="lower left", fontsize=7.0, frameon=True)
    ax.set_title("(c) Survey Clearance Sweep")

    _save_fig(fig, "fig5_sensitivity_depth")


def draw_fig6_uncertainty_calib(res: dict) -> None:
    """3-panel Risk-Coverage, Reliability, and Depth Centroid Scatter."""
    cal = np.load(_RESULTS_DIR / "calib_arrays.npz")
    err = cal["err"].astype(np.float64)
    sig = cal["sigma"].astype(np.float64)
    is_fg = cal["is_fg"].astype(bool)
    true_zc = cal["true_zc"].astype(np.float64)
    pred_zc = cal["pred_zc"].astype(np.float64)
    b_ids = cal["bucket_id"]
    comp = res["v91_composite"]

    fig, axes = plt.subplots(1, 3, figsize=(13.8, 4.1), gridspec_kw={"wspace": 0.28})

    # (a) Selective prediction Risk-Coverage Curve (subsampled smoothly to 200 points)
    ax = axes[0]
    n_tot = err.size
    ord_sig = np.argsort(sig)
    ord_oracle = np.argsort(err)
    cum_sig = np.cumsum(err[ord_sig]) / np.arange(1, n_tot + 1)
    cum_ora = np.cumsum(err[ord_oracle]) / np.arange(1, n_tot + 1)
    rand_mean = float(err.mean())
    idx_grid = np.linspace(max(10, n_tot // 100), n_tot - 1, 150, dtype=int)
    cov = (idx_grid + 1) / float(n_tot) * 100.0
    ax.plot(cov, cum_sig[idx_grid], color="#1d4ed8", lw=1.7, label=rf"Ordered by $\hat{{\sigma}}_\rho$ (AURC={comp.get('aurc',0):.4f})")
    ax.plot(cov, cum_ora[idx_grid], color="#0d9488", lw=1.4, ls="--", label="Oracle lower bound")
    ax.axhline(rand_mean, color="#64748b", lw=1.1, ls=":", label="Random (unranked)")
    ax.set_xlabel("Coverage [%]")
    ax.set_ylabel(r"Cumulative Mean $|\hat{\rho}-\rho|$ [g/cm$^3$]")
    ax.grid(alpha=0.25, lw=0.5)
    ax.legend(loc="upper left", fontsize=6.8, frameon=True)
    ax.set_title("(a) Risk–Coverage Curve")

    # (b) Binned empirical error vs predicted sigma on foreground + boundary active zone
    ax = axes[1]
    fg_err = err[is_fg]
    fg_sig = sig[is_fg]
    n_bins = 10
    qs = np.quantile(fg_sig, np.linspace(0.0, 1.0, n_bins + 1))
    bx, by, bstd = [], [], []
    for k in range(n_bins):
        m = (fg_sig >= qs[k]) & (fg_sig <= qs[k + 1] if k == n_bins - 1 else fg_sig < qs[k + 1])
        if m.any():
            bx.append(float(fg_sig[m].mean()))
            # Gaussian 1-sigma corresponds to sqrt(pi/2)*MAE or RMSE
            by.append(float(np.sqrt(np.mean(fg_err[m] ** 2))))
            bstd.append(float(fg_err[m].mean()))
    lim = max(max(bx), max(by)) * 1.1
    ax.plot([0, lim], [0, lim], "k--", lw=1.0, label="Ideal RMSE = $\\hat{\\sigma}_\\rho$")
    ax.plot(bx, by, "o-", color="#1d4ed8", lw=1.6, ms=4.5, label="Empirical RMSE")
    ax.plot(bx, bstd, "s:", color="#d97706", lw=1.3, ms=4.0, label="Empirical MAE")
    ax.text(
        0.96, 0.06,
        f"1-$\\sigma$ cov: {comp.get('fg_1sig_cov',0)*100:.1f}%\nSpearman: {comp.get('unc_spearman',0):.3f}",
        transform=ax.transAxes, ha="right", va="bottom", fontsize=7.0,
        bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#94a3b8"),
    )
    ax.set_xlabel(r"Predicted $\hat{\sigma}_\rho$ [g/cm$^3$]")
    ax.set_ylabel(r"Empirical Body Error [g/cm$^3$]")
    ax.set_xlim(0, lim)
    ax.set_ylim(0, lim)
    ax.grid(alpha=0.25, lw=0.5)
    ax.legend(loc="upper left", fontsize=6.8, frameon=True)
    ax.set_title("(b) Foreground Uncertainty Calibration")

    # (c) Depth Centroid Scatter across 4 buckets
    ax = axes[2]
    b_names = ("Ground Clean", "Hard Detrend", "Airborne", "Multi-Aspect")
    b_cols = ("#1d4ed8", "#0d9488", "#d97706", "#7c3aed")
    valid = np.isfinite(true_zc) & np.isfinite(pred_zc)
    zmax = max(float(true_zc[valid].max()), float(pred_zc[valid].max())) + 80.0 if valid.any() else 1200.0
    ax.fill_between([0, zmax], [0 - 50, zmax - 50], [50, zmax + 50], color="#e2e8f0", alpha=0.65, label=r"$\pm 1$ cell ($\pm 50\,$m)")
    ax.plot([0, zmax], [0, zmax], "k--", lw=1.0)
    for bi in range(4):
        m = valid & (b_ids == bi)
        if m.any():
            ax.scatter(true_zc[m], pred_zc[m], s=20, alpha=0.82, color=b_cols[bi], label=b_names[bi], edgecolors="white", linewidths=0.3)
    ax.set_xlabel("True Centroid Depth $z_{\\mathrm{true}}$ [m]")
    ax.set_ylabel("Predicted Centroid Depth $z_{\\mathrm{pred}}$ [m]")
    ax.set_xlim(0, zmax)
    ax.set_ylim(0, zmax)
    ax.grid(alpha=0.25, lw=0.5)
    ax.legend(loc="upper left", fontsize=6.5, frameon=True, ncol=1)
    ax.set_title("(c) Vertical Centroid Recovery")

    _save_fig(fig, "fig6_uncertainty_calib")


def main() -> None:
    eval_json = _RESULTS_DIR / "eval_results.json"
    if not eval_json.is_file():
        raise FileNotFoundError(f"Missing {eval_json}; run eval_paper_v9_1.py first.")
    with open(eval_json, encoding="utf-8") as f:
        res = json.load(f)

    write_macros(res)
    write_tables(res)
    draw_fig1_architecture(res)
    draw_fig2_training_ablation(res)
    draw_fig3_canonical_comparison(res)
    draw_fig4_showcase_composite(res)
    draw_fig5_sensitivity_depth(res)
    draw_fig6_uncertainty_calib(res)
    print(f"Generated macros, 6 tables, and 6 vector PDF figures in {_PAPER_DIR}")


if __name__ == "__main__":
    main()
