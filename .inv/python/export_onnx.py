"""Export the trained transformer to ONNX for C++ inference (inv::NeuralPriorInference).

    python export_onnx.py --checkpoint ./checkpoints/best_swin3d.pt --output ./models/inv_swin3d.onnx

Interface (static shapes, matches neural_prior.hpp):
    v1-v4 input  "observation_field" (1, 2, 64, 64): [gz mGal, terrain depth / domain depth]
          output "density_volume"    (1, 1, 32, 64, 64): g/cm^3 for 50 m training cells
    v5    input  "observation_field" (1, 5, 64, 64): [gz, terr, valid_mask, log(aspect), clearance]
          output "density_volume"    (1, 1, 32, 64, 64)
          output "log_var_volume"    (1, 1, 32, 64, 64)
The export is verified against PyTorch with onnxruntime (CPU and CoreML MLProgram on macOS).
"""

import argparse
import os
import tempfile

import numpy as np
import torch
import torch.nn as nn

from dataset import CELL, NX, NY, NZ
from model_swin3d import PotentialFieldTransformer3DV5, create_model


class _V5UncertaintyExportWrapper(nn.Module):
    def __init__(self, core: PotentialFieldTransformer3DV5):
        super().__init__()
        self.core = core

    def forward(self, x: torch.Tensor):
        return self.core.forward_uncertainty(x)


def export_checkpoint_to_onnx(checkpoint: str, output: str, opset: int = 17, verify: bool = True):
    os.makedirs(os.path.dirname(output) or ".", exist_ok=True)
    ck = torch.load(checkpoint, map_location="cpu")
    g = ck.get("grid", {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL})
    arch = ck.get("arch", {})
    model = create_model(g["nx"], g["ny"], g["nz"], **arch)
    model.load_state_dict(ck["model_state_dict"])
    model.eval()

    is_v5 = isinstance(model, PotentialFieldTransformer3DV5) or arch.get("variant") == "v5"
    in_ch = 5 if is_v5 else 2
    x = torch.zeros(1, in_ch, g["nx"], g["ny"])
    xx, yy = np.meshgrid(np.arange(g["nx"]) - 0.5 * g["nx"], np.arange(g["ny"]) - 0.5 * g["ny"], indexing="ij")
    x[0, 0] = torch.from_numpy(1.5 * np.exp(-(xx ** 2 + yy ** 2) / 60.0)).float()
    x[0, 1] = torch.from_numpy(0.05 * (1 + np.sin(xx / 9.0))).float()
    if is_v5:
        x[0, 2] = 1.0
        x[0, 3] = 0.0
        x[0, 4] = 0.35 / g.get("cell", CELL)

    export_mod = _V5UncertaintyExportWrapper(model).eval() if is_v5 else model
    output_names = ["density_volume", "log_var_volume"] if is_v5 else ["density_volume"]

    for stale in (output, output + ".data"):
        if os.path.exists(stale):
            os.remove(stale)
    torch.onnx.export(
        export_mod, (x,), output, export_params=True, opset_version=opset,
        do_constant_folding=True, input_names=["observation_field"],
        output_names=output_names, dynamo=False,
    )
    print(f"Exported {output} ({os.path.getsize(output) / 2**20:.1f} MB, in_ch={in_ch}, outputs={output_names})")

    if not verify:
        return
    try:
        import onnxruntime as ort
        sess = ort.InferenceSession(output, providers=["CPUExecutionProvider"])
        y_onnx = sess.run(None, {"observation_field": x.numpy()})
        with torch.no_grad():
            y_t = export_mod(x)
            if not isinstance(y_t, tuple):
                y_t = (y_t,)
            y_t = [t.numpy() for t in y_t]
        for name, yo, yt in zip(output_names, y_onnx, y_t):
            err = float(np.abs(yo - yt).max() / (np.abs(yt).max() + 1e-12))
            print(f"onnxruntime CPU check ({name}): shape {yo.shape}, max rel diff {err:.2e}")
            assert err < 1e-3, f"CPU parity check failed for {name}: rel diff {err}"

        if "CoreMLExecutionProvider" in ort.get_available_providers():
            with tempfile.TemporaryDirectory(prefix="inv_coreml_verify_") as cache_dir:
                coreml_opts = {
                    "ModelFormat": "MLProgram",
                    "MLComputeUnits": "ALL",
                    "RequireStaticInputShapes": "1",
                    "ModelCacheDirectory": cache_dir,
                }
                cm_sess = ort.InferenceSession(
                    output,
                    providers=[("CoreMLExecutionProvider", coreml_opts), "CPUExecutionProvider"],
                )
                y_cm = cm_sess.run(None, {"observation_field": x.numpy()})
                for name, yo, yt in zip(output_names, y_cm, y_t):
                    err_cm = float(np.abs(yo - yt).max() / (np.abs(yt).max() + 1e-12))
                    print(f"onnxruntime CoreML MLProgram check ({name}): shape {yo.shape}, max rel diff {err_cm:.2e}")
                    assert err_cm < 1e-2, f"CoreML parity check failed for {name}: rel diff {err_cm}"
    except ImportError:
        print("onnxruntime not installed; skipped verification")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", default="./checkpoints/best_swin3d.pt")
    ap.add_argument("--output", default="./models/inv_swin3d.onnx")
    ap.add_argument("--opset", type=int, default=17)
    ap.add_argument("--no-verify", action="store_true")
    args = ap.parse_args()
    export_checkpoint_to_onnx(args.checkpoint, args.output, opset=args.opset, verify=not args.no_verify)


if __name__ == "__main__":
    main()
