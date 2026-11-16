"""Export a standalone static best-result virtual preview into ``.inv/demo``.

Copies ONLY 2D and 3D visualization data for the best validation epoch alongside
the stripped demo HTML UI — omitting all training loss curves, checkpoints, metrics,
and per-epoch history.
"""

import argparse
import json
import os
import torch

from dashboard import build_dashboard_html
from vol3d_io import (
    _atomic_write,
    decode_field2d_i16_b64,
    decode_terrain_u16_b64,
    decode_vol3d_rle_b64,
)


def _load_json(path):
    with open(path, "r") as f:
        return json.load(f)


def _select_best_epoch_entry(epochs, target_epoch):
    for entry in epochs:
        if entry.get("epoch") == target_epoch:
            return entry
    return min(epochs, key=lambda e: e.get("val_loss", float("inf")))


def export_demo(
    save_dir=None,
    out_dir=None,
    checkpoint=None,
    samples_per_category=8,
    random_val=True,
    include_base=False,
    device=None,
    version_tag="v6.1",
    keep_legacy=False,
):
    """Build static 2D+3D best-result demo bundle in ``out_dir``."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if out_dir is None:
        out_dir = os.path.normpath(os.path.join(script_dir, "..", "demo"))
    os.makedirs(out_dir, exist_ok=True)

    # If checkpoint is provided or exists in save_dir, generate fresh sidecars
    if checkpoint:
        from dataset import CLEARANCE_CLAMP_V6, NX, NY, NZ, CELL
        from forward import TerrainGravityOperator
        from model_swin3d import create_model
        from evaluate import export_vol3d_snapshot

        if device is None:
            device = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")

        ck = torch.load(checkpoint, map_location="cpu")
        g = ck.get("grid", {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL})
        grid = (g["nx"], g["ny"], g["nz"], g["cell"], g["cell"], g["cell"])
        arch = ck.get("arch", {})
        variant = str(arch.get("variant", "v5"))
        model = create_model(g["nx"], g["ny"], g["nz"], **arch)
        model.load_state_dict(ck["model_state_dict"])
        model.to(device).eval()

        h_min = -220.0 if variant == "v6" else -85.0
        op = TerrainGravityOperator(*grid, h_min=h_min).to(device)
        clr_clamp = CLEARANCE_CLAMP_V6 if variant == "v6" else 4.0
        best_epoch = int(ck.get("epoch") or 1)

        print(f"Generating 3D volume snapshot from {checkpoint} (variant={variant}, samples_per_cat={samples_per_category}, random_val={random_val})...")
        export_vol3d_snapshot(
            model, op, device, grid, out_dir,
            epoch=best_epoch, is_best=True, clearance_clamp=clr_clamp,
            samples_per_category=samples_per_category,
            random_val=random_val,
            include_base=include_base,
        )
        save_dir = out_dir

    hist_path = os.path.join(save_dir, "training_history.json") if save_dir else None
    vmeta_path = os.path.join(save_dir, "vol3d_meta.json")
    vbest_path = os.path.join(save_dir, "vol3d_best.json")
    if not os.path.exists(vmeta_path) and version_tag:
        alt_meta = os.path.join(save_dir, f"vol3d_{version_tag}_meta.json")
        if os.path.exists(alt_meta):
            vmeta_path = alt_meta
    if not os.path.exists(vbest_path) and version_tag:
        alt_best = os.path.join(save_dir, f"vol3d_{version_tag}_best.json")
        if os.path.exists(alt_best):
            vbest_path = alt_best

    for req_path in (vmeta_path, vbest_path):
        if not os.path.exists(req_path):
            raise FileNotFoundError(f"Required source artifact not found: {req_path}")

    hist = _load_json(hist_path) if (hist_path and os.path.exists(hist_path)) else {}
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
        y_km = sm.get("y_km", vm.get("y_km", round(float(iy_clamped * cell / 1000.0), 2)))

        truth_slice = sm.get("truth_slice")
        if not truth_slice and vm.get("truth_b64"):
            t_vol = decode_vol3d_rle_b64(vm["truth_b64"], (nz, nx, ny), vmax)
            truth_slice = [[round(float(v), 4) for v in row] for row in t_vol[:, :, iy_clamped]]

        terrain = sm.get("terrain")
        if not terrain and vm.get("terrain_b64"):
            t_xy = decode_terrain_u16_b64(vm["terrain_b64"], (nx, ny), nz * cell)
            terrain = [round(float(v), 2) for v in t_xy[:, iy_clamped]]

        obs_profile = sm.get("obs_profile")
        if not obs_profile and vm.get("obs_field_b64"):
            o_vmax = vm.get("obs_field_vmax", 1.0)
            f_xy = decode_field2d_i16_b64(vm["obs_field_b64"], (nx, ny), o_vmax)
            obs_profile = [round(float(v), 3) for v in f_xy[:, iy_clamped]]

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
            "y_km": y_km,
            "vmax": vmax,
            "obs_profile": obs_profile or [],
            "terrain": terrain or [],
            "truth_slice": truth_slice or [],
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

    vmeta_name = f"vol3d_{version_tag}_meta.js" if version_tag else "vol3d_meta.js"
    vbest_name = f"vol3d_{version_tag}_best.js" if version_tag else "vol3d_best.js"
    index_path = os.path.join(out_dir, "index.html")
    preview_js_path = os.path.join(out_dir, "preview_2d.js")
    vmeta_js_path = os.path.join(out_dir, vmeta_name)
    vbest_js_path = os.path.join(out_dir, vbest_name)

    _atomic_write(index_path, build_dashboard_html(demo_mode=True, version_tag=version_tag))
    _atomic_write(
        preview_js_path,
        f"window.PREVIEW_2D = {json.dumps(preview_payload, separators=(',', ':'))};\n",
    )
    _atomic_write(
        vmeta_js_path,
        f"window.VOL3D_META = {json.dumps(vol3d_meta, separators=(',', ':'))};\n",
    )
    vbest_payload = (
        "window.VOL3D_EPOCHS = window.VOL3D_EPOCHS || {};\n"
        f"window.VOL3D_EPOCHS[{best_epoch}] = {json.dumps(vol3d_best, separators=(',', ':'))};\n"
        f"window.VOL3D_BEST_EPOCH = {best_epoch};\n"
    )
    _atomic_write(vbest_js_path, vbest_payload)

    out_files = ["index.html", "preview_2d.js", vmeta_name, vbest_name]

    if keep_legacy:
        legacy_meta = os.path.join(out_dir, "vol3d_meta.js")
        legacy_best = os.path.join(out_dir, "vol3d_best.js")
        _atomic_write(legacy_meta, f"window.VOL3D_META = {json.dumps(vol3d_meta, separators=(',', ':'))};\n")
        _atomic_write(legacy_best, vbest_payload)
        out_files.extend(["vol3d_meta.js", "vol3d_best.js"])
    else:
        for legacy in (
            "vol3d_best.js", "vol3d_meta.js", "vol3d_best.json", "vol3d_meta.json",
            "vol3d_latest.js", "vol3d_latest.json", "vol3d_index.js", "vol3d_index.json",
        ):
            if legacy in (vmeta_name, vbest_name):
                continue
            legacy_p = os.path.join(out_dir, legacy)
            if os.path.exists(legacy_p):
                try:
                    os.remove(legacy_p)
                except OSError:
                    pass

    return {
        "best_epoch": best_epoch,
        "n_samples": len(preview_samples),
        "out_dir": out_dir,
        "files": out_files,
    }


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_ck = os.path.join(script_dir, "checkpoints_v6", "best_swin3d.pt")
    p = argparse.ArgumentParser(description="Export best-result 2D+3D demo bundle without training data")
    p.add_argument(
        "--checkpoint",
        type=str,
        default=default_ck if os.path.exists(default_ck) else None,
        help="Path to trained model checkpoint to evaluate fresh demo samples from (default: checkpoints_v6/best_swin3d.pt)",
    )
    p.add_argument(
        "--save-dir",
        type=str,
        default=os.path.join(script_dir, "checkpoints_v6"),
        help="Training checkpoint directory containing vol3d_*.json",
    )
    p.add_argument(
        "--out-dir",
        type=str,
        default=os.path.normpath(os.path.join(script_dir, "..", "demo")),
        help="Output static demo directory",
    )
    p.add_argument(
        "--samples-per-category",
        type=int,
        default=8,
        help="Number of web examples per category (default: 8)",
    )
    p.add_argument(
        "--random-val",
        action="store_true",
        default=True,
        help="Use random validation data for demo examples (default: True)",
    )
    p.add_argument(
        "--fixed-seed",
        action="store_true",
        help="Use fixed deterministic seed instead of random validation data",
    )
    p.add_argument(
        "--include-base",
        action="store_true",
        help="Include standard/ground and hard/large base sets alongside the 8 categories",
    )
    p.add_argument(
        "--version-tag",
        type=str,
        default="v6.1",
        help="Model version tag for output volume sidecars (default: v6.1)",
    )
    p.add_argument(
        "--keep-legacy",
        action="store_true",
        default=False,
        help="Keep unversioned legacy vol3d_best.js and vol3d_meta.js sidecars",
    )
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    p.add_argument("--device", default=default_dev)
    args = p.parse_args()

    random_val = not args.fixed_seed
    summary = export_demo(
        save_dir=args.save_dir,
        out_dir=args.out_dir,
        checkpoint=args.checkpoint,
        samples_per_category=args.samples_per_category,
        random_val=random_val,
        include_base=args.include_base,
        device=args.device,
        version_tag=args.version_tag,
        keep_legacy=args.keep_legacy,
    )
    print(
        f"Exported demo preview for Epoch {summary['best_epoch']} "
        f"({summary['n_samples']} scenes) -> {summary['out_dir']}"
    )


if __name__ == "__main__":
    main()
