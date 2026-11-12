"""Compact quantized + RLE sidecar serialization for 3D density volumes in the web dashboard."""

import base64
import json
import os

import numpy as np


_BG_BYTE = 128  # 0.0 density contrast maps to 128 in uint8 [1..255]; 0 is reserved as RLE escape


def _atomic_write(path, content):
    tmp_path = path + ".tmp"
    with open(tmp_path, "w") as f:
        f.write(content)
    os.replace(tmp_path, path)


def encode_vol3d_rle_b64(vol_zxy, vmax=None):
    """Quantize a (NZ, NX, NY) float volume to int8 [-127, 127], reorder (NZ, NY, NX), and RLE-compress.

    Reordering to (NZ, NY, NX) matches WebGL2 ``texImage3D(..., width=NX, height=NY, depth=NZ)``
    linear C memory order directly, avoiding JS transpose work on upload.
    """
    vol = np.asarray(vol_zxy, dtype=np.float32)
    scale = float(vmax) if vmax is not None else float(np.abs(vol).max())
    scale = max(1e-6, scale)
    vol_zyx = np.transpose(vol, (0, 2, 1))
    q = np.clip(np.round((vol_zyx / scale) * 127.0), -127, 127).astype(np.int16)
    arr = np.ascontiguousarray(q + _BG_BYTE, dtype=np.uint8).ravel()

    is_bg = arr == _BG_BYTE
    padded = np.empty(arr.size + 2, dtype=bool)
    padded[0] = False
    padded[-1] = False
    padded[1:-1] = is_bg
    starts = np.flatnonzero(~padded[:-1] & padded[1:])
    ends = np.flatnonzero(padded[:-1] & ~padded[1:])

    out = bytearray()
    pos = 0
    for s, e in zip(starts.tolist(), ends.tolist()):
        if s > pos:
            out.extend(arr[pos:s].tobytes())
        rem = e - s
        while rem > 255:
            out.append(0)
            out.append(255)
            rem -= 255
        if rem > 0:
            out.append(0)
            out.append(rem)
        pos = e
    if pos < arr.size:
        out.extend(arr[pos:].tobytes())

    return base64.b64encode(bytes(out)).decode("ascii"), round(scale, 4)


def decode_vol3d_rle_b64(b64_str, shape_zxy, vmax):
    """Decode an ``encode_vol3d_rle_b64`` payload back into a (NZ, NX, NY) float32 array."""
    nz, nx, ny = shape_zxy
    total = nz * nx * ny
    raw = base64.b64decode(b64_str)
    out = np.empty(total, dtype=np.uint8)
    p = 0
    idx = 0
    n_raw = len(raw)
    while p < n_raw and idx < total:
        v = raw[p]
        p += 1
        if v == 0:
            run = raw[p]
            p += 1
            end = min(total, idx + run)
            out[idx:end] = _BG_BYTE
            idx = end
        else:
            out[idx] = v
            idx += 1
    if idx < total:
        out[idx:total] = _BG_BYTE
    vol_zyx = (out.reshape(nz, ny, nx).astype(np.float32) - float(_BG_BYTE)) / 127.0 * float(vmax)
    return np.transpose(vol_zyx, (0, 2, 1)).astype(np.float32)


def encode_terrain_u16_b64(terr_xy, depth_total_m):
    """Encode (NX, NY) terrain depth [m] into (NY, NX) little-endian uint16 normalized to [0, depth_total_m]."""
    terr_yx = np.transpose(np.asarray(terr_xy, dtype=np.float32), (1, 0))
    frac = np.clip(terr_yx / max(1e-6, float(depth_total_m)), 0.0, 1.0)
    u16 = np.ascontiguousarray(np.round(frac * 65535.0), dtype="<u2")
    return base64.b64encode(u16.tobytes()).decode("ascii")


def encode_field2d_i16_b64(field_xy, vmax=None):
    """Encode (NX, NY) surface gravity field [mGal] into (NY, NX) little-endian int16 normalized to vmax."""
    field_yx = np.transpose(np.asarray(field_xy, dtype=np.float32), (1, 0))
    scale = float(vmax) if vmax is not None else float(np.abs(field_yx).max())
    scale = max(1e-4, scale)
    q = np.clip(np.round((field_yx / scale) * 32767.0), -32767, 32767)
    i16 = np.ascontiguousarray(q, dtype="<i2")
    return base64.b64encode(i16.tobytes()).decode("ascii"), round(scale, 4)


def decode_field2d_i16_b64(b64_str, shape_xy, vmax):
    """Decode an ``encode_field2d_i16_b64`` payload back into a (NX, NY) float32 array."""
    nx, ny = shape_xy
    i16 = np.frombuffer(base64.b64decode(b64_str), dtype="<i2").reshape(ny, nx)
    field_yx = i16.astype(np.float32) / 32767.0 * float(vmax)
    return np.transpose(field_yx, (1, 0)).astype(np.float32)


def write_vol3d_sidecars(
    save_dir,
    epoch,
    nx,
    ny,
    nz,
    cell_m,
    vtrue_vols,
    vpred_vols,
    terrain_m_xy,
    sample_meta_list,
    is_best=False,
    stride=10,
    write_meta=True,
    obs_fields=None,
    pred_fields=None,
):
    """Write static 3D truth/terrain metadata + per-epoch 3D prediction sidecar files (.json + .js)."""
    os.makedirs(save_dir, exist_ok=True)
    depth_total_m = float(nz) * float(cell_m)
    n_samples = len(sample_meta_list)

    if write_meta or not os.path.exists(os.path.join(save_dir, "vol3d_meta.js")):
        meta_samples = []
        for s_idx in range(n_samples):
            sm = sample_meta_list[s_idx]
            vmax_t = max(0.1, float(np.abs(vtrue_vols[s_idx]).max()))
            t_b64, vmax_t = encode_vol3d_rle_b64(vtrue_vols[s_idx], vmax=vmax_t)
            terr_b64 = encode_terrain_u16_b64(terrain_m_xy[s_idx], depth_total_m)
            item = {
                "id": int(sm.get("id", s_idx)),
                "iy": int(sm.get("iy", 0)),
                "y_km": sm.get("y_km"),
                "vmax": vmax_t,
                "truth_b64": t_b64,
                "terrain_b64": terr_b64,
            }
            if obs_fields is not None and s_idx < len(obs_fields) and obs_fields[s_idx] is not None:
                obs_b64, obs_vmax = encode_field2d_i16_b64(obs_fields[s_idx])
                item["obs_field_b64"] = obs_b64
                item["obs_field_vmax"] = obs_vmax
            if "tag" in sm:
                item["tag"] = sm["tag"]
            meta_samples.append(item)
        meta_payload = {"nx": int(nx), "ny": int(ny), "nz": int(nz), "cell": float(cell_m), "samples": meta_samples}
        _atomic_write(os.path.join(save_dir, "vol3d_meta.json"), json.dumps(meta_payload))
        _atomic_write(
            os.path.join(save_dir, "vol3d_meta.js"),
            "window.VOL3D_META = " + json.dumps(meta_payload) + ";\n",
        )

    pred_b64_list = []
    vmax_pred_list = []
    pred_field_b64_list = []
    pred_field_vmax_list = []
    for s_idx in range(n_samples):
        p_vol = np.asarray(vpred_vols[s_idx], dtype=np.float32)
        vmax_s = max(0.1, float(sample_meta_list[s_idx].get("vmax", 0.6)), float(np.abs(p_vol).max()))
        p_b64, vmax_p = encode_vol3d_rle_b64(p_vol, vmax=vmax_s)
        pred_b64_list.append(p_b64)
        vmax_pred_list.append(vmax_p)
        if pred_fields is not None and s_idx < len(pred_fields) and pred_fields[s_idx] is not None:
            pf_b64, pf_vmax = encode_field2d_i16_b64(pred_fields[s_idx])
            pred_field_b64_list.append(pf_b64)
            pred_field_vmax_list.append(pf_vmax)

    ep_int = int(epoch)
    ep_payload = {"epoch": ep_int, "vmax_preds": vmax_pred_list, "pred_b64": pred_b64_list}
    if len(pred_field_b64_list) == n_samples:
        ep_payload["pred_field_b64"] = pred_field_b64_list
        ep_payload["pred_field_vmax"] = pred_field_vmax_list
    ep_json = json.dumps(ep_payload)
    ep_js = (
        f"window.VOL3D_EPOCHS = window.VOL3D_EPOCHS || {{}};\n"
        f"window.VOL3D_EPOCHS[{ep_int}] = {ep_json};\n"
    )

    _atomic_write(os.path.join(save_dir, "vol3d_latest.json"), ep_json)
    _atomic_write(
        os.path.join(save_dir, "vol3d_latest.js"),
        ep_js + f"window.VOL3D_LATEST_EPOCH = {ep_int};\n",
    )

    if is_best:
        _atomic_write(os.path.join(save_dir, "vol3d_best.json"), ep_json)
        _atomic_write(
            os.path.join(save_dir, "vol3d_best.js"),
            ep_js + f"window.VOL3D_BEST_EPOCH = {ep_int};\n",
        )

    idx_path = os.path.join(save_dir, "vol3d_index.json")
    index_data = {"best": None, "latest": ep_int, "epochs": []}
    if os.path.exists(idx_path):
        try:
            with open(idx_path) as f:
                loaded = json.load(f)
                if isinstance(loaded, dict):
                    index_data.update(loaded)
        except Exception:
            pass
    epochs_saved = set(int(e) for e in index_data.get("epochs", []) if e is not None)

    if stride and stride > 0 and (ep_int % int(stride) == 0 or ep_int == 1):
        ep_tag = f"vol3d_ep_{ep_int:04d}"
        _atomic_write(os.path.join(save_dir, f"{ep_tag}.json"), ep_json)
        _atomic_write(os.path.join(save_dir, f"{ep_tag}.js"), ep_js)
        epochs_saved.add(ep_int)

    index_data["latest"] = ep_int
    if is_best:
        index_data["best"] = ep_int
    index_data["epochs"] = sorted(epochs_saved)
    _atomic_write(os.path.join(save_dir, "vol3d_index.json"), json.dumps(index_data))
    _atomic_write(
        os.path.join(save_dir, "vol3d_index.js"),
        "window.VOL3D_INDEX = " + json.dumps(index_data) + ";\n",
    )
    return index_data
