"""Write a neural-network density prior into an `inv` setup JSON (hybrid NN + focusing solver).

    python infer_prior.py --setup setup.json --checkpoint checkpoints/best_swin3d.pt --output setup_prior.json
    inv_app / InversionEngine::run_setup(setup_prior.json)      # solver rescales the prior to the data
                                                               # and focuses relative to it

Pipeline: scattered stations -> bin-average onto the TRAINING grid columns (neighbour fill for empty
columns) -> network -> trilinear resample to the setup grid -> ``setup["prior"]["values"]`` in
VoxelGrid order ``(ix*ny + iy)*nz + iz``.

Terrain: ``setup["topography"]`` (DEM node depths below the grid top) is fed as the 2nd channel.
The amplitude is rescaled for domains whose cell size differs from training (``amplitude_scale``);
very different depth/width aspect ratios still degrade the prior. Planar modelling only.
"""

import argparse
import json
import math

import numpy as np
import torch
import torch.nn.functional as F

from dataset import build_v5_input, infill_missing_obs
from evaluate import project_amplitude
from forward import TerrainGravityOperator
from model_swin3d import create_model


def scattered_to_columns_with_mask(x, y, v, nx, ny, x0, y0, dx, dy):
    """Bin-average stations into ``(nx, ny)`` columns; return ``(out, valid_mask)`` after neighbour infill."""
    s = np.zeros((nx, ny))
    n = np.zeros((nx, ny))
    for xi, yi, vi in zip(x, y, v):
        ix, iy = int(np.floor((xi - x0) / dx)), int(np.floor((yi - y0) / dy))
        if 0 <= ix < nx and 0 <= iy < ny and np.isfinite(vi):
            s[ix, iy] += vi
            n[ix, iy] += 1
    orig_known = n > 0
    if not orig_known.any():
        raise ValueError("no observation inside the model domain")
    out = np.where(orig_known, s / np.maximum(n, 1), 0.0)
    known = orig_known.copy()
    while not known.all():
        acc = np.zeros_like(out)
        cnt = np.zeros_like(out)
        for ax, sh in ((0, 1), (0, -1), (1, 1), (1, -1)):
            k = np.roll(known, sh, axis=ax)
            val = np.roll(out, sh, axis=ax)
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
    return out, orig_known.astype(np.float32)


def scattered_to_columns(x, y, v, nx, ny, x0, y0, dx, dy):
    """Bin-average stations into (nx, ny) columns; fill empty columns from neighbours. Returns (nx, ny) array."""
    out, _ = scattered_to_columns_with_mask(x, y, v, nx, ny, x0, y0, dx, dy)
    return out


def column_surface_depth(topo_nodes, nx, ny):
    """Mean of the 4 DEM corner depths per column (same as inv::column_surface_z). (nx, ny) metres."""
    if topo_nodes is None or len(topo_nodes) != (nx + 1) * (ny + 1):
        return np.zeros((nx, ny))
    t = np.asarray(topo_nodes, dtype=np.float64).reshape(ny + 1, nx + 1).T   # [ix, iy]
    return 0.25 * (t[:-1, :-1] + t[1:, :-1] + t[:-1, 1:] + t[1:, 1:])


def amplitude_scale(nx, ny, nz, dx, dy, dz, g):
    """Gravity of a geometrically similar model scales with length: rescale input observations by
    kTrainCell / L_eff = 1 / ((effective cell volume)^(1/3) / training cell) so physical contrasts
    (-2.67 air, -1.67 water, [-1.55, +1.85] lithology bands) match 50 m training units (mirrors neural_prior.hpp)."""
    eff = (nx * dx / g["nx"]) * (ny * dy / g["ny"]) * (nz * dz / g["nz"])
    return 1.0 / (eff ** (1.0 / 3.0) / g["cell"])


def predict_prior_and_uncertainty(setup, checkpoint, device="cpu", project=None, mc_samples=1):
    """Predict ``(prior_flat, sigma_flat_or_None)`` for setup in VoxelGrid order."""
    ck = torch.load(checkpoint, map_location=device)
    g = ck.get("grid", {"nx": 64, "ny": 64, "nz": 32, "cell": 50.0})
    arch = ck.get("arch", {})
    variant = arch.get("variant", "v1")
    is_v5 = variant == "v5"
    if project is None:
        project = variant not in ("v2", "v3", "v4", "v5")
    vd = setup["voxel_domain"]
    nx, ny, nz = vd["nx"], vd["ny"], vd["nz"]
    obs = setup["observations"]
    fdx, fdy = vd["nx"] * vd["dx"] / g["nx"], vd["ny"] * vd["dy"] / g["ny"]
    cols, valid_mask = scattered_to_columns_with_mask(
        obs["x"], obs["y"], obs["gz"], g["nx"], g["ny"], vd["x0"], vd["y0"], fdx, fdy,
    )
    t_orig = column_surface_depth(setup.get("topography"), nx, ny)
    t = F.interpolate(torch.from_numpy(t_orig)[None, None].float(), size=(g["nx"], g["ny"]), mode="bilinear",
                      align_corners=True)[0, 0].numpy()
    t_norm = np.clip(t / (nz * vd["dz"]), 0.0, 0.25)

    model = create_model(g["nx"], g["ny"], g["nz"], **arch)
    model.load_state_dict(ck["model_state_dict"])
    model.to(device).eval()

    with torch.no_grad():
        if is_v5:
            dxy_canon = math.sqrt((nx * vd["dx"] / g["nx"]) * (ny * vd["dy"] / g["ny"]))
            dz_canon = (nz * vd["dz"]) / g["nz"]
            scale_in = g["cell"] / max(dxy_canon, 1e-6)
            aspect = dz_canon / max(dxy_canon, 1e-6)
            clearance_m = 0.35
            if "z" in obs and len(obs["z"]) == len(obs["x"]):
                clr_list = []
                for xi, yi, zi in zip(obs["x"], obs["y"], obs["z"]):
                    ix = min(nx - 1, max(0, int(np.floor((xi - vd["x0"]) / vd["dx"]))))
                    iy = min(ny - 1, max(0, int(np.floor((yi - vd["y0"]) / vd["dy"]))))
                    clr_list.append(max(0.0, float(t_orig[ix, iy] - zi)))
                if clr_list:
                    clearance_m = float(np.median(clr_list))
            cols_t = torch.from_numpy(cols.astype(np.float32))[None, None].to(device)
            vm_t = torch.from_numpy(valid_mask.astype(np.float32))[None, None].to(device)
            if not bool((vm_t > 0.5).all()):
                cols_t = infill_missing_obs(cols_t, vm_t, iters=24)
            t_t = torch.from_numpy(t.astype(np.float32))[None].to(device)
            x = build_v5_input(
                cols_t * scale_in, t_t, float(nz * vd["dz"]),
                valid_mask=vm_t, aspect=aspect, clearance=clearance_m, dxy_canon=dxy_canon,
            )
        else:
            scale_in = amplitude_scale(nx, ny, nz, vd["dx"], vd["dy"], vd["dz"], g)
            cols_scaled = cols * scale_in
            x = torch.from_numpy(np.stack([cols_scaled, t_norm]).astype(np.float32))[None].to(device)

        sigma = None
        if mc_samples > 1 and is_v5 and hasattr(model, "enable_mc_dropout"):
            model.enable_mc_dropout(True)
            preds, vars_ = [], []
            for _ in range(mc_samples):
                p_k, aux_k = model(x, return_aux=True)
                preds.append(p_k)
                vars_.append(torch.exp(aux_k["log_var"]))
            model.enable_mc_dropout(False)
            pred_stack = torch.stack(preds, dim=0)
            pred = pred_stack.mean(dim=0)
            tot_var = torch.stack(vars_, dim=0).mean(dim=0) + pred_stack.var(dim=0, unbiased=False)
            sigma = torch.sqrt(tot_var.clamp_min(1e-8))
        elif is_v5:
            pred, aux = model(x, return_aux=True)
            sigma = torch.exp(0.5 * aux["log_var"])
        else:
            pred = model(x)

        if project:
            op = TerrainGravityOperator(g["nx"], g["ny"], g["nz"], g["cell"], g["cell"], g["cell"]).to(device)
            z_obs = torch.from_numpy(t_norm * (g["nz"] * g["cell"]) - 0.35).float()[None].to(device)
            pred = project_amplitude(pred, x[:, :1], z_obs, op)

        pred = F.interpolate(pred, size=(nz, nx, ny), mode="trilinear", align_corners=False)
        vol = pred[0, 0].cpu().numpy()
        prior_flat = np.transpose(vol, (1, 2, 0)).ravel(order="C")

        sigma_flat = None
        if sigma is not None:
            sig_var = F.interpolate(sigma * sigma, size=(nz, nx, ny), mode="trilinear", align_corners=False)
            svol = torch.sqrt(sig_var.clamp_min(1e-8))[0, 0].cpu().numpy()
            sigma_flat = np.transpose(svol, (1, 2, 0)).ravel(order="C")
    return prior_flat, sigma_flat


def predict_prior(setup, checkpoint, device="cpu", project=None):
    prior_flat, _ = predict_prior_and_uncertainty(setup, checkpoint, device=device, project=project)
    return prior_flat


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--setup", required=True)
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--device", default="cpu")
    ap.add_argument("--with-sigma", action="store_true")
    ap.add_argument("--mc-samples", type=int, default=1)
    args = ap.parse_args()
    with open(args.setup) as f:
        setup = json.load(f)
    prior, sigma = predict_prior_and_uncertainty(
        setup, args.checkpoint, args.device, mc_samples=args.mc_samples,
    )
    setup["prior"] = {"values": prior.astype(float).tolist()}
    if args.with_sigma and sigma is not None:
        setup["prior"]["sigma"] = sigma.astype(float).tolist()
    with open(args.output, "w") as f:
        json.dump(setup, f)
    print(f"Wrote prior ({prior.size} cells, peak {prior.max():.3f} / min {prior.min():.3f} g/cm^3) to {args.output}")


if __name__ == "__main__":
    main()
