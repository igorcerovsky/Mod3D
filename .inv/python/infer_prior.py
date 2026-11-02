"""Write a neural-network density prior into an `inv` setup JSON (hybrid NN + focusing solver).

    python infer_prior.py --setup setup.json --checkpoint checkpoints/best_swin3d.pt --output setup_prior.json
    inv_app / InversionEngine::run_setup(setup_prior.json)      # solver rescales the prior to the data
                                                               # and focuses relative to it

Pipeline: scattered stations -> bin-average onto the TRAINING grid columns (neighbour fill for empty
columns) -> network -> trilinear resample to the setup grid -> ``setup["prior"]["values"]`` in
VoxelGrid order ``(ix*ny + iy)*nz + iz``.

Caveats (printed as warnings): the network was trained on a flat observation plane with the cell size
stored in the checkpoint; other spacings degrade the prior (the solver still enforces the data fit,
and rescales the prior amplitude by least squares). Planar modelling only.
"""

import argparse
import json

import numpy as np
import torch
import torch.nn.functional as F

from model_swin3d import PotentialFieldTransformer3D


def scattered_to_columns(x, y, v, nx, ny, x0, y0, dx, dy):
    """Bin-average stations into (nx, ny) columns; fill empty columns from neighbours. Returns (nx, ny) array."""
    s = np.zeros((nx, ny))
    n = np.zeros((nx, ny))
    for xi, yi, vi in zip(x, y, v):
        ix, iy = int(np.floor((xi - x0) / dx)), int(np.floor((yi - y0) / dy))
        if 0 <= ix < nx and 0 <= iy < ny:
            s[ix, iy] += vi
            n[ix, iy] += 1
    known = n > 0
    if not known.any():
        raise ValueError("no observation inside the model domain")
    out = np.where(known, s / np.maximum(n, 1), 0.0)
    while not known.all():
        acc = np.zeros_like(out)
        cnt = np.zeros_like(out)
        for ax, sh in ((0, 1), (0, -1), (1, 1), (1, -1)):
            k = np.roll(known, sh, axis=ax)
            val = np.roll(out, sh, axis=ax)
            # undo wrap-around at the borders
            edge = [slice(None), slice(None)]
            edge[ax] = slice(0, 1) if sh == 1 else slice(-1, None)
            k = k.copy()
            k[tuple(edge)] = False
            acc += np.where(k, val, 0.0)
            cnt += k
        new = (~known) & (cnt > 0)
        if not new.any():
            break
        out[new] = acc[new] / cnt[new]
        known |= new
    return out


def predict_prior(setup, checkpoint, device="cpu"):
    ck = torch.load(checkpoint, map_location=device)
    g = ck.get("grid", {"nx": 64, "ny": 64, "nz": 32, "dx": 50.0, "dy": 50.0, "dz": 50.0})
    vd = setup["voxel_domain"]
    nx, ny, nz = vd["nx"], vd["ny"], vd["nz"]
    for k in ("dx", "dy", "dz"):
        if abs(vd[k] - g[k]) > 1e-6:
            print(f"WARNING: setup {k}={vd[k]} m differs from training {g[k]} m; prior quality will degrade")

    obs = setup["observations"]
    # observations on the TRAINING grid footprint (same physical extent as the setup domain)
    cols = scattered_to_columns(obs["x"], obs["y"], obs["gz"], g["nx"], g["ny"], vd["x0"], vd["y0"],
                                vd["nx"] * vd["dx"] / g["nx"], vd["ny"] * vd["dy"] / g["ny"])
    model = PotentialFieldTransformer3D(in_channels=1, out_depth=g["nz"], embed_dim=96, num_heads=4)
    model.load_state_dict(ck["model_state_dict"])
    model.to(device).eval()
    with torch.no_grad():
        x = torch.from_numpy(cols.astype(np.float32))[None, None].to(device)    # (1,1,nx,ny)
        pred = model(x)                                                           # (1,1,nz,nx,ny)
        pred = F.interpolate(pred, size=(nz, nx, ny), mode="trilinear", align_corners=False)
    vol = pred[0, 0].cpu().numpy()                                                # (nz, nx, ny)
    return np.transpose(vol, (1, 2, 0)).ravel(order="C")                          # (ix*ny+iy)*nz+iz


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--setup", required=True)
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--device", default="cpu")
    args = ap.parse_args()
    with open(args.setup) as f:
        setup = json.load(f)
    prior = predict_prior(setup, args.checkpoint, args.device)
    setup["prior"] = {"values": prior.astype(float).tolist()}
    with open(args.output, "w") as f:
        json.dump(setup, f)
    print(f"Wrote prior ({prior.size} cells, peak {prior.max():.3f} / min {prior.min():.3f} g/cm^3) to {args.output}")


if __name__ == "__main__":
    main()
