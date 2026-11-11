"""Export the trained transformer to ONNX for C++ inference (inv::NeuralPriorInference).

    python export_onnx.py --checkpoint ./checkpoints/best_swin3d.pt --output ./models/inv_swin3d.onnx

Interface (static shapes, matches neural_prior.hpp):
    input  "observation_field" (1, 2, 64, 64): [gz mGal, terrain depth / domain depth]
    output "density_volume"    (1, 1, 32, 64, 64): g/cm^3 for 50 m training cells
The export is verified against PyTorch with onnxruntime.
"""

import argparse
import os

import numpy as np
import torch

from dataset import CELL, NX, NY, NZ
from model_swin3d import create_model


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", default="./checkpoints/best_swin3d.pt")
    ap.add_argument("--output", default="./models/inv_swin3d.onnx")
    ap.add_argument("--opset", type=int, default=17)
    args = ap.parse_args()
    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)

    ck = torch.load(args.checkpoint, map_location="cpu")
    g = ck.get("grid", {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL})
    model = create_model(g["nx"], g["ny"], g["nz"], **ck.get("arch", {}))
    model.load_state_dict(ck["model_state_dict"])
    model.eval()

    x = torch.zeros(1, 2, g["nx"], g["ny"])
    xx, yy = np.meshgrid(np.arange(g["nx"]) - 32.0, np.arange(g["ny"]) - 32.0, indexing="ij")
    x[0, 0] = torch.from_numpy(1.5 * np.exp(-(xx ** 2 + yy ** 2) / 60.0)).float()
    x[0, 1] = torch.from_numpy(0.05 * (1 + np.sin(xx / 9.0))).float()

    for stale in (args.output, args.output + ".data"):
        if os.path.exists(stale):
            os.remove(stale)
    torch.onnx.export(model, (x,), args.output, export_params=True, opset_version=args.opset,
                      do_constant_folding=True, input_names=["observation_field"],
                      output_names=["density_volume"], dynamo=False)
    print(f"Exported {args.output} ({os.path.getsize(args.output) / 2**20:.1f} MB)")

    try:
        import onnxruntime as ort
        sess = ort.InferenceSession(args.output, providers=["CPUExecutionProvider"])
        y_onnx = sess.run(None, {"observation_field": x.numpy()})[0]
        with torch.no_grad():
            y_t = model(x).numpy()
        err = np.abs(y_onnx - y_t).max() / (np.abs(y_t).max() + 1e-12)
        print(f"onnxruntime check: output {y_onnx.shape}, max rel diff {err:.2e}")
        assert err < 1e-3
    except ImportError:
        print("onnxruntime not installed; skipped verification")


if __name__ == "__main__":
    main()
