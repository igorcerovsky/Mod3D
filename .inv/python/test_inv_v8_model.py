"""Unit, unrolled-physics, sharpness-loss, V7 warm-start, ONNX/CoreML, and CPU smoke tests for V8."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import torch

from evaluate import _infer_eval_model_cfg
from export_onnx import export_checkpoint_to_onnx
from forward import TerrainGravityOperator
from losses import CompoundInversionLoss
from model_swin3d import (
    PotentialFieldTransformer3DV7,
    PotentialFieldTransformer3DV8,
    V7_SIZE_PRESETS,
    V8_SIZE_PRESETS,
    create_model,
)
from test_inv_v7_model import _corr_3d, _make_probe_input


def test_v8_presets_unrolled_aux_and_shapes():
    assert set(V8_SIZE_PRESETS.keys()) == {"slim", "base", "heavy"}
    x6 = _make_probe_input(batch=2, channels=6, nx=64, ny=64)
    for sz in ("slim", "base", "heavy"):
        cfg = V8_SIZE_PRESETS[sz]
        assert cfg["variant"] == "v8" and cfg["n_iters"] == 3
        m = create_model(64, 64, 32, **cfg).eval()
        assert isinstance(m, PotentialFieldTransformer3DV8)
        assert isinstance(m, PotentialFieldTransformer3DV7)
        with torch.no_grad():
            pred, aux = m(x6, return_aux=True)
            pred_u, lv_u = m.forward_uncertainty(x6)
        assert pred.shape == (2, 1, 32, 64, 64)
        assert lv_u.shape == (2, 1, 32, 64, 64)
        assert torch.isfinite(pred).all() and torch.isfinite(lv_u).all()
        assert torch.allclose(pred, pred_u, atol=1e-6)
        assert float(pred[:, :, :3].abs().max()) == 0.0
        assert aux["is_v7"] is True and aux["is_v8"] is True
        assert len(aux["rho_iter_preds"]) == 2
        for rk in aux["rho_iter_preds"]:
            assert rk.shape == (2, 1, 32, 64, 64)
            assert float(rk[:, :, :3].abs().max()) == 0.0
        is_v7, is_v6, in_ch, _ = _infer_eval_model_cfg(m)
        assert is_v7 is True and is_v6 is True and in_ch == 6


def test_v8_warm_start_from_v7_exact_and_gradients():
    torch.manual_seed(71)
    x6 = _make_probe_input(batch=2, channels=6, nx=64, ny=64)

    m7 = create_model(64, 64, 32, **V7_SIZE_PRESETS["base"]).eval()
    v7_sd = {k: v.clone() for k, v in m7.state_dict().items()}

    m8 = create_model(64, 64, 32, **V8_SIZE_PRESETS["base"]).eval()
    m8.load_state_dict(v7_sd, strict=True)

    with torch.no_grad():
        p7, lv7 = m7.forward_uncertainty(x6)
        p8, lv8 = m8.forward_uncertainty(x6)
    corr = _corr_3d(p7, p8)
    lv_corr = _corr_3d(lv7, lv8)
    assert corr > 0.999, f"Expected V7 -> V8 step-0 correlation > 0.999, got {corr:.6f}"
    assert lv_corr > 0.999, f"Expected V7 -> V8 step-0 log_var correlation > 0.999, got {lv_corr:.6f}"
    assert float((p7 - p8).abs().max()) < 1e-5
    assert float((lv7 - lv8).abs().max()) < 0.02

    # Unrolled gates & iter_scale receive gradients on backward pass
    m8.train()
    pred, aux = m8(x6, return_aux=True)
    loss = (pred ** 2).mean() + sum((rk ** 2).mean() for rk in aux["rho_iter_preds"])
    loss.backward()
    for name in ("res_vox_gates.0", "iter_step_gates.0", "iter_step_gates.1", "iter_scale"):
        p = dict(m8.named_parameters())[name]
        assert p.grad is not None and torch.isfinite(p.grad).all()
        assert float(p.grad.abs().sum()) > 0.0, f"Expected non-zero gradient on {name}"


def test_v8_sharpness_losses_and_iter_supervision():
    torch.manual_seed(79)
    B, D, H, W = 2, 32, 64, 64
    m_true = torch.zeros(B, 1, D, H, W)
    unit_id = torch.zeros(B, 1, D, H, W, dtype=torch.int32)
    m_true[0, 0, 4:10, 24:36, 24:36] = 1.10
    unit_id[0, 0, 4:10, 24:36, 24:36] = 1
    m_true[1, 0, 12:18, 20:32, 20:32] = -0.90
    unit_id[1, 0, 12:18, 20:32, 20:32] = 1

    op = TerrainGravityOperator(H, W, D, 50.0, 50.0, 50.0, h_min=-220.0)
    z_obs = torch.full((B, H, W), -0.35)
    d_clean = op(m_true, z_obs)

    crit = CompoundInversionLoss(
        lambda_data=0.80, lambda_ms=0.05, lambda_cls=0.0, lambda_coarse=0.0,
        lambda_rho_init=0.0, lambda_nll=0.0, lambda_tier=0.0,
        forward_op=op, use_v6_losses=True,
    )
    loss_exact, stats_exact = crit(
        m_true.clone(), m_true, d_clean=d_clean, z_obs=z_obs,
        aux={"is_v8": True, "rho_iter_preds": (m_true.clone(), m_true.clone()), "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert float(loss_exact) < 1e-6
    assert stats_exact["loss_peak_amp"] < 1e-6
    assert stats_exact["loss_focus"] < 1e-6
    assert stats_exact["loss_iter_data"] < 1e-6

    # Smeared & attenuated prediction triggers positive peak_amp, focus, and iter_data losses
    m_smeared = (m_true * 0.65).clone()
    m_smeared[0, 0, 4:10, 22:24, 24:36] = 0.25  # exterior skirt
    m_smeared.requires_grad_(True)
    rk0 = (m_true * 0.55).detach().requires_grad_(True)
    rk1 = (m_true * 0.60).detach().requires_grad_(True)
    loss_sm, stats_sm = crit(
        m_smeared, m_true, d_clean=d_clean, z_obs=z_obs,
        aux={"is_v8": True, "rho_iter_preds": (rk0, rk1), "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert stats_sm["loss_peak_amp"] > 0.20
    assert stats_sm["loss_focus"] > 0.01
    assert stats_sm["loss_iter_data"] > 0.05
    loss_sm.backward()
    for t in (m_smeared, rk0, rk1):
        assert t.grad is not None and torch.isfinite(t.grad).all() and float(t.grad.abs().sum()) > 0.0


def test_v8_onnx_export_coreml_single_partition():
    torch.manual_seed(83)
    model = create_model(64, 64, 32, **V8_SIZE_PRESETS["slim"]).eval()
    with tempfile.TemporaryDirectory(prefix="v8_onnx_") as td:
        ckpt_path = Path(td) / "v8_slim.pt"
        onnx_path = Path(td) / "v8_slim.onnx"
        torch.save({
            "model_state_dict": model.state_dict(),
            "arch": dict(V8_SIZE_PRESETS["slim"]),
            "grid": {"nx": 64, "ny": 64, "nz": 32, "cell": 50.0},
        }, ckpt_path)
        export_checkpoint_to_onnx(str(ckpt_path), str(onnx_path), opset=17, verify=True, verify_mps=False)
        assert onnx_path.is_file() and onnx_path.stat().st_size > 1_000_000


def _run_v8_train_cli(td: str, extra_args: list[str]) -> tuple[str, dict, dict]:
    inv_py_dir = Path(__file__).resolve().parent
    cmd = [
        sys.executable, "-u", str(inv_py_dir / "train.py"),
        "--variant", "v8",
        "--model-size", "slim",
        "--epochs", "1",
        "--steps", "2",
        "--batch-size", "2",
        "--n-val", "4",
        "--device", "cpu",
        "--save-dir", td,
        *extra_args,
    ]
    env = dict(os.environ)
    env["PYTHONPATH"] = str(inv_py_dir) + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    proc = subprocess.run(cmd, env=env, capture_output=True, text=True, check=True)
    out = proc.stdout + proc.stderr
    assert "Architecture: v8 (slim)" in out
    assert "Best val loss" in out
    assert (Path(td) / "best_swin3d.pt").is_file()
    assert (Path(td) / "last_swin3d.pt").is_file()
    ck = torch.load(Path(td) / "best_swin3d.pt", map_location="cpu")
    assert ck["arch"]["variant"] == "v8"
    assert ck["arch"]["n_iters"] == 3
    with open(Path(td) / "training_history.json", encoding="utf-8") as f:
        hist = json.load(f)
    assert len(hist["epochs"]) == 1 and np.isfinite(hist["epochs"][0]["val_loss"])
    return out, ck, hist


def test_v8_train_cli_cpu_smoke():
    with tempfile.TemporaryDirectory(prefix="v8_cpu_smoke_") as td:
        _run_v8_train_cli(td, ["--num-workers", "0"])


def test_v8_v3_loss_flag_unit():
    torch.manual_seed(91)
    B, D, H, W = 2, 16, 32, 32
    m_true = torch.zeros(B, 1, D, H, W)
    m_true[0, 0, 3:7, 10:18, 10:18] = 1.0
    m_true[1, 0, 5:9, 12:20, 12:20] = -1.2
    m_pred = m_true * 0.7 + 0.05 * torch.randn_like(m_true)
    rho_init = m_true * 0.5
    log_var = torch.full_like(m_true, -2.0)
    tier_preds = (
        torch.nn.functional.avg_pool3d(m_pred[:, :, :4], 4),
        torch.nn.functional.avg_pool3d(m_pred[:, :, 4:10], 2),
        m_pred[:, :, 10:],
    )
    op = TerrainGravityOperator(H, W, D, 50.0, 50.0, 50.0, h_min=-85.0)
    z_obs = torch.full((B, H, W), -0.35)
    d_clean = op(m_true, z_obs)

    crit_v3 = CompoundInversionLoss(
        lambda_data=0.80, lambda_ms=0.05, lambda_cls=0.0, lambda_coarse=0.0,
        lambda_rho_init=0.10, lambda_nll=0.06, lambda_tier=0.20,
        forward_op=op, use_v6_losses=False,
    )
    aux_v3_flag = {
        "is_v7": True, "is_v8": True, "use_v3_loss": True,
        "rho_init": rho_init, "log_var": log_var, "tier_preds": tier_preds,
        "rho_iter_preds": (m_pred * 0.8,),
    }
    aux_v6_flag = {
        "is_v7": True, "is_v8": True,
        "rho_init": rho_init, "log_var": log_var, "tier_preds": tier_preds,
        "rho_iter_preds": (m_pred * 0.8,),
    }
    aux_plain_v3 = {
        "is_v8": True, "log_var": log_var, "tier_preds": tier_preds,
        "rho_iter_preds": (m_pred * 0.8,),
    }

    _, st_v3_flag = crit_v3(m_pred, m_true, d_clean=d_clean, z_obs=z_obs, aux=aux_v3_flag)
    _, st_plain_v3 = crit_v3(m_pred, m_true, d_clean=d_clean, z_obs=z_obs, aux=aux_plain_v3)
    _, st_v6_flag = crit_v3(m_pred, m_true, d_clean=d_clean, z_obs=z_obs, aux=aux_v6_flag)

    for k in ("loss_sup", "loss_data", "loss_nll", "loss_unit_iou", "loss_tier"):
        assert abs(st_v3_flag[k] - st_plain_v3[k]) < 1e-6, f"Mismatch on {k} vs V3 baseline"
    assert st_v3_flag["loss_rho_init"] > 0.0
    assert abs(st_v3_flag["loss_unit_iou"] - st_v6_flag["loss_unit_iou"]) > 1e-4


def test_v8_v3_data_and_v3_loss_smoke():
    with tempfile.TemporaryDirectory(prefix="v8_v3mode_smoke_") as td:
        out, _, hist = _run_v8_train_cli(td, ["--v3-data", "--v3-loss"])
        assert "Zero predictor (std):" in out
        assert "val_gc_loss" not in hist["epochs"][0]


if __name__ == "__main__":
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok  {name}")
    print(f"{len(tests)} tests passed")
