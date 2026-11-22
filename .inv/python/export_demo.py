"""Export standalone static best-result virtual previews into ``.inv/demo``.

Supports single- or multi-model bundles: each model is written to a single
self-contained ``demo_<id>.js`` data file (containing 2D slices + 3D RLE
volumes/terrain/fields), registered in ``demo_models.js``, and selectable via
the Model dropdown in ``index.html``.
"""

import argparse
import glob
import json
import os
import tempfile
import torch

from dashboard import build_dashboard_html
from vol3d_io import (
    _atomic_write,
    decode_field2d_i16_b64,
    decode_terrain_u16_b64,
    decode_vol3d_rle_b64,
)


def _load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _load_existing_manifest(out_dir):
    """Read existing ``demo_models.js`` entries if present."""
    manifest_path = os.path.join(out_dir, "demo_models.js")
    if not os.path.exists(manifest_path):
        return []
    try:
        raw = open(manifest_path, "r", encoding="utf-8").read().strip()
        eq = raw.find("=")
        if eq < 0:
            return []
        payload = raw[eq + 1:].strip().rstrip(";").strip()
        entries = json.loads(payload)
        return entries if isinstance(entries, list) else []
    except Exception:
        return []


def _select_best_epoch_entry(epochs, target_epoch):
    for entry in epochs:
        if entry.get("epoch") == target_epoch:
            return entry
    return min(epochs, key=lambda e: e.get("val_loss", float("inf")))


def _build_bundle_from_dir(save_dir, model_id, label, version_tag=None):
    """Assemble a self-contained 2D+3D demo bundle dict from ``vol3d_*.json``."""
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
    return {
        "id": model_id,
        "label": label,
        "best_epoch": best_epoch,
        "n_samples": len(preview_samples),
        "preview_2d": preview_payload,
        "vol3d_meta": vol3d_meta,
        "vol3d_best": vol3d_best,
    }


def _evaluate_checkpoint_to_bundle(
    checkpoint,
    model_id,
    label,
    samples_per_category=8,
    random_val=True,
    include_base=False,
    device=None,
    v3_data=False,
):
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

    is_v6_variant = variant in ("v6", "v7", "v8", "v9", "v9.1") and (not v3_data)
    h_min = -220.0 if is_v6_variant else -85.0
    op = TerrainGravityOperator(*grid, h_min=h_min).to(device)
    clr_clamp = CLEARANCE_CLAMP_V6 if is_v6_variant else 4.0
    best_epoch = int(ck.get("epoch") or 1)

    print(
        f"Generating 3D volume snapshot for '{model_id}' from {checkpoint} "
        f"(variant={variant}, v3_data={v3_data}, ep={best_epoch}, "
        f"samples_per_cat={samples_per_category}, random_val={random_val})..."
    )
    with tempfile.TemporaryDirectory(prefix=f"demo_{model_id}_") as tmp_dir:
        export_vol3d_snapshot(
            model, op, device, grid, tmp_dir,
            epoch=best_epoch, is_best=True, clearance_clamp=clr_clamp,
            samples_per_category=samples_per_category,
            random_val=random_val,
            include_base=include_base,
            v6_physics=is_v6_variant,
        )
        return _build_bundle_from_dir(tmp_dir, model_id=model_id, label=label)


def export_demo_models(
    model_specs,
    out_dir=None,
    samples_per_category=8,
    random_val=True,
    include_base=False,
    device=None,
    append=False,
):
    """Export one or more models into ``demo_<id>.js`` + ``demo_models.js`` + ``index.html``."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if out_dir is None:
        out_dir = os.path.normpath(os.path.join(script_dir, "..", "demo"))
    os.makedirs(out_dir, exist_ok=True)

    manifest = _load_existing_manifest(out_dir) if append else []
    by_id = {m["id"]: i for i, m in enumerate(manifest)}
    written_files = []

    for spec in model_specs:
        model_id = spec["id"]
        label = spec.get("label") or model_id
        checkpoint = spec.get("checkpoint")
        save_dir = spec.get("save_dir")
        v3_data = bool(spec.get("v3_data", False))

        if checkpoint:
            bundle = _evaluate_checkpoint_to_bundle(
                checkpoint=checkpoint,
                model_id=model_id,
                label=label,
                samples_per_category=samples_per_category,
                random_val=random_val,
                include_base=include_base,
                device=device,
                v3_data=v3_data,
            )
        else:
            bundle = _build_bundle_from_dir(save_dir, model_id=model_id, label=label, version_tag=model_id)

        bundle_file = f"demo_{model_id}.js"
        bundle_path = os.path.join(out_dir, bundle_file)
        bundle_js = (
            "window.DEMO_BUNDLES = window.DEMO_BUNDLES || {};\n"
            f"window.DEMO_BUNDLES[{json.dumps(model_id)}] = {json.dumps(bundle, separators=(',', ':'))};\n"
        )
        _atomic_write(bundle_path, bundle_js)
        written_files.append(bundle_file)

        entry = {
            "id": model_id,
            "label": label,
            "file": bundle_file,
            "best_epoch": bundle["best_epoch"],
            "n_samples": bundle["n_samples"],
        }
        if model_id in by_id:
            manifest[by_id[model_id]] = entry
        else:
            by_id[model_id] = len(manifest)
            manifest.append(entry)

    manifest_path = os.path.join(out_dir, "demo_models.js")
    _atomic_write(
        manifest_path,
        f"window.DEMO_MODELS = {json.dumps(manifest, indent=2)};\n",
    )

    index_path = os.path.join(out_dir, "index.html")
    _atomic_write(
        index_path,
        build_dashboard_html(demo_mode=True, demo_models=manifest),
    )

    # Clean up legacy single-model sidecar files that are no longer needed
    keep_set = {"index.html", "demo_models.js"} | {m["file"] for m in manifest}
    for pattern in ("preview_2d.js", "vol3d_*.js", "vol3d_*.json"):
        for path in glob.glob(os.path.join(out_dir, pattern)):
            if os.path.basename(path) not in keep_set:
                try:
                    os.remove(path)
                except OSError:
                    pass

    return {
        "out_dir": out_dir,
        "models": manifest,
        "files": ["index.html", "demo_models.js"] + written_files,
    }


def _parse_model_spec(raw: str) -> dict:
    """Parse ``id|label|checkpoint[|hard|v3]`` into a model spec dict."""
    parts = [p.strip() for p in raw.split("|")]
    if len(parts) < 3:
        raise ValueError(f"--model expects 'id|label|checkpoint[|hard|v3]', got: {raw!r}")
    model_id, label, ckpt = parts[0], parts[1], parts[2]
    mode = parts[3].lower() if len(parts) >= 4 and parts[3] else "hard"
    if mode not in ("hard", "v3", "v3data", "v3_data"):
        raise ValueError(f"Unknown model mode {mode!r} in --model {raw!r}; expected 'hard' or 'v3'")
    return {
        "id": model_id,
        "label": label or model_id,
        "checkpoint": ckpt,
        "v3_data": mode != "hard",
    }


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_ck = os.path.join(script_dir, "checkpoints_v9.1_hard", "best_swin3d.pt")
    p = argparse.ArgumentParser(description="Export best-result 2D+3D multi-model demo bundle")
    p.add_argument(
        "--model",
        action="append",
        default=[],
        help="Repeatable model spec: 'id|label|checkpoint[|hard|v3]'",
    )
    p.add_argument(
        "--checkpoint",
        type=str,
        default=None,
        help="Single-model checkpoint path (used when --model is not passed)",
    )
    p.add_argument(
        "--save-dir",
        type=str,
        default=os.path.join(script_dir, "checkpoints"),
        help="Training checkpoint/monitor directory when exporting without --checkpoint",
    )
    p.add_argument(
        "--out-dir",
        type=str,
        default=os.path.normpath(os.path.join(script_dir, "..", "demo")),
        help="Output static demo directory (default: .inv/demo)",
    )
    p.add_argument(
        "--samples-per-category",
        type=int,
        default=8,
        help="Number of random validation samples per category (default: 8 -> 64 total)",
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
        default="v9.1_hard",
        help="Model ID for single-model export (default: v9.1_hard)",
    )
    p.add_argument(
        "--label",
        type=str,
        default=None,
        help="Display label in the demo dropdown for single-model export",
    )
    p.add_argument(
        "--v3-data",
        action="store_true",
        default=False,
        help="Evaluate single-model demo samples using V3 ground-clearance physics (h_min=-85, v6_physics=False)",
    )
    p.add_argument(
        "--append",
        action="store_true",
        default=False,
        help="Append/update models in existing demo_models.js instead of replacing it",
    )
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    p.add_argument("--device", default=default_dev)
    args = p.parse_args()

    if args.model:
        model_specs = [_parse_model_spec(raw) for raw in args.model]
    else:
        ckpt = args.checkpoint or (default_ck if os.path.exists(default_ck) else None)
        model_specs = [{
            "id": args.version_tag,
            "label": args.label or args.version_tag,
            "checkpoint": ckpt,
            "save_dir": None if ckpt else args.save_dir,
            "v3_data": args.v3_data,
        }]

    summary = export_demo_models(
        model_specs=model_specs,
        out_dir=args.out_dir,
        samples_per_category=args.samples_per_category,
        random_val=not args.fixed_seed,
        include_base=args.include_base,
        device=args.device,
        append=args.append,
    )
    for m in summary["models"]:
        print(f"  - {m['id']}: {m['label']} (Epoch {m['best_epoch']}, {m['n_samples']} scenes -> {m['file']})")
    print(f"Exported {len(summary['models'])} demo model(s) -> {summary['out_dir']}")


if __name__ == "__main__":
    main()
