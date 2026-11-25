"""Generate auto-updated LaTeX macros, booktabs tables, and vector PDF figures
from ``.inv/paper/results/eval_results.json`` and archived training history in
``.inv/paper/models/training_history_v9.1_hard.json``.

Outputs (flat inside ``.inv/paper/``):
  - ``macros_results.tex``
  - ``table_v91_buckets.tex``
  - ``table_ablation.tex``
  - ``table_canonical.tex``
  - ``table_ood.tex``
  - ``table_noise.tex``
  - ``table_depth_clearance.tex``
  - ``table_timing.tex``
  - ``figures/fig1_architecture.pdf`` .. ``figures/fig7_uncertainty_calib.pdf``
  - Preview PNGs in ``results/preview_png/`` for visual inspection.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import numpy as np

_THIS_DIR = Path(__file__).resolve().parent
_PAPER_DIR = _THIS_DIR.parent
_MODELS_DIR = _PAPER_DIR / "models"
_RESULTS_DIR = _PAPER_DIR / "results"
_FIG_DIR = _PAPER_DIR / "figures"
_PREVIEW_DIR = _RESULTS_DIR / "preview_png"

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


def _f_pm(mean_val, std_val, nd: int = 3, scale: float = 1.0) -> str:
    ms = _f(mean_val, nd=nd, scale=scale)
    if ms == "---":
        return "---"
    ss = _f(std_val, nd=nd, scale=scale)
    if ss == "---":
        return ms
    return rf"{ms}\,$\pm$\,{ss}"


def _tex_escape(s: str) -> str:
    return (
        str(s)
        .replace("\\", r"\textbackslash{}")
        .replace("&", r"\&")
        .replace("%", r"\%")
        .replace("_", r"\_")
        .replace("#", r"\#")
    )


def _bold_best_rounded(
    vals: list[float],
    higher_is_better: bool = True,
    nd: int = 3,
    scale: float = 1.0,
    target: float | None = None,
) -> list[str]:
    """Format values to ``nd`` decimals and bold EVERY entry tied for best AFTER rounding."""
    formatted = [_f(v, nd=nd, scale=scale) for v in vals]
    rounded = [
        float(s) if s != "---" else float("nan")
        for s in formatted
    ]
    valid = [r for r in rounded if not math.isnan(r)]
    if not valid:
        return formatted
    if target is not None:
        best_dist = min(abs(r - target) for r in valid)
        return [
            (rf"\textbf{{{s}}}" if (not math.isnan(r) and abs(abs(r - target) - best_dist) <= 1e-12) else s)
            for s, r in zip(formatted, rounded)
        ]
    best = max(valid) if higher_is_better else min(valid)
    return [
        (rf"\textbf{{{s}}}" if (not math.isnan(r) and abs(r - best) <= 1e-12) else s)
        for s, r in zip(formatted, rounded)
    ]


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
    pd = res["physics_diagnostics"]
    comp = res["v91_composite"]
    bk = res["v91_buckets"]
    abl_a = res["ablation"]["panel_a_realistic"]
    abl_b = res["ablation"]["panel_b_clean_ground"]
    ep97 = res["ablation"].get("v91_hard_matched_ep97", {})
    ns = res["noise_sweep"]
    dq = res["depth_quartiles"]
    clr = res["clearance_sweep"]
    can = res["canonical"]
    ood = res["ood_noninversecrime"]
    sh = res["showcase"]
    tm = res["timing"]

    def _mean_bench(bdict: dict, mkey: str, metric: str) -> float:
        vals = [float(bdict[c]["methods"][mkey][metric]) for c in bdict if metric in bdict[c]["methods"][mkey]]
        return float(np.mean(vals)) if vals else float("nan")

    foc_s = tm.get("focusing_irls_s", float("nan"))
    pt_ms = tm.get("pytorch_cpu_ms", float("nan"))
    onnx_ms = tm.get("onnx_cpu_ms", float("nan"))
    cm_ms = tm.get("onnx_coreml_ne_ms", float("nan"))
    speed_onnx = (foc_s * 1000.0 / onnx_ms) if (not math.isnan(foc_s) and not math.isnan(onnx_ms) and onnx_ms > 0) else float("nan")
    speed_cm = (foc_s * 1000.0 / cm_ms) if (not math.isnan(foc_s) and not math.isnan(cm_ms) and cm_ms > 0) else float("nan")

    v9_m = abl_b.get("v9_v3data", {}).get("metrics", {})
    v91_m = abl_b.get("v9.1_v3data", {}).get("metrics", {})

    # Format adjoint error in scientific notation for LaTeX (e.g., 1.08 \times 10^{-7})
    adj_e = pd["adjoint_dot_rel_err"]
    adj_exp = int(math.floor(math.log10(max(adj_e, 1e-30))))
    adj_man = adj_e / (10.0 ** adj_exp)
    adj_latex = rf"{adj_man:.2f} \times 10^{{{adj_exp}}}"

    # Per-canonical & OOD helpers
    bo = can["block_ore"]["methods"]
    dd = can["dipping_dyke"]["methods"]
    to = can["twin_opposing"]["methods"]
    sb = can["sedimentary_basin"]["methods"]

    os_ = ood["ood_sphere"]["methods"]
    og_ = ood["ood_graded_dome"]["methods"]
    ob_ = ood["ood_bipolar_curved"]["methods"]

    macros = {
        # Metadata & diagnostics
        "valVNineOneEp": str(meta["checkpoint_hard_epoch"]),
        "valVNineOneCleanEp": str(abl_b.get("v9.1_v3data", {}).get("epoch", -1)),
        "valVNineOneParams": f"{meta['checkpoint_hard_params'] / 1e6:.2f}M",
        "valVNineOneParamsExact": _fi(meta["checkpoint_hard_params"]),
        "valVNineOneSlimParams": "1.36M",
        "valVNineOneHeavyParams": "6.90M",
        "valNBucketTotal": str(4 * meta["n_per_bucket"]),
        "valNAblBucket": str(4 * meta["ablation_n_per_bucket"]),
        "valNCleanStd": str(meta["clean_n"]),
        "valNCleanHard": str(meta["clean_hard_n"]),
        "valNNoise": str(meta["noise_n"]),
        "valNIrlsNoise": str(meta["n_irls_noise"]),
        "valTestSeed": str(meta["test_seed"]),
        "valTrainValSeed": str(meta["train_val_seed"]),
        "valCpuThreads": str(meta["cpu_threads"]),
        "valAdjointRelErr": adj_latex,
        "valOddPyrRelErr": _f(pd["odd_pyramid_rel_err_mean"], 2, 100.0),
        "valOddPyrRelErrStd": _f(pd["odd_pyramid_rel_err_std"], 2, 100.0),

        # Multi-regime 4-bucket held-out test
        "valCompIoU": _f(comp.get("iou"), 3),
        "valCompIoUStd": _f(comp.get("iou_std"), 3),
        "valCompDz": _f(comp.get("centroid_dz"), 1),
        "valCompDzStd": _f(comp.get("centroid_dz_std"), 1),
        "valCompRMSE": _f(comp.get("rmse"), 3),
        "valCompFGRMSE": _f(comp.get("fg_rmse"), 3),
        "valCompSSIM": _f(comp.get("ssim_3d"), 3),
        "valCompSSIMd": _f(comp.get("ssim_d3"), 3),
        "valCompAmp": _f(comp.get("amp_ratio"), 3),
        "valCompMisfit": _f(comp.get("data_misfit_eff"), 2, 100.0),
        "valCompHalo": _f(comp.get("halo_opp"), 2, 1e4),
        "valCompUncSp": _f(comp.get("unc_spearman"), 3),
        "valCompFgUncSp": _f(comp.get("fg_unc_spearman"), 3),
        "valCompFgCov": _f(comp.get("fg_1sig_cov"), 1, 100.0),
        "valCompAURCPooled": _f(comp.get("aurc_pooled"), 4),
        "valCompBdRatio": _f(comp.get("bd_sig_err_ratio"), 2),
        "valCompCentroidBias": _f(comp.get("centroid_bias_m"), 1),
        "valCompCentroidWithinFifty": _f(comp.get("centroid_within_50m_pct"), 1),

        "valHLIoU": _f(bk["ground_hard_large_detrend"].get("iou"), 3),
        "valHLDz": _f(bk["ground_hard_large_detrend"].get("centroid_dz"), 1),
        "valAirIoU": _f(bk["airborne_relief_drape"].get("iou"), 3),
        "valAirDz": _f(bk["airborne_relief_drape"].get("centroid_dz"), 1),
        "valMAIoU": _f(bk["multi_aspect"].get("iou"), 3),
        "valMADz": _f(bk["multi_aspect"].get("centroid_dz"), 1),

        # Progression Panel A & B + matched ep-97 snapshot
        "valAblVSixIoU": _f(abl_a["v6"]["metrics"]["iou"], 3),
        "valAblVSixMisfit": _f(abl_a["v6"]["metrics"]["data_misfit_eff"], 2, 100.0),
        "valAblVSixOneIoU": _f(abl_a["v6.1"]["metrics"]["iou"], 3),
        "valAblVSixOneMisfit": _f(abl_a["v6.1"]["metrics"]["data_misfit_eff"], 2, 100.0),
        "valAblVSevenIoU": _f(abl_a["v7"]["metrics"]["iou"], 3),
        "valAblVSevenDz": _f(abl_a["v7"]["metrics"]["centroid_dz"], 1),
        "valAblVEightIoU": _f(abl_a["v8"]["metrics"]["iou"], 3),
        "valAblVEightDz": _f(abl_a["v8"]["metrics"]["centroid_dz"], 1),
        "valAblVEightMisfit": _f(abl_a["v8"]["metrics"]["data_misfit_eff"], 2, 100.0),
        "valAblVNineOneIoU": _f(abl_a["v9.1_hard"]["metrics"]["iou"], 3),
        "valAblVNineOneDz": _f(abl_a["v9.1_hard"]["metrics"]["centroid_dz"], 1),
        "valAblVNineOneSSIMd": _f(abl_a["v9.1_hard"]["metrics"]["ssim_d3"], 3),
        "valAblVNineOneMisfit": _f(abl_a["v9.1_hard"]["metrics"]["data_misfit_eff"], 2, 100.0),
        "valMatchedEp": str(ep97.get("epoch", 97)),
        "valMatchedIoU": _f(ep97.get("iou"), 3),
        "valMatchedDz": _f(ep97.get("centroid_dz"), 1),

        "valCleanVThreeIoU": _f(abl_b["v3"]["metrics"]["iou"], 3),
        "valCleanVFiveTwoIoU": _f(abl_b["v5.2"]["metrics"]["iou"], 3),
        "valCleanVEightIoU": _f(abl_b["v8_v3data"]["metrics"]["iou"], 3),
        "valCleanVEightDz": _f(abl_b["v8_v3data"]["metrics"]["centroid_dz"], 1),
        "valCleanVEightEp": str(abl_b["v8_v3data"]["epoch"]),
        "valCleanVNineIoU": _f(v9_m.get("iou"), 3),
        "valCleanVNineOneIoU": _f(v91_m.get("iou"), 3),
        "valCleanVNineOneDz": _f(v91_m.get("centroid_dz"), 1),
        "valCleanVNineOneMisfit": _f(v91_m.get("data_rel_misfit"), 2, 100.0),
        "valCleanHardXferIoU": _f(abl_b["v9.1_hard_xfer"]["metrics"]["iou"], 3),
        "valCleanHardXferDz": _f(abl_b["v9.1_hard_xfer"]["metrics"]["centroid_dz"], 1),
        "valCleanHardXferCov": _f(abl_b["v9.1_hard_xfer"]["metrics"].get("fg_1sig_cov"), 1, 100.0),

        # Canonical benchmarks (per-case & means)
        "valCanonTikOreIoU": _f(bo["tikhonov_l2"]["iou"], 3),
        "valCanonTikOreDz": _f(bo["tikhonov_l2"]["centroid_dz"], 1),
        "valCanonFocOreIoU": _f(bo["focusing_irls"]["iou"], 3),
        "valCanonFocOreDz": _f(bo["focusing_irls"]["centroid_dz"], 1),
        "valCanonSwinOreIoU": _f(bo["swin3d_v9_1"]["iou"], 3),
        "valCanonSwinOreDz": _f(bo["swin3d_v9_1"]["centroid_dz"], 1),
        "valCanonSwinOreSSIM": _f(bo["swin3d_v9_1"]["ssim_d3"], 3),
        "valCanonSwinOreMisfit": _f(bo["swin3d_v9_1"]["data_misfit_eff"], 2, 100.0),
        "valCanonSwinOreChi": _f(bo["swin3d_v9_1"]["chi2_red"], 2),
        "valCanonSwinOreCov": _f(bo["swin3d_v9_1"].get("fg_1sig_cov"), 1, 100.0),

        "valCanonTikDykeDz": _f(dd["tikhonov_l2"]["centroid_dz"], 1),
        "valCanonFocDykeIoU": _f(dd["focusing_irls"]["iou"], 3),
        "valCanonFocDykeDz": _f(dd["focusing_irls"]["centroid_dz"], 1),
        "valCanonSwinDykeIoU": _f(dd["swin3d_v9_1"]["iou"], 3),
        "valCanonSwinDykeDz": _f(dd["swin3d_v9_1"]["centroid_dz"], 1),
        "valCanonSwinDykeMisfit": _f(dd["swin3d_v9_1"]["data_misfit_eff"], 2, 100.0),
        "valCanonSwinDykeCov": _f(dd["swin3d_v9_1"].get("fg_1sig_cov"), 1, 100.0),

        "valCanonFocTwinIoU": _f(to["focusing_irls"]["iou"], 3),
        "valCanonFocTwinMisfit": _f(to["focusing_irls"]["data_misfit_eff"], 2, 100.0),
        "valCanonSwinTwinIoU": _f(to["swin3d_v9_1"]["iou"], 3),
        "valCanonSwinTwinDz": _f(to["swin3d_v9_1"]["centroid_dz"], 1),
        "valCanonSwinTwinSSIM": _f(to["swin3d_v9_1"]["ssim_d3"], 3),

        "valCanonFocBasinIoU": _f(sb["focusing_irls"]["iou"], 3),
        "valCanonFocBasinDz": _f(sb["focusing_irls"]["centroid_dz"], 1),
        "valCanonFocBasinTime": _f(sb["focusing_irls"]["wall_time_s"], 2),
        "valCanonSwinBasinIoU": _f(sb["swin3d_v9_1"]["iou"], 3),
        "valCanonSwinBasinAmp": _f(sb["swin3d_v9_1"]["amp_ratio"], 3),
        "valCanonFocBasinAmp": _f(sb["focusing_irls"]["amp_ratio"], 3),
        "valCanonSwinBasinTime": _f(sb["swin3d_v9_1"]["wall_time_s"], 2),

        "valCanonMeanTikIoU": _f(_mean_bench(can, "tikhonov_l2", "iou"), 3),
        "valCanonMeanFocIoU": _f(_mean_bench(can, "focusing_irls", "iou"), 3),
        "valCanonMeanSwinIoU": _f(_mean_bench(can, "swin3d_v9_1", "iou"), 3),
        "valCanonMeanTikDz": _f(_mean_bench(can, "tikhonov_l2", "centroid_dz"), 1),
        "valCanonMeanFocDz": _f(_mean_bench(can, "focusing_irls", "centroid_dz"), 1),
        "valCanonMeanSwinDz": _f(_mean_bench(can, "swin3d_v9_1", "centroid_dz"), 1),
        "valCanonMeanTikSSIM": _f(_mean_bench(can, "tikhonov_l2", "ssim_d3"), 3),
        "valCanonMeanFocSSIM": _f(_mean_bench(can, "focusing_irls", "ssim_d3"), 3),
        "valCanonMeanSwinSSIM": _f(_mean_bench(can, "swin3d_v9_1", "ssim_d3"), 3),

        # Independent Non-Inverse-Crime OOD Benchmarks
        "valOodSwinSphIoU": _f(os_["swin3d_v9_1"]["iou"], 3),
        "valOodSwinSphDz": _f(os_["swin3d_v9_1"]["centroid_dz"], 1),
        "valOodFocSphIoU": _f(os_["focusing_irls"]["iou"], 3),
        "valOodSwinDomeIoU": _f(og_["swin3d_v9_1"]["iou"], 3),
        "valOodSwinDomeDz": _f(og_["swin3d_v9_1"]["centroid_dz"], 1),
        "valOodFocDomeIoU": _f(og_["focusing_irls"]["iou"], 3),
        "valOodSwinBipIoU": _f(ob_["swin3d_v9_1"]["iou"], 3),
        "valOodSwinBipDz": _f(ob_["swin3d_v9_1"]["centroid_dz"], 1),
        "valOodFocBipIoU": _f(ob_["focusing_irls"]["iou"], 3),
        "valOodMeanTikIoU": _f(_mean_bench(ood, "tikhonov_l2", "iou"), 3),
        "valOodMeanFocIoU": _f(_mean_bench(ood, "focusing_irls", "iou"), 3),
        "valOodMeanSwinIoU": _f(_mean_bench(ood, "swin3d_v9_1", "iou"), 3),
        "valOodMeanTikDz": _f(_mean_bench(ood, "tikhonov_l2", "centroid_dz"), 1),
        "valOodMeanFocDz": _f(_mean_bench(ood, "focusing_irls", "centroid_dz"), 1),
        "valOodMeanSwinDz": _f(_mean_bench(ood, "swin3d_v9_1", "centroid_dz"), 1),

        # Showcase 6 archetypes
        "valShowZeroIoU": _f(sh[0]["iou"], 2), "valShowZeroDz": _f(sh[0]["centroid_dz"], 0),
        "valShowOneIoU": _f(sh[1]["iou"], 2), "valShowOneDz": _f(sh[1]["centroid_dz"], 0),
        "valShowTwoIoU": _f(sh[2]["iou"], 2), "valShowTwoDz": _f(sh[2]["centroid_dz"], 0),
        "valShowThreeIoU": _f(sh[3]["iou"], 2), "valShowThreeDz": _f(sh[3]["centroid_dz"], 0),
        "valShowFourIoU": _f(sh[4]["iou"], 2), "valShowFourDz": _f(sh[4]["centroid_dz"], 0),
        "valShowFiveIoU": _f(sh[5]["iou"], 2), "valShowFiveDz": _f(sh[5]["centroid_dz"], 0),

        # Noise sweep (Swin3D + paired Focusing IRLS)
        "valNoiseZeroIoU": _f(ns["white"]["0pct"]["iou"], 3),
        "valNoiseFiveWhiteIoU": _f(ns["white"]["5pct"]["iou"], 3),
        "valNoiseTenWhiteIoU": _f(ns["white"]["10pct"]["iou"], 3),
        "valNoiseTenWhiteDz": _f(ns["white"]["10pct"]["centroid_dz"], 1),
        "valNoiseOneColoredIoU": _f(ns["colored"]["1pct"]["iou"], 3),
        "valNoiseTwoColoredIoU": _f(ns["colored"]["2pct"]["iou"], 3),
        "valNoiseOneColoredDz": _f(ns["colored"]["1pct"]["centroid_dz"], 1),
        "valNoiseTwoColoredDz": _f(ns["colored"]["2pct"]["centroid_dz"], 1),
        "valNoiseFiveColoredIoU": _f(ns["colored"]["5pct"]["iou"], 3),
        "valNoiseFiveColoredDz": _f(ns["colored"]["5pct"]["centroid_dz"], 1),
        "valNoiseTenColoredIoU": _f(ns["colored"]["10pct"]["iou"], 3),
        "valNoiseTenColoredDz": _f(ns["colored"]["10pct"]["centroid_dz"], 1),
        "valIrlsTenColoredIoU": _f(ns.get("irls_colored", {}).get("10pct", {}).get("iou"), 3),
        "valIrlsTenColoredDz": _f(ns.get("irls_colored", {}).get("10pct", {}).get("centroid_dz"), 1),

        # Depth quartiles & clearance sweep
        "valQOneIoU": _f(dq[0]["metrics"]["iou"], 3),
        "valQOneDz": _f(dq[0]["metrics"]["centroid_dz"], 1),
        "valQOneAmp": _f(dq[0]["metrics"]["amp_ratio"], 3),
        "valQOneRange": f"{dq[0]['depth_lo_m']:.0f}--{dq[0]['depth_hi_m']:.0f}",
        "valQFourIoU": _f(dq[3]["metrics"]["iou"], 3),
        "valQFourDz": _f(dq[3]["metrics"]["centroid_dz"], 1),
        "valQFourAmp": _f(dq[3]["metrics"]["amp_ratio"], 3),
        "valQFourRange": f"{dq[3]['depth_lo_m']:.0f}--{dq[3]['depth_hi_m']:.0f}",
        "valQMinSSIMd": _f(min(q["metrics"]["ssim_d3"] for q in dq), 3),
        "valQMaxSSIMd": _f(max(q["metrics"]["ssim_d3"] for q in dq), 3),

        "valClrGroundIoU": _f(clr["0.35m"]["iou"], 3),
        "valClrTwentyIoU": _f(clr["20m"]["iou"], 3),
        "valClrHighIoU": _f(clr["160m"]["iou"], 3),
        "valClrHighDz": _f(clr["160m"]["centroid_dz"], 1),

        # Timing & ONNX parity
        "valTimePtMs": _f(pt_ms, 1),
        "valTimeOnnxCpuMs": _f(onnx_ms, 1),
        "valTimeCoremlMs": _f(cm_ms, 1),
        "valTimeOnnxCpuDiff": f"{tm['onnx_cpu_max_rel_diff']:.2e}" if not math.isnan(tm.get("onnx_cpu_max_rel_diff", float("nan"))) else "---",
        "valTimeCoremlDiff": f"{tm['onnx_coreml_ne_max_rel_diff']:.2e}" if not math.isnan(tm.get("onnx_coreml_ne_max_rel_diff", float("nan"))) else "---",
        "valOnnxNodes": _fi(tm.get("onnx_num_nodes")),
        "valTimeFocS": _f(foc_s, 2),
        "valSpeedupOnnxVsFoc": f"{speed_onnx:.0f}$\\times$" if not math.isnan(speed_onnx) else "---",
        "valSpeedupCoremlVsFoc": f"{speed_cm:.0f}$\\times$" if not math.isnan(speed_cm) else "---",
    }

    lines = ["% Auto-generated by scripts/generate_tables_and_figures.py -- do not edit manually.\n"]
    for k, v in macros.items():
        lines.append(rf"\newcommand{{\{k}}}{{{v}}}" + "\n")
    (_PAPER_DIR / "macros_results.tex").write_text("".join(lines), encoding="utf-8")


def write_tables(res: dict) -> None:
    # 1. table_v91_buckets.tex (with mean ± std on IoU & |dz|)
    bk = res["v91_buckets"]
    comp = res["v91_composite"]
    weights = res["meta"]["bucket_weights"]
    rows_b = [
        ("ground_clean", "Ground Clean", f"{weights['ground_clean']:.2f}"),
        ("ground_hard_large_detrend", "Hard-Large Detrended", f"{weights['ground_hard_large_detrend']:.2f}"),
        ("airborne_relief_drape", "Airborne Relief Drape", f"{weights['airborne_relief_drape']:.2f}"),
        ("multi_aspect", "Multi-Aspect ($\\Delta z/\\Delta x$)", f"{weights['multi_aspect']:.2f}"),
    ]
    t1 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Held-out multi-regime test performance of \texttt{Swin3D v9.1} (\valVNineOneParams{} parameters, epoch~\valVNineOneEp{}) across four stratified survey regimes ($N=\valNBucketTotal{}$ scenes, disjoint test seed~\valTestSeed{}). Columns report regime weight (Wt.), Smooth 3D IoU (mean\,$\pm$\,std), subsurface RMSE and foreground target RMSE (FG-RMSE), 3-voxel dilated foreground SSIM ($\text{SSIM}_{d_3}$), vertical centroid error $|\Delta z|$ (mean\,$\pm$\,std), foreground mass recovery ratio (Amp.), effective gravity misfit, and Laplace $1$-$b$ coverage (ideal $63.2\%$).}",
        r"\label{tab:v91_buckets}",
        r"\small",
        r"\setlength{\tabcolsep}{4pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{lccccccccc}",
        r"\toprule",
        r"Regime Bucket & Wt. & IoU $\uparrow$ & RMSE $\downarrow$ & FG-RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & $1$-$b$ Cov. \\",
        r"\midrule",
    ]
    for key, label, wt in rows_b:
        m = bk[key]
        t1.append(
            f"{label} & {wt} & {_f_pm(m.get('iou'), m.get('iou_std'), 3)} & {_f(m.get('rmse'),3)} & {_f(m.get('fg_rmse'),3)} & "
            f"{_f(m.get('ssim_d3'),3)} & {_f_pm(m.get('centroid_dz'), m.get('centroid_dz_std'), 1)} & {_f(m.get('amp_ratio'),3)} & "
            f"{_f(m.get('data_misfit_eff'),2,100.0)} & {_f(m.get('fg_1sig_cov'),1,100.0)}\\% \\\\"
        )
    t1.append(r"\midrule")
    t1.append(
        rf"\textbf{{Weighted Composite}} & 1.00 & \textbf{{{_f_pm(comp.get('iou'), comp.get('iou_std'), 3)}}} & "
        rf"\textbf{{{_f(comp.get('rmse'),3)}}} & \textbf{{{_f(comp.get('fg_rmse'),3)}}} & "
        rf"\textbf{{{_f(comp.get('ssim_d3'),3)}}} & \textbf{{{_f_pm(comp.get('centroid_dz'), comp.get('centroid_dz_std'), 1)}}} & "
        rf"\textbf{{{_f(comp.get('amp_ratio'),3)}}} & \textbf{{{_f(comp.get('data_misfit_eff'),2,100.0)}}} & "
        rf"\textbf{{{_f(comp.get('fg_1sig_cov'),1,100.0)}\%}} \\"
    )
    t1.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_v91_buckets.tex").write_text("\n".join(t1), encoding="utf-8")

    # 2. table_ablation.tex (Honest chronological progression WITH Ep. column & std!)
    abl_a = res["ablation"]["panel_a_realistic"]
    abl_b = res["ablation"]["panel_b_clean_ground"]

    def _emit_prog_panel(specs: list[tuple[str, str]], panel_dict: dict) -> list[str]:
        keys = [k for k, _ in specs if k in panel_dict]
        ious_b = _bold_best_rounded([panel_dict[k]["metrics"]["iou"] for k in keys], True, 3)
        dzs_b = _bold_best_rounded([panel_dict[k]["metrics"]["centroid_dz"] for k in keys], False, 1)
        amps_b = _bold_best_rounded([panel_dict[k]["metrics"]["amp_ratio"] for k in keys], True, 3, target=1.0)
        misfits_b = _bold_best_rounded([panel_dict[k]["metrics"]["data_misfit_eff"] for k in keys], False, 2, 100.0)
        rmses_b = _bold_best_rounded([panel_dict[k]["metrics"]["rmse"] for k in keys], False, 3)
        ssims_b = _bold_best_rounded([panel_dict[k]["metrics"]["ssim_d3"] for k in keys], True, 3)
        out = []
        for idx, k in enumerate(keys):
            d = panel_dict[k]
            lbl = dict(specs)[k]
            p_m = f"{d['params'] / 1e6:.2f}M"
            iou_s = rf"{ious_b[idx]}\,$\pm$\,{_f(d['metrics'].get('iou_std'), 3)}"
            out.append(
                f"{lbl} & {d['epoch']} & {p_m} & {d['in_channels']} & {_tex_escape(d['desc'])} & "
                f"{iou_s} & {dzs_b[idx]} & {amps_b[idx]} & {misfits_b[idx]} & {rmses_b[idx]} & {ssims_b[idx]} \\\\"
            )
        return out

    specs_a = [
        ("v6", r"\texttt{v6}"),
        ("v6.1", r"\texttt{v6.1}$^\dagger$"),
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
        r"\caption{Chronological architecture progression evaluated on identical held-out test sets (disjoint seed~\valTestSeed{}). Panel~A evaluates the 4-bucket realistic survey suite ($N=\valNAblBucket{}$); Panel~B evaluates the clean ground benchmark ($N=\valNCleanStd{}$ standard + $\valNCleanHard{}$ hard scenes). Columns include best checkpoint epoch (Ep.), parameter count, input channels (Ch.), Green's Vertical Bias (GVB), and lithology coherence (Coh.). $^\dagger$\texttt{v6.1} was warm-started from \texttt{v6} epoch~236. Because training budgets differ across historical runs, Section~\ref{sec:experiments} also compares \texttt{v9.1} at matched epoch~\valMatchedEp{} (IoU~\valMatchedIoU{}, $|\Delta z|=\valMatchedDz{}$\,m) to separate architectural gains from longer training. Bold marks best per panel (ties after rounding both bolded).}",
        r"\label{tab:ablation}",
        r"\small",
        r"\setlength{\tabcolsep}{3.5pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{lccllcccccc}",
        r"\toprule",
        r"Model & Ep. & Params & Ch. & Key Configuration & IoU (mean\,$\pm$\,std) $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ \\",
        r"\midrule",
        r"\multicolumn{11}{l}{\textit{Panel A: Multi-Regime Realistic Survey Suite (ground, hard-detrended, airborne drape, multi-aspect)}} \\",
        *_emit_prog_panel(specs_a, abl_a),
        r"\midrule",
        r"\multicolumn{11}{l}{\textit{Panel B: Clean Ground Survey Benchmark (standard + hard composite targets)}} \\",
        *_emit_prog_panel(specs_b, abl_b),
        r"\bottomrule",
        r"\end{tabular}%",
        r"}",
        r"\end{table}",
        "",
    ]
    (_PAPER_DIR / "table_ablation.tex").write_text("\n".join(t2), encoding="utf-8")

    # Helper for 3-method comparison tables (table_canonical.tex and table_ood.tex)
    def _write_3way_table(
        bdict: dict,
        order: tuple[str, ...],
        caption: str,
        label: str,
        out_name: str,
    ) -> None:
        method_order = (
            ("tikhonov_l2", r"Tikhonov $L_2$ ($\gamma=0.50$)"),
            ("focusing_irls", r"Focusing IRLS ($\gamma=0.75$)"),
            ("swin3d_v9_1", r"\texttt{Swin3D v9.1}"),
        )
        lines = [
            r"\begin{table}[t]",
            r"\centering",
            rf"\caption{{{caption}}}",
            rf"\label{{{label}}}",
            r"\small",
            r"\setlength{\tabcolsep}{4pt}",
            r"\resizebox{\textwidth}{!}{%",
            r"\begin{tabular}{llcccccccc}",
            r"\toprule",
            r"Benchmark & Method & IoU $\uparrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & $\chi^2_{\mathrm{red}}$ $\to 1$ & Time [s] $\downarrow$ \\",
            r"\midrule",
        ]
        for cname in order:
            cobj = bdict[cname]
            ms = [cobj["methods"][mk] for mk, _ in method_order]
            ious = _bold_best_rounded([m["iou"] for m in ms], True, 3)
            rmses = _bold_best_rounded([m["rmse"] for m in ms], False, 3)
            ssims = _bold_best_rounded([m["ssim_d3"] for m in ms], True, 3)
            dzs = _bold_best_rounded([m["centroid_dz"] for m in ms], False, 1)
            amps = _bold_best_rounded([m["amp_ratio"] for m in ms], True, 3, target=1.0)
            misfits = _bold_best_rounded([m["data_misfit_eff"] for m in ms], False, 2, 100.0)
            chis = _bold_best_rounded([m.get("chi2_red", float("nan")) for m in ms], True, 2, target=1.0)
            times = _bold_best_rounded([m["wall_time_s"] for m in ms], False, 2)
            for idx, (_, mlbl) in enumerate(method_order):
                b_cell = rf"\multirow{{3}}{{*}}{{{cobj['title']}}}" if idx == 0 else ""
                lines.append(
                    f"{b_cell} & {mlbl} & {ious[idx]} & {rmses[idx]} & {ssims[idx]} & "
                    f"{dzs[idx]} & {amps[idx]} & {misfits[idx]} & {chis[idx]} & {times[idx]} \\\\"
                )
            lines.append(r"\midrule")

        mean_ms = []
        for mk, _ in method_order:
            mean_ms.append({
                k: float(np.mean([bdict[c]["methods"][mk][k] for c in order]))
                for k in ("iou", "rmse", "ssim_d3", "centroid_dz", "amp_ratio", "data_misfit_eff", "chi2_red", "wall_time_s")
            })
        ious = _bold_best_rounded([m["iou"] for m in mean_ms], True, 3)
        rmses = _bold_best_rounded([m["rmse"] for m in mean_ms], False, 3)
        ssims = _bold_best_rounded([m["ssim_d3"] for m in mean_ms], True, 3)
        dzs = _bold_best_rounded([m["centroid_dz"] for m in mean_ms], False, 1)
        amps = _bold_best_rounded([m["amp_ratio"] for m in mean_ms], True, 3, target=1.0)
        misfits = _bold_best_rounded([m["data_misfit_eff"] for m in mean_ms], False, 2, 100.0)
        chis = _bold_best_rounded([m["chi2_red"] for m in mean_ms], True, 2, target=1.0)
        times = _bold_best_rounded([m["wall_time_s"] for m in mean_ms], False, 2)
        mean_lbl = rf"\textbf{{Mean ({len(order)} Benchmarks)}}"
        for idx, (_, mlbl) in enumerate(method_order):
            b_cell = rf"\multirow{{3}}{{*}}{{{mean_lbl}}}" if idx == 0 else ""
            lines.append(
                f"{b_cell} & {mlbl} & {ious[idx]} & {rmses[idx]} & {ssims[idx]} & "
                f"{dzs[idx]} & {amps[idx]} & {misfits[idx]} & {chis[idx]} & {times[idx]} \\\\"
            )
        lines.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
        (_PAPER_DIR / out_name).write_text("\n".join(lines), encoding="utf-8")

    # 3. table_canonical.tex
    _write_3way_table(
        res["canonical"],
        ("block_ore", "dipping_dyke", "twin_opposing", "sedimentary_basin"),
        caption=(
            r"Head-to-head comparison against depth-weighted Tikhonov $L_2$ ($\gamma=0.50$, up to 10 cooling iterations) "
            r"and Portniaguine--Zhdanov Focusing IRLS ($\gamma=0.75$, 12 iterations) on four canonical 3D benchmarks "
            r"(single deterministic realizations, 2\% white Gaussian noise, \valCpuThreads{} CPU threads). "
            r"Bold marks best method per benchmark (ties after rounding both bolded; closest to $1.0$ for Amp.\ and $\chi^2_{\mathrm{red}}$)."
        ),
        label="tab:canonical",
        out_name="table_canonical.tex",
    )

    # 4. table_ood.tex (Independent Non-Inverse-Crime OOD Benchmarks)
    _write_3way_table(
        res["ood_noninversecrime"],
        ("ood_sphere", "ood_graded_dome", "ood_bipolar_curved"),
        caption=(
            r"Independent non-inverse-crime out-of-distribution (OOD) benchmark comparison (2\% white Gaussian noise). "
            r"Forward anomalies are synthesized without the $50$\,m training operator: an exact closed-form analytical sphere "
            r"and two continuous curved/depth-graded structures integrated via direct exact Nagy prism summation on a $2\times$ finer "
            r"$25$\,m mesh ($128\times 128\times 64$) at exact station elevations. Bold marks best per benchmark."
        ),
        label="tab:ood",
        out_name="table_ood.tex",
    )

    # 5. table_noise.tex (Swin3D N=24 + paired Focusing IRLS N=6)
    ns = res["noise_sweep"]
    t4 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Sensitivity to surface measurement noise level and spatial correlation on unified ground-survey scenes ($N=\valNNoise{}$ for \texttt{Swin3D v9.1}, and a paired $N=\valNIrlsNoise{}$ subset for classical Focusing IRLS). White Gaussian noise is compared against spatially smoothed colored noise ($\sigma_c = 1.5\,\Delta x = 75$\,m).}",
        r"\label{tab:noise}",
        r"\small",
        r"\setlength{\tabcolsep}{4.5pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{llcccccccc}",
        r"\toprule",
        r"Method & Noise Type & $\sigma / \max|d|$ & IoU (mean\,$\pm$\,std) $\uparrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ & $\chi^2_{\mathrm{red}}$ \\",
        r"\midrule",
    ]
    for kind, klbl, keys in (
        ("white", "White Gaussian", ("0pct", "1pct", "2pct", "5pct", "10pct")),
        ("colored", r"Correlated ($\sigma_c=1.5\Delta x$)", ("1pct", "2pct", "5pct", "10pct")),
    ):
        for idx, k in enumerate(keys):
            m = ns[kind][k]
            pct_str = k.replace("pct", r"\%")
            m_cell = rf"\multirow{{{len(keys)}}}{{*}}{{\texttt{{Swin3D v9.1}}}}" if idx == 0 else ""
            t_cell = rf"\multirow{{{len(keys)}}}{{*}}{{{klbl}}}" if idx == 0 else ""
            t4.append(
                f"{m_cell} & {t_cell} & {pct_str} & {_f_pm(m.get('iou'), m.get('iou_std'), 3)} & {_f(m.get('rmse'),3)} & "
                f"{_f(m.get('ssim_d3'),3)} & {_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & "
                f"{_f(m.get('data_rel_misfit'),2,100.0)} & {_f(m.get('chi2_red'),2)} \\\\"
            )
        t4.append(r"\midrule")

    # Paired Focusing IRLS colored noise rows
    if ns.get("irls_colored"):
        keys_ic = ("1pct", "2pct", "5pct", "10pct")
        for idx, k in enumerate(keys_ic):
            m = ns["irls_colored"][k]
            pct_str = k.replace("pct", r"\%")
            m_cell = rf"\multirow{{{len(keys_ic)}}}{{*}}{{Focusing IRLS ($N=\valNIrlsNoise{{}}$)}}" if idx == 0 else ""
            t_cell = rf"\multirow{{{len(keys_ic)}}}{{*}}{{Correlated ($\sigma_c=1.5\Delta x$)}}" if idx == 0 else ""
            t4.append(
                f"{m_cell} & {t_cell} & {pct_str} & {_f_pm(m.get('iou'), m.get('iou_std'), 3)} & {_f(m.get('rmse'),3)} & "
                f"{_f(m.get('ssim_d3'),3)} & {_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & "
                f"{_f(m.get('data_rel_misfit'),2,100.0)} & {_f(m.get('chi2_red'),2)} \\\\"
            )
    t4.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_noise.tex").write_text("\n".join(t4), encoding="utf-8")

    # 6. table_depth_clearance.tex (with mean ± std on IoU)
    dq = res["depth_quartiles"]
    clr = res["clearance_sweep"]
    t5 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Depth-quartile resolution across the held-out $N=\valNBucketTotal{}$ test suite (Panel~A) and paired sensor-clearance sweep at fixed 1.5\% white noise on the same $N=\valNNoise{}$ ground scenes used in Table~\ref{tab:noise} (Panel~B).}",
        r"\label{tab:depth_clearance}",
        r"\small",
        r"\setlength{\tabcolsep}{5pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{lccccccc}",
        r"\toprule",
        r"Sub-group / Regime & $N$ & IoU (mean\,$\pm$\,std) $\uparrow$ & RMSE $\downarrow$ & $\text{SSIM}_{d_3}$ $\uparrow$ & $|\Delta z|$ [m] $\downarrow$ & Amp. $\to 1$ & Misfit [\%] $\downarrow$ \\",
        r"\midrule",
        r"\multicolumn{8}{l}{\textit{Panel A: Source Depth Quartiles (mass-weighted depth below terrain)}} \\",
    ]
    for q in dq:
        m = q["metrics"]
        rng_s = f"{q['quartile']} ({q['depth_lo_m']:.0f}--{q['depth_hi_m']:.0f}\\,m)"
        t5.append(
            f"{rng_s} & {q['n']} & {_f_pm(m.get('iou'), m.get('iou_std'), 3)} & {_f(m.get('rmse'),3)} & {_f(m.get('ssim_d3'),3)} & "
            f"{_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & {_f(m.get('data_misfit_eff'),2,100.0)} \\\\"
        )
    t5.append(r"\midrule")
    t5.append(r"\multicolumn{8}{l}{\textit{Panel B: Paired Survey Clearance Height Above Terrain (1.5\% white noise)}} \\")
    for ck in ("0.35m", "5m", "20m", "60m", "120m", "160m"):
        m = clr[ck]
        t5.append(
            f"Clearance $h = {ck[:-1]}$\\,m & {res['meta']['noise_n']} & {_f_pm(m.get('iou'), m.get('iou_std'), 3)} & {_f(m.get('rmse'),3)} & "
            f"{_f(m.get('ssim_d3'),3)} & {_f(m.get('centroid_dz'),1)} & {_f(m.get('amp_ratio'),3)} & {_f(m.get('data_rel_misfit'),2,100.0)} \\\\"
        )
    t5.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_depth_clearance.tex").write_text("\n".join(t5), encoding="utf-8")

    # 7. table_timing.tex (Matched 4-thread CPU & CoreML; zero Hybrid; clear Python/PyTorch FFT attribution)
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
        r"\texttt{Swin3D v9.1} (ONNX Runtime CPU, 4 threads)",
        f"{tm['onnx_cpu_ms']:.1f}\\,ms",
        f"{foc_s * 1000.0 / tm['onnx_cpu_ms']:.0f}$\\times$",
        f"{tm['onnx_cpu_max_rel_diff']:.2e}",
    ))
    rows_t.append((
        r"\texttt{Swin3D v9.1} (PyTorch CPU, 4 threads)",
        f"{tm['pytorch_cpu_ms']:.1f}\\,ms",
        f"{foc_s * 1000.0 / tm['pytorch_cpu_ms']:.0f}$\\times$",
        "Reference",
    ))
    rows_t.append((
        r"Tikhonov $L_2$ (Python/PyTorch FFT, 10 iters, 4 threads)",
        f"{tm['tikhonov_l2_s']:.2f}\\,s",
        f"{foc_s / tm['tikhonov_l2_s']:.1f}$\\times$",
        "---",
    ))
    rows_t.append((
        r"Focusing IRLS (Python/PyTorch FFT, 12 iters, 4 threads)",
        f"{foc_s:.2f}\\,s",
        "1.0$\\times$",
        "---",
    ))
    t6 = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Single-volume ($64\times 64\times 32$) median wall-clock runtime and ONNX numerical parity on a MacBook Air (Apple~M4, 32\,GB unified memory). Neural timings report the median of 10 runs; classical timings report the median across the 4 canonical benchmarks in Table~\ref{tab:canonical}, all constrained to \valCpuThreads{} CPU threads for matched comparison.}",
        r"\label{tab:timing}",
        r"\small",
        r"\setlength{\tabcolsep}{6pt}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{@{}lccc@{}}",
        r"\toprule",
        r"Solver / Execution Backend & Median Runtime & Speedup vs.\ IRLS & Max Rel.\ Parity Diff \\",
        r"\midrule",
    ]
    for r_lbl, r_t, r_sp, r_df in rows_t:
        t6.append(f"{r_lbl} & {r_t} & {r_sp} & {r_df} \\\\")
    t6.extend([r"\bottomrule", r"\end{tabular}%", r"}", r"\end{table}", ""])
    (_PAPER_DIR / "table_timing.tex").write_text("\n".join(t6), encoding="utf-8")


def draw_fig1_architecture(res: dict) -> None:
    """Clean schematic architecture diagram of PotentialFieldTransformer3DV9 (no Hybrid box, accurate specs)."""
    fig, ax = plt.subplots(figsize=(13.5, 5.7))
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
            ax.text(x + w / 2, y + h * 0.66, title, ha="center", va="center", fontsize=fs, fontweight="bold", color="#111827")
            ax.text(x + w / 2, y + h * 0.27, sub, ha="center", va="center", fontsize=fs - 1.2, color="#374151")
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

    # Row 1: Input -> Fixed Stem -> 2D Encoder -> Lifter -> 3D Swin+Axial
    box(1.5, 32, 14.5, 8, "6-Ch Survey Input", "(B, 6, 64, 64)\n" + r"$\Delta g_z$, terr, mask, asp, clr, dt", fc="#eff6ff", ec="#1d4ed8")
    box(19.0, 32, 16.5, 8, "Fixed 19-Ch Stem", "asinh, multi-scale LP,\n" + r"FD $g_{zz}$, Euler, struct. tensor", fc="#eff6ff", ec="#1d4ed8")
    box(38.5, 32, 16.5, 8, "2D U-Net + Global Attn", r"enc0 ($64^2$) $\rightarrow$ enc2 ($16^2$)" + "\n+ 9-D additive scale MLP", fc="#f0fdf4", ec="#15803d")
    box(58.0, 32, 18.0, 8, "2D-to-3D Depth Lifter", r"$(8\times 16\times 16\times 128)$ tokens" + "\n" + r"+ $1/d^2$ sensitivity cond.", fc="#f0fdf4", ec="#15803d")
    box(79.0, 32, 19.5, 8, "3D Swin + Axial Blocks", r"$6\times$ alternating W/SW-MSA" + "\n" + r"+ Green's Vertical Bias $B_z$", fc="#fef3c7", ec="#b45309")

    arrow(16.3, 36, 18.7, 36)
    arrow(35.8, 36, 38.2, 36)
    arrow(55.3, 36, 57.7, 36)
    arrow(76.3, 36, 78.7, 36)

    # Row 2: Prototypes -> Stage 1 -> OddPyramid Mid -> Stage 2
    box(79.0, 18, 19.5, 8, "7-Class Lithology Attn", r"voxels $\leftrightarrow$ 21 prototypes" + "\n(bg, air, water, 4 rock bands)", fc="#fef3c7", ec="#b45309")
    box(54.0, 18, 21.0, 8, r"Stage-1 Decoder ($16\times 32\times 32$)", "SepSubpixelUp + DepthSkip\n" + r"$\rightarrow$ occ $\cdot$ coarse $\hat{\rho}_1$", fc="#f3e8ff", ec="#6b21a8")
    box(29.0, 18, 21.0, 8, "Stage-1 Physics Residual", "Frozen OddPyramidGravity\n" + r"+ Frozen BorderProj + $D_1^{-1}\tilde{G}_1^T r_{\mathrm{mid}}$", fc="#ffe4e6", ec="#be123c")
    box(2.5, 18, 22.5, 8, r"Stage-2 Decoder ($32\times 64\times 64$)", "Unified full-res SepSubpixelUp\n" + r"+ DepthSkip $\rightarrow \rho_{\mathrm{init}}$", fc="#f3e8ff", ec="#6b21a8")

    arrow(88.7, 31.7, 88.7, 26.3)
    arrow(78.7, 22, 75.3, 22)
    arrow(53.7, 22, 50.3, 22)
    arrow(28.7, 22, 25.3, 22)

    # Row 3: Stage 2 Physics Feedback -> Final Refinement -> Dual 3D Outputs (3 balanced wide boxes)
    box(2.5, 3, 29.5, 8.5, "Stage-2 Physics Feedback", r"Frozen OddPyramidGravity ($32\times 64\times 64$)" + "\n" + r"$D_1^{-1}\tilde{G}^T(\hat{r}_{64})\cdot w_{\mathrm{bp}}(d_{\mathrm{surf}})$ + 2D gate", fc="#ffe4e6", ec="#be123c")
    box(36.0, 3, 28.5, 8.5, "Refinement & Mixture Head", "2.5D separable conv + depth prior\n+ occ-gated 7-class density mixture", fc="#ecfeff", ec="#0e7490")
    box(68.5, 3, 29.5, 8.5, "Dual 3D Volumetric Outputs", r"Density Contrast $\Delta\hat{\rho}$ ($32\times 64\times 64$)" + "\n" + r"+ Laplace Scale $\hat{b}_\rho = \exp(\hat{s}/2)$", fc="#ecfeff", ec="#0e7490")

    arrow(13.7, 17.7, 13.7, 11.8)
    arrow(32.3, 7.2, 35.7, 7.2)
    arrow(64.8, 7.2, 68.2, 7.2)

    # Skip connections
    arrow(51.0, 31.7, 64.5, 26.3, color="#15803d", lw=1.0, ls=":", rad=-0.08)
    ax.text(58.8, 29.2, "f1 skip", fontsize=7.0, color="#15803d", style="italic",
            bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none", alpha=0.85))
    arrow(42.5, 31.7, 13.7, 26.3, color="#15803d", lw=1.0, ls=":", rad=0.08)
    ax.text(26.2, 29.2, "f0 skip", fontsize=7.0, color="#15803d", style="italic",
            bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none", alpha=0.85))

    ax.text(50.0, 42.3, f"Physics-Coupled 3D Swin Transformer (Swin3D v9.1 Base, {res['meta']['checkpoint_hard_params']:,} Params)",
            ha="center", va="center", fontsize=10.5, fontweight="bold", color="#0f172a")
    _save_fig(fig, "fig1_architecture")


def draw_fig2_training_ablation(res: dict) -> None:
    """3-panel convergence, weighted loss share, and chronological architecture progression plot."""
    hist_p = _MODELS_DIR / "training_history_v9.1_hard.json"
    with open(hist_p, encoding="utf-8") as f:
        hist = json.load(f)["epochs"]

    ep = np.array([e["epoch"] for e in hist])
    val_loss = np.array([e["val_loss"] for e in hist])
    iou = np.array([e["iou"] for e in hist])
    dz = np.array([e["centroid_dz"] for e in hist])

    best_ep = int(res["meta"]["checkpoint_hard_epoch"])
    best_e = next((e for e in hist if int(e["epoch"]) == best_ep), hist[int(np.argmin(val_loss))])

    fig, axes = plt.subplots(1, 3, figsize=(15.2, 4.4), gridspec_kw={"width_ratios": [1.18, 0.90, 1.22], "wspace": 0.68})

    # Panel (a): Full training trajectory (unclipped val_loss!)
    ax = axes[0]
    ax2 = ax.twinx()
    l1 = ax.plot(ep, iou, color="#1d4ed8", lw=1.6, label="Smooth IoU (left)")
    l2 = ax.plot(ep, val_loss, color="#64748b", lw=1.2, ls="--", label="Val Loss (left)")
    l3 = ax2.plot(ep, dz, color="#b91c1c", lw=1.3, alpha=0.85, label=r"$|\Delta z|$ [m] (right)")
    ax.axvline(best_ep, color="#0f172a", ls=":", lw=1.0)
    ax.text(best_ep - 12, 0.36, f"Best ep {best_ep}", ha="right", va="bottom", fontsize=7.0,
            bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="#cbd5e1", alpha=0.9))
    ax.set_xlabel("Training Epoch")
    ax.set_ylabel("Smooth IoU / Val Loss")
    ax2.set_ylabel(r"Depth Centroid Error $|\Delta z|$ [m]", color="#b91c1c")
    ax.set_ylim(0.30, max(1.65, float(np.percentile(val_loss, 98)) * 1.05))
    ax2.set_ylim(25, 115)
    ax.grid(alpha=0.25, lw=0.5)
    lines = l1 + l2 + l3
    ax.legend(lines, [ln.get_label() for ln in lines], loc="upper right", fontsize=6.9, frameon=True, framealpha=0.92)
    ax.set_title("(a) Swin3D v9.1 Multi-Regime Convergence")

    # Panel (b): Weighted loss breakdown at best epoch
    ax = axes[1]
    comp_keys = [
        ("val_wloss_sup", "Structural Sup."),
        ("val_wloss_cls", "Lithology CE/Dice"),
        ("val_wloss_nll", r"Laplace $\beta$-NLL"),
        ("val_wloss_coarse", "Stage-1 Coarse"),
        ("val_wloss_rho_init", r"Stage-2 $\rho_{\mathrm{init}}$"),
        ("val_wloss_data", "Gravity Misfit"),
        ("val_wloss_edt", "Interior EDT"),
        ("val_wloss_coherence", "Lith. Coherence"),
    ]
    vals = [float(best_e.get(k, 0.0)) for k, _ in comp_keys]
    tot_w = max(sum(vals), 1e-8)
    shares = [100.0 * v / tot_w for v in vals]
    lbls = [lbl for _, lbl in comp_keys]
    y_pos = np.arange(len(lbls))
    colors = ["#1d4ed8", "#0284c7", "#0d9488", "#7c3aed", "#9333ea", "#dc2626", "#64748b", "#d97706"]
    bars = ax.barh(y_pos, shares, color=colors, edgecolor="#1e293b", linewidth=0.5)
    ax.set_yticks(y_pos)
    ax.set_yticklabels(lbls)
    ax.invert_yaxis()
    ax.set_xlabel("Validation Loss Share [%]")
    ax.set_xlim(0, max(shares) * 1.25)
    ax.grid(axis="x", alpha=0.25, lw=0.5)
    for b, sh in zip(bars, shares):
        ax.text(sh + 1.0, b.get_y() + b.get_height() / 2, f"{sh:.1f}%", va="center", fontsize=7.0)
    ax.set_title(f"(b) Compound Loss Shares (ep {best_ep})")

    # Panel (c): Chronological Architecture Progression with epoch labels + legend + ep-97 marker
    ax = axes[2]
    abl_a = res["ablation"]["panel_a_realistic"]
    ep97 = res["ablation"].get("v91_hard_matched_ep97", {})
    order_a = [("v6", "v6"), ("v6.1", "v6.1"), ("v7", "v7\n(slab)"), ("v8", "v8\n(slab)"), ("v9.1_hard", "v9.1\n(unified)")]
    keys_a = [k for k, _ in order_a if k in abl_a]
    x_pos = np.arange(len(keys_a))
    ious_a = [abl_a[k]["metrics"]["iou"] for k in keys_a]
    dzs_a = [abl_a[k]["metrics"]["centroid_dz"] for k in keys_a]
    eps_a = [abl_a[k]["epoch"] for k in keys_a]

    ax2_c = ax.twinx()
    bar_cols = ["#94a3b8", "#64748b", "#f59e0b", "#d97706", "#1d4ed8"][: len(keys_a)]
    b_h = ax.bar(x_pos, ious_a, width=0.55, color=bar_cols, edgecolor="#1e293b", lw=0.6, label="Smooth IoU (L)")
    l_dz = ax2_c.plot(x_pos, dzs_a, "o-", color="#b91c1c", lw=1.6, ms=5.5, label=r"$|\Delta z|$ [m] (R)")
    h_extra = []
    if ep97 and not math.isnan(ep97.get("iou", float("nan"))):
        h_ref = ax.axhline(ep97["iou"], color="#1d4ed8", ls=":", lw=1.1, label=f"v9.1 @ ep {ep97['epoch']} ({ep97['iou']:.3f})")
        h_extra.append(h_ref)
    ax.set_xticks(x_pos)
    ax.set_xticklabels([f"{dict(order_a)[k]}\n(ep {e})" for k, e in zip(keys_a, eps_a)], fontsize=6.8)
    ax.set_ylim(max(0.0, min(ious_a) - 0.08), min(1.0, max(ious_a) + 0.09))
    ax2_c.set_ylim(max(0.0, min(dzs_a) - 10.0), max(dzs_a) + 14.0)
    ax.set_ylabel("Smooth 3D IoU")
    ax2_c.set_ylabel(r"$|\Delta z|$ [m]", color="#b91c1c")
    ax.grid(axis="y", alpha=0.25, lw=0.5)
    for idx, bv in enumerate(ious_a):
        ax.text(idx, bv + 0.005, f"{bv:.3f}", ha="center", va="bottom", fontsize=6.6, color="#0f172a")
    handles = [b_h, *h_extra, *l_dz]
    ax.legend(handles, [h.get_label() for h in handles], loc="upper left", fontsize=6.4, frameon=True, framealpha=0.92)
    ax.set_title("(c) Held-Out Progression (v6–v9.1)")

    _save_fig(fig, "fig2_training_ablation")


def _plot_xz_section(ax, sec, terr_x, vmax, cmap="coolwarm", badge="", title="", is_sigma=False):
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


def _draw_3way_comparison_fig(
    bdict: dict,
    order: tuple[str, ...],
    slice_path: Path,
    fig_name: str,
    truth_col_title: str = "Ground Truth",
) -> None:
    """N models x 5 columns (Obs gz + colorbar polarity, Truth, Tikhonov L2, Focusing IRLS, Swin3D v9.1)."""
    slices = np.load(slice_path)
    col_methods = (
        ("truth", truth_col_title),
        ("tikhonov_l2", r"Tikhonov $L_2$ ($\gamma=0.50$)"),
        ("focusing_irls", r"Focusing IRLS ($\gamma=0.75$)"),
        ("swin3d_v9_1", "Swin3D v9.1"),
    )
    n_rows = len(order)
    fig = plt.figure(figsize=(14.4, 2.48 * n_rows))
    gs = fig.add_gridspec(
        n_rows, 6,
        width_ratios=[1.08, 0.24, 1.0, 1.0, 1.0, 1.0],
        wspace=0.14, hspace=0.26,
    )

    for row, cname in enumerate(order):
        cobj = bdict[cname]
        d_obs = slices[f"{cname}__d_obs"]
        terr_x = slices[f"{cname}__terr_x"]
        truth_xz = slices[f"{cname}__truth_xz"]
        iy_pk = cobj["iy_pk"]

        ax0 = fig.add_subplot(gs[row, 0])
        gmax = max(float(np.abs(d_obs).max()), 0.1)
        im0 = ax0.imshow(d_obs.T, origin="lower", extent=[0, 3.2, 0, 3.2], cmap="RdBu_r", vmin=-gmax, vmax=gmax, aspect="equal")
        ax0.axhline((iy_pk + 0.5) * 0.05, color="#0f172a", ls="--", lw=1.0)
        ax0.set_ylabel(cobj["title"].replace(" ($45^\\circ$)", "\n($45^\\circ$)").replace(" (", "\n(") + "\nY [km]", fontsize=7.6)
        if row == 0:
            ax0.set_title(r"Surface $\Delta g_z$ [mGal]", fontsize=8.5, pad=3)
        if row == n_rows - 1:
            ax0.set_xlabel("X [km]")
        else:
            ax0.tick_params(labelbottom=False)
        cb0 = fig.colorbar(im0, ax=ax0, fraction=0.046, pad=0.035)
        cb0.ax.tick_params(labelsize=6.0)

        # Spacer column to keep Col 0 colorbar cleanly separated from Col 1 Z [km] ticks
        ax_sp = fig.add_subplot(gs[row, 1])
        ax_sp.axis("off")

        vmax = max(0.35, float(np.abs(truth_xz).max()) * 1.05)
        row_xz_axes = []
        for ci, (mk, clbl) in enumerate(col_methods):
            ax = fig.add_subplot(gs[row, ci + 2])
            row_xz_axes.append(ax)
            sec = slices[f"{cname}__{mk}_xz"] if mk != "truth" else truth_xz
            badge = ""
            if mk != "truth":
                mm = cobj["methods"][mk]
                badge = f"IoU {mm['iou']:.2f}\n|dz| {mm['centroid_dz']:.0f}m"
            im = _plot_xz_section(ax, sec, terr_x, vmax=vmax, cmap="coolwarm", badge=badge, title=(clbl if row == 0 else ""))
            if ci == 0:
                ax.set_ylabel("Z [km]", fontsize=7.5, labelpad=2)
                ax.tick_params(labelsize=6.8)
            else:
                ax.tick_params(labelleft=False)
            if row == n_rows - 1:
                ax.set_xlabel("X [km]")
            else:
                ax.tick_params(labelbottom=False)
            if ci == 3:
                cb = fig.colorbar(im, ax=row_xz_axes, fraction=0.017, pad=0.015)
                cb.set_label(r"$\Delta\rho$ [g/cm$^3$]", fontsize=7.5)
                cb.ax.tick_params(labelsize=6.8)

    _save_fig(fig, fig_name)


def draw_fig3_canonical_comparison(res: dict) -> None:
    _draw_3way_comparison_fig(
        res["canonical"],
        ("block_ore", "dipping_dyke", "twin_opposing", "sedimentary_basin"),
        _RESULTS_DIR / "canonical_slices.npz",
        "fig3_canonical_comparison",
        truth_col_title="Ground Truth",
    )


def draw_fig4_ood_noninversecrime(res: dict) -> None:
    _draw_3way_comparison_fig(
        res["ood_noninversecrime"],
        ("ood_sphere", "ood_graded_dome", "ood_bipolar_curved"),
        _RESULTS_DIR / "ood_slices.npz",
        "fig4_ood_noninversecrime",
        truth_col_title=r"Truth ($25\to 50\,$m)",
    )


def draw_fig5_showcase_composite(res: dict) -> None:
    """6 diverse geological archetypes x 4 columns (Profile gz, Truth, v9.1 Pred, Laplace Scale b_rho)."""
    slices = np.load(_RESULTS_DIR / "showcase_slices.npz")
    meta = res["showcase"]
    n_rows = min(6, len(meta))

    fig, axes = plt.subplots(n_rows, 4, figsize=(13.8, 12.6), gridspec_kw={"wspace": 0.28, "hspace": 0.28})
    x_km = (np.arange(64) + 0.5) * 0.05

    for r in range(n_rows):
        m = meta[r]
        obs_p = slices[f"s{r}__obs_x_prof"]
        pred_p = slices[f"s{r}__pred_x_prof"]
        truth_xz = slices[f"s{r}__truth_xz"]
        pred_xz = slices[f"s{r}__pred_xz"]
        sigma_xz = slices[f"s{r}__sigma_xz"]
        terr_x = slices[f"s{r}__terr_x"]

        ax0 = axes[r, 0]
        ax0.plot(x_km, obs_p, "o", ms=2.2, color="#dc2626", alpha=0.85, label="Observed")
        ax0.plot(x_km, pred_p, "-", lw=1.4, color="#1d4ed8", label="Predicted")
        ax0.set_ylabel(f"{m['label']}\n$\\Delta g_z$ [mGal]", fontsize=8.0)
        ax0.grid(alpha=0.25, lw=0.5)
        if r == 0:
            ax0.set_title("Surface Profile Fit", fontsize=8.5, pad=3)
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
        im_s = _plot_xz_section(axes[r, 3], sigma_xz, terr_x, vmax=smax, cmap="viridis", is_sigma=True, title=(r"Laplace Scale $\hat{b}_\rho$ [g/cm$^3$]" if r == 0 else ""))

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
        cb_p.set_label(r"$\Delta\rho$ [g/cm$^3$]", fontsize=6.5)
        cb_p.ax.tick_params(labelsize=6.5)
        cb_s = fig.colorbar(im_s, ax=axes[r, 3], fraction=0.045, pad=0.03)
        cb_s.set_label(r"$\hat{b}_\rho$", fontsize=6.5)
        cb_s.ax.tick_params(labelsize=6.5)

    _save_fig(fig, "fig5_showcase_composite")


def draw_fig6_sensitivity_depth(res: dict) -> None:
    """3-panel Noise (Swin3D + paired IRLS colored), Depth-Quartile, and Clearance sensitivity plot."""
    ns = res["noise_sweep"]
    dq = res["depth_quartiles"]
    clr = res["clearance_sweep"]

    fig, axes = plt.subplots(1, 3, figsize=(15.0, 4.3), gridspec_kw={"wspace": 0.56})

    # (a) Noise sensitivity with paired Focusing IRLS colored baseline
    ax = axes[0]
    w_lvls = [0, 1, 2, 5, 10]
    c_lvls = [1, 2, 5, 10]
    w_iou = [ns["white"][f"{k}pct"]["iou"] for k in w_lvls]
    c_iou = [ns["colored"][f"{k}pct"]["iou"] for k in c_lvls]
    w_dz = [ns["white"][f"{k}pct"]["centroid_dz"] for k in w_lvls]
    c_dz = [ns["colored"][f"{k}pct"]["centroid_dz"] for k in c_lvls]

    ax2 = ax.twinx()
    l1 = ax.plot(w_lvls, w_iou, "o-", color="#1d4ed8", lw=1.6, label="Swin White IoU (L)")
    l2 = ax.plot(c_lvls, c_iou, "s--", color="#0284c7", lw=1.5, label="Swin Colored IoU (L)")
    l_extra = []
    if ns.get("irls_colored"):
        ic_iou = [ns["irls_colored"][f"{k}pct"]["iou"] for k in c_lvls]
        l_extra = ax.plot(c_lvls, ic_iou, "^-.", color="#0f766e", lw=1.4, label="IRLS Colored IoU (L)")
    l3 = ax2.plot(w_lvls, w_dz, "o:", color="#b91c1c", lw=1.3, label=r"Swin White $|\Delta z|$ (R)")
    l4 = ax2.plot(c_lvls, c_dz, "s:", color="#ea580c", lw=1.3, label=r"Swin Colored $|\Delta z|$ (R)")
    ax.set_xlabel(r"Noise Level $\sigma / \max|d|$ [%]")
    ax.set_ylabel("Smooth 3D IoU")
    ax2.set_ylabel(r"$|\Delta z|$ [m]", color="#b91c1c")
    ax.set_ylim(0.0, 0.80)
    ax2.set_ylim(0.0, max(260.0, max(c_dz) * 1.25))
    ax.grid(alpha=0.25, lw=0.5)
    lines = l1 + l2 + l_extra + l3 + l4
    ax.legend(lines, [ln.get_label() for ln in lines], loc="upper right", ncol=1, fontsize=6.0, frameon=True, framealpha=0.92)
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
    ax.set_ylabel("IoU / Mass Recovery Ratio")
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
    h_iou = [clr[k]["iou"] for k in h_keys]
    h_dz = [clr[k]["centroid_dz"] for k in h_keys]
    ax2 = ax.twinx()
    l1 = ax.plot(range(len(h_keys)), h_iou, "o-", color="#1d4ed8", lw=1.6, label="IoU (L)")
    l2 = ax2.plot(range(len(h_keys)), h_dz, "s-", color="#b91c1c", lw=1.5, label=r"$|\Delta z|$ (R)")
    ax.set_xticks(range(len(h_keys)))
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

    _save_fig(fig, "fig6_sensitivity_depth")


def draw_fig7_uncertainty_calib(res: dict) -> None:
    """3-panel Risk-Coverage (exact pooled AURC), Laplace Foreground Reliability, and Depth Centroid Scatter."""
    cal = np.load(_RESULTS_DIR / "calib_arrays.npz")
    err = cal["err"].astype(np.float64)
    sig = cal["sigma"].astype(np.float64)
    is_fg = cal["is_fg"].astype(bool)
    true_zc = cal["true_zc"].astype(np.float64)
    pred_zc = cal["pred_zc"].astype(np.float64)
    b_ids = cal["bucket_id"]
    comp = res["v91_composite"]

    fig, axes = plt.subplots(1, 3, figsize=(14.0, 4.2), gridspec_kw={"wspace": 0.28})

    # (a) Selective prediction Risk-Coverage Curve (displays EXACT pooled AURC of plotted curve!)
    ax = axes[0]
    n_tot = err.size
    ord_sig = np.argsort(sig)
    ord_oracle = np.argsort(err)
    cum_sig = np.cumsum(err[ord_sig]) / np.arange(1, n_tot + 1)
    cum_ora = np.cumsum(err[ord_oracle]) / np.arange(1, n_tot + 1)
    aurc_pool = float(cum_sig.mean())
    rand_mean = float(err.mean())
    idx_grid = np.linspace(max(10, n_tot // 100), n_tot - 1, 150, dtype=int)
    cov = (idx_grid + 1) / float(n_tot) * 100.0
    ax.plot(cov, cum_sig[idx_grid], color="#1d4ed8", lw=1.7, label=rf"Ordered by $\hat{{b}}_\rho$ (AURC={aurc_pool:.4f})")
    ax.plot(cov, cum_ora[idx_grid], color="#0d9488", lw=1.4, ls="--", label="Oracle lower bound")
    ax.axhline(rand_mean, color="#64748b", lw=1.1, ls=":", label="Random (unranked)")
    ax.set_xlabel("Coverage [%]")
    ax.set_ylabel(r"Cumulative Mean $|\Delta\hat{\rho}-\Delta\rho|$ [g/cm$^3$]")
    ax.grid(alpha=0.25, lw=0.5)
    ax.legend(loc="upper left", fontsize=6.7, frameon=True)
    ax.set_title("(a) Risk–Coverage Curve")

    # (b) Binned empirical error vs predicted Laplace scale b_rho on foreground voxels (truth != 0)
    ax = axes[1]
    fg_err = err[is_fg]
    fg_sig = sig[is_fg]
    n_bins = 10
    qs = np.quantile(fg_sig, np.linspace(0.0, 1.0, n_bins + 1))
    bx, by_rmse, by_mae = [], [], []
    for k in range(n_bins):
        m = (fg_sig >= qs[k]) & (fg_sig <= qs[k + 1] if k == n_bins - 1 else fg_sig < qs[k + 1])
        if m.any():
            bx.append(float(fg_sig[m].mean()))
            by_rmse.append(float(np.sqrt(np.mean(fg_err[m] ** 2))))
            by_mae.append(float(fg_err[m].mean()))
    lim = max(max(bx), max(by_rmse)) * 1.08
    x_diag = np.linspace(0, lim, 50)
    ax.plot(x_diag, x_diag, "k--", lw=1.0, label=r"Ideal Laplace MAE = $\hat{b}_\rho$")
    ax.plot(x_diag, np.sqrt(2.0) * x_diag, color="#64748b", ls=":", lw=1.0, label=r"Ideal Laplace RMSE = $\sqrt{2}\,\hat{b}_\rho$")
    ax.plot(bx, by_mae, "s-", color="#d97706", lw=1.6, ms=4.3, label="Empirical MAE")
    ax.plot(bx, by_rmse, "o-", color="#1d4ed8", lw=1.5, ms=4.3, label="Empirical RMSE")
    ax.text(
        0.96, 0.05,
        f"1-$b$ cov: {comp.get('fg_1sig_cov', 0)*100:.1f}% (ideal 63.2%)\nFG Spearman: {comp.get('fg_unc_spearman', 0):.3f}",
        transform=ax.transAxes, ha="right", va="bottom", fontsize=6.5,
        bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#94a3b8"),
    )
    ax.set_xlabel(r"Predicted Laplace Scale $\hat{b}_\rho$ [g/cm$^3$]")
    ax.set_ylabel(r"Empirical Foreground Error [g/cm$^3$]")
    ax.set_xlim(0, lim)
    ax.set_ylim(0, lim)
    ax.grid(alpha=0.25, lw=0.5)
    ax.legend(loc="upper left", fontsize=6.2, frameon=True)
    ax.set_title("(b) Foreground Laplace Calibration")

    # (c) Depth Centroid Scatter across 4 buckets with signed bias annotation
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
    bias_m = comp.get("centroid_bias_m", 0.0)
    pct50 = comp.get("centroid_within_50m_pct", 0.0)
    ax.text(
        0.96, 0.05,
        f"Mean bias: {bias_m:+.1f} m\nWithin ±50 m: {pct50:.1f}%",
        transform=ax.transAxes, ha="right", va="bottom", fontsize=6.6,
        bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#94a3b8"),
    )
    ax.set_xlabel(r"True Centroid Depth $z_{\mathrm{true}}$ [m]")
    ax.set_ylabel(r"Predicted Centroid Depth $z_{\mathrm{pred}}$ [m]")
    ax.set_xlim(0, zmax)
    ax.set_ylim(0, zmax)
    ax.grid(alpha=0.25, lw=0.5)
    ax.legend(loc="upper left", fontsize=6.4, frameon=True, ncol=1)
    ax.set_title("(c) Vertical Centroid Recovery")

    _save_fig(fig, "fig7_uncertainty_calib")


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
    draw_fig4_ood_noninversecrime(res)
    draw_fig5_showcase_composite(res)
    draw_fig6_sensitivity_depth(res)
    draw_fig7_uncertainty_calib(res)
    print(f"Generated macros, 7 tables, and 7 vector PDF figures in {_PAPER_DIR}")


if __name__ == "__main__":
    main()
