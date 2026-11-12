"""Export a standalone static best-result virtual preview into ``.inv/demo``.

Copies ONLY 2D and 3D visualization data for the best validation epoch alongside
the stripped demo HTML UI — omitting all training loss curves, checkpoints, metrics,
and per-epoch history.
"""

import argparse
import json
import os

from dashboard import build_dashboard_html
from vol3d_io import _atomic_write, decode_field2d_i16_b64, decode_vol3d_rle_b64


def _load_json(path):
    with open(path, "r") as f:
        return json.load(f)


def _select_best_epoch_entry(epochs, target_epoch):
    for entry in epochs:
        if entry.get("epoch") == target_epoch:
            return entry
    return min(epochs, key=lambda e: e.get("val_loss", float("inf")))


def export_demo(save_dir, out_dir):
    """Build static 2D+3D best-result demo bundle in ``out_dir`` from ``save_dir``."""
    hist_path = os.path.join(save_dir, "training_history.json")
    vmeta_path = os.path.join(save_dir, "vol3d_meta.json")
    vbest_path = os.path.join(save_dir, "vol3d_best.json")

    for req_path in (hist_path, vmeta_path, vbest_path):
        if not os.path.exists(req_path):
            raise FileNotFoundError(f"Required source artifact not found: {req_path}")

    hist = _load_json(hist_path)
    vol3d_meta = _load_json(vmeta_path)
    vol3d_best = _load_json(vbest_path)

    best_epoch = int(vol3d_best.get("epoch") or 0)
    epochs = hist.get("epochs") or []
    best_entry = _select_best_epoch_entry(epochs, best_epoch) if epochs else {}
    if best_epoch <= 0:
        best_epoch = int(best_entry.get("epoch") or 1)

    meta_in = hist.get("meta") or {}
    nx = int(meta_in.get("nx") or vol3d_meta.get("nx") or 64)
    ny = int(meta_in.get("ny") or vol3d_meta.get("ny") or 64)
    nz = int(meta_in.get("nz") or vol3d_meta.get("nz") or 32)
    cell = float(meta_in.get("cell") or vol3d_meta.get("cell") or 50.0)

    samples_in = meta_in.get("samples") or []
    vmeta_samples = vol3d_meta.get("samples") or []
    n_samples = max(len(samples_in), len(vmeta_samples))

    pred_slices_list = best_entry.get("pred_slices") or []
    pred_b64_list = vol3d_best.get("pred_b64") or []
    vmax_preds = vol3d_best.get("vmax_preds") or []
    pred_field_b64_list = vol3d_best.get("pred_field_b64") or []
    pred_field_vmax_list = vol3d_best.get("pred_field_vmax") or []

    preview_samples = []
    for idx in range(n_samples):
        sm = samples_in[idx] if idx < len(samples_in) else {}
        vm = vmeta_samples[idx] if idx < len(vmeta_samples) else {}
        iy = int(sm.get("iy", vm.get("iy", ny // 2)))
        iy_clamped = max(0, min(iy, ny - 1))
        vmax = sm.get("vmax", vm.get("vmax", 0.6))

        pred_slice = pred_slices_list[idx] if idx < len(pred_slices_list) else None
        if pred_slice is None and idx == 0:
            pred_slice = best_entry.get("pred_slice")
        if pred_slice is None and idx < len(pred_b64_list) and pred_b64_list[idx]:
            vp = vmax_preds[idx] if idx < len(vmax_preds) else vmax
            vol_zxy = decode_vol3d_rle_b64(pred_b64_list[idx], (nz, nx, ny), vp)
            pred_slice = [[round(float(v), 4) for v in row] for row in vol_zxy[:, :, iy_clamped]]

        pred_profile = None
        if idx < len(pred_field_b64_list) and pred_field_b64_list[idx]:
            pf_vmax = pred_field_vmax_list[idx] if idx < len(pred_field_vmax_list) else 1.0
            field_xy = decode_field2d_i16_b64(pred_field_b64_list[idx], (nx, ny), pf_vmax)
            pred_profile = [round(float(v), 3) for v in field_xy[:, iy_clamped]]

        sample_out = {
            "id": int(sm.get("id", vm.get("id", idx))),
            "iy": iy,
            "y_km": sm.get("y_km", vm.get("y_km")),
            "vmax": vmax,
            "obs_profile": sm.get("obs_profile", []),
            "terrain": sm.get("terrain", []),
            "truth_slice": sm.get("truth_slice", []),
            "pred_slice": pred_slice or [],
        }
        if pred_profile is not None:
            sample_out["pred_profile"] = pred_profile
        tag = sm.get("tag", vm.get("tag"))
        if tag is not None:
            sample_out["tag"] = tag
        preview_samples.append(sample_out)

    preview_payload = {
        "best_epoch": best_epoch,
        "meta": {
            "nx": nx,
            "ny": ny,
            "nz": nz,
            "cell": cell,
            "samples": preview_samples,
        },
    }

    os.makedirs(out_dir, exist_ok=True)
    index_path = os.path.join(out_dir, "index.html")
    preview_js_path = os.path.join(out_dir, "preview_2d.js")
    vmeta_js_path = os.path.join(out_dir, "vol3d_meta.js")
    vbest_js_path = os.path.join(out_dir, "vol3d_best.js")

    _atomic_write(index_path, build_dashboard_html(demo_mode=True))
    _atomic_write(
        preview_js_path,
        f"window.PREVIEW_2D = {json.dumps(preview_payload, separators=(',', ':'))};\n",
    )
    _atomic_write(
        vmeta_js_path,
        f"window.VOL3D_META = {json.dumps(vol3d_meta, separators=(',', ':'))};\n",
    )
    _atomic_write(
        vbest_js_path,
        (
            "window.VOL3D_EPOCHS = window.VOL3D_EPOCHS || {};\n"
            f"window.VOL3D_EPOCHS[{best_epoch}] = {json.dumps(vol3d_best, separators=(',', ':'))};\n"
            f"window.VOL3D_BEST_EPOCH = {best_epoch};\n"
        ),
    )

    return {
        "best_epoch": best_epoch,
        "n_samples": len(preview_samples),
        "out_dir": out_dir,
        "files": ["index.html", "preview_2d.js", "vol3d_meta.js", "vol3d_best.js"],
    }


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    p = argparse.ArgumentParser(description="Export best-result 2D+3D demo bundle without training data")
    p.add_argument(
        "--save-dir",
        type=str,
        default=os.path.join(script_dir, "checkpoints_v3"),
        help="Training checkpoint directory containing training_history.json and vol3d_*.json",
    )
    p.add_argument(
        "--out-dir",
        type=str,
        default=os.path.normpath(os.path.join(script_dir, "..", "demo")),
        help="Output static demo directory",
    )
    args = p.parse_args()
    summary = export_demo(args.save_dir, args.out_dir)
    print(
        f"Exported demo preview for Epoch {summary['best_epoch']} "
        f"({summary['n_samples']} scenes) -> {summary['out_dir']}"
    )


if __name__ == "__main__":
    main()
