"""Exports trained 3D Swin Potential Field Inversion Transformer to ONNX format.

Enables high-performance C++20 inference via ONNX Runtime in Mod3D.

Usage:
    python export_onnx.py --checkpoint ./checkpoints/best_swin3d.pt --output ./models/inv_swin3d.onnx
"""

import argparse
import os
import torch
from model_swin3d import PotentialFieldTransformer3D


def parse_args():
    parser = argparse.ArgumentParser(description="Export 3D Swin Inversion Model to ONNX")
    parser.add_argument("--checkpoint", type=str, default="", help="Path to PyTorch checkpoint (.pt)")
    parser.add_argument("--output", type=str, default="./models/inv_swin3d.onnx", help="Output ONNX model path")
    parser.add_argument("--opset", type=int, default=17, help="ONNX opset version")
    return parser.parse_args()


def main():
    args = parse_args()
    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)

    print(f"[1] Instantiating 3D Swin Transformer architecture...")
    model = PotentialFieldTransformer3D(in_channels=1, out_depth=32, embed_dim=96, num_heads=4)

    if args.checkpoint and os.path.exists(args.checkpoint):
        print(f"[2] Loading weights from checkpoint: {args.checkpoint}...")
        ckpt = torch.load(args.checkpoint, map_location="cpu")
        state_dict = ckpt["model_state_dict"] if "model_state_dict" in ckpt else ckpt
        model.load_state_dict(state_dict)
    else:
        print("[2] No checkpoint provided; exporting initialized model structure...")

    model.eval()

    # Dummy input: (Batch=1, Channels=1, Height=64, Width=64)
    dummy_input = torch.randn(1, 1, 64, 64, dtype=torch.float32)

    print(f"[3] Exporting model to ONNX format at: {args.output}...")
    torch.onnx.export(
        model,
        dummy_input,
        args.output,
        export_params=True,
        opset_version=args.opset,
        do_constant_folding=True,
        input_names=["observation_field"],
        output_names=["density_volume"],
        dynamic_axes={
            "observation_field": {0: "batch_size", 2: "height", 3: "width"},
            "density_volume": {0: "batch_size", 3: "height", 4: "width"},
        },
    )

    print(f"[4] Successfully exported ONNX model!")
    print(f"    File: {args.output}")
    print(f"    File size: {os.path.getsize(args.output) / (1024 * 1024):.2f} MB")


if __name__ == "__main__":
    main()
