"""Unit, depth-gradient, coarse-occupancy, and CPU CLI smoke tests for V9."""

import json
import math
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
    PotentialFieldTransformer3DV9,
    V9_SIZE_PRESETS,
    V9_1_SIZE_PRESETS,
    create_model,
)
from test_inv_v7_model import _make_probe_input


def test_v9_presets_shapes_and_pruned_aux():
    assert set(V9_SIZE_PRESETS.keys()) == {"slim", "base", "heavy"}
    x6 = _make_probe_input(batch=2, channels=6, nx=64, ny=64)
    for sz in ("slim", "base", "heavy"):
        cfg = V9_SIZE_PRESETS[sz]
        assert cfg["variant"] == "v9" and cfg["in_channels"] == 6
        m = create_model(64, 64, 32, **cfg).eval()
        assert isinstance(m, PotentialFieldTransformer3DV9)
        with torch.no_grad():
            pred, aux = m(x6, return_aux=True)
            pred_u, lv_u = m.forward_uncertainty(x6)
        assert pred.shape == (2, 1, 32, 64, 64)
        assert lv_u.shape == (2, 1, 32, 64, 64)
        assert torch.isfinite(pred).all() and torch.isfinite(lv_u).all()
        assert torch.allclose(pred, pred_u, atol=1e-6)
        assert float(pred[:, :, :3].abs().max()) == 0.0
        assert aux["is_v9"] is True
        assert "tier_preds" not in aux and "rho_iter_preds" not in aux
        assert aux["coarse_pred"].shape == (2, 1, 16, 32, 32)
        assert float(aux["coarse_pred"].abs().mean()) < 0.35
        assert aux["rho_init"].shape == (2, 1, 32, 64, 64)
        is_v7, is_v6, in_ch, _ = _infer_eval_model_cfg(m)
        assert is_v7 is True and is_v6 is True and in_ch == 6


def test_v9_gradients_depth_and_physics_feedback():
    torch.manual_seed(95)
    B, D, H, W = 2, 32, 64, 64
    x6 = _make_probe_input(batch=B, channels=6, nx=H, ny=W)
    x6[:, 5] = 0.0  # raw mode exercises un-detrended residual path too
    m_true = torch.zeros(B, 1, D, H, W)
    m_true[0, 0, 8:14, 24:36, 24:36] = 0.9
    m_true[1, 0, 10:16, 20:30, 20:30] = -1.1
    op = TerrainGravityOperator(H, W, D, 50.0, 50.0, 50.0, h_min=-220.0)
    z_obs = torch.full((B, H, W), -0.35)
    d_clean = op(m_true, z_obs)

    m = create_model(H, W, D, **V9_SIZE_PRESETS["slim"]).train()
    crit = CompoundInversionLoss(
        lambda_data=0.50, lambda_ms=0.05, lambda_cls=0.25,
        depth_boost=0.5, forward_op=op, use_v6_losses=False,
    )
    pred, aux = m(x6, return_aux=True)
    loss, stats = crit(pred, m_true, d_clean=d_clean, z_obs=z_obs, aux=aux)
    assert stats["loss_rho_init"] > 0.0
    loss.backward()

    named = dict(m.named_parameters())
    for p_name in (
        "skip_mid.z_gate", "skip_mid.d_bias.weight",
        "skip_vox.z_gate", "skip_vox.d_bias.weight",
        "cls_depth_prior.0.weight", "cls_depth_prior.2.weight",
        "coarse_occ_head.weight", "res_mid_gate", "res_vox_gate", "bp_z_beta",
    ):
        p = named[p_name]
        assert p.grad is not None and torch.isfinite(p.grad).all(), f"Invalid grad on {p_name}"
        assert float(p.grad.abs().sum()) > 0.0, f"Zero grad on {p_name}"


def test_v9_onnx_export_coreml_single_partition():
    torch.manual_seed(97)
    model = create_model(64, 64, 32, **V9_SIZE_PRESETS["slim"]).eval()
    with tempfile.TemporaryDirectory(prefix="v9_onnx_") as td:
        ckpt_path = Path(td) / "v9_slim.pt"
        onnx_path = Path(td) / "v9_slim.onnx"
        torch.save({
            "model_state_dict": model.state_dict(),
            "arch": dict(V9_SIZE_PRESETS["slim"]),
            "grid": {"nx": 64, "ny": 64, "nz": 32, "cell": 50.0},
        }, ckpt_path)
        export_checkpoint_to_onnx(str(ckpt_path), str(onnx_path), opset=17, verify=True, verify_mps=False)
        assert onnx_path.is_file() and onnx_path.stat().st_size > 1_000_000


def test_v9_train_cli_v3data_cpu_smoke():
    inv_py_dir = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="v9_cpu_smoke_") as td:
        cmd = [
            sys.executable, "-u", str(inv_py_dir / "train.py"),
            "--variant", "v9",
            "--model-size", "slim",
            "--v3-data",
            "--epochs", "1",
            "--steps", "2",
            "--batch-size", "2",
            "--n-val", "4",
            "--num-workers", "0",
            "--device", "cpu",
            "--save-dir", td,
        ]
        env = dict(os.environ)
        env["PYTHONPATH"] = str(inv_py_dir) + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
        proc = subprocess.run(cmd, env=env, capture_output=True, text=True, check=True)
        out = proc.stdout + proc.stderr
        assert "Architecture: v9 (slim)" in out
        assert "Best val loss" in out
        assert (Path(td) / "best_swin3d.pt").is_file()
        assert (Path(td) / "last_swin3d.pt").is_file()
        ck = torch.load(Path(td) / "best_swin3d.pt", map_location="cpu")
        assert ck["arch"]["variant"] == "v9"
        with open(Path(td) / "training_history.json", encoding="utf-8") as f:
            hist = json.load(f)
        assert len(hist["epochs"]) == 1 and np.isfinite(hist["epochs"][0]["val_loss"])


def test_v9_1_presets_and_coherence_loss_properties():
    assert set(V9_1_SIZE_PRESETS.keys()) == {"slim", "base", "heavy"}
    x6 = _make_probe_input(batch=2, channels=6, nx=64, ny=64)
    m = create_model(64, 64, 32, **V9_1_SIZE_PRESETS["slim"]).eval()
    assert isinstance(m, PotentialFieldTransformer3DV9)
    assert m.variant == "v9.1"
    with torch.no_grad():
        pred, aux = m(x6, return_aux=True)
    assert pred.shape == (2, 1, 32, 64, 64)
    assert aux["is_v9"] is True and aux["is_v9_1"] is True

    # Construct a composite scene with abutting coal (unit 1, class 3) + ore (unit 2, class 6)
    B, D, H, W = 1, 16, 24, 24
    m_true = torch.zeros(B, 1, D, H, W)
    unit_id = torch.zeros(B, 1, D, H, W, dtype=torch.int32)
    m_true[0, 0, 4:10, 6:12, 8:14] = -1.20   # light/coal (class 3)
    unit_id[0, 0, 4:10, 6:12, 8:14] = 1
    m_true[0, 0, 4:10, 12:18, 8:14] = 1.40   # ore (class 6), abutting unit 1 along x=11/12
    unit_id[0, 0, 4:10, 12:18, 8:14] = 2

    cls_tgt = CompoundInversionLoss._lithology_class_targets(m_true, num_classes=7, is_v6=False)
    p_exact = torch.nn.functional.one_hot(cls_tgt, num_classes=7).permute(0, 4, 1, 2, 3).float()
    logits_exact = torch.log(p_exact.clamp_min(1e-7))

    crit = CompoundInversionLoss(lambda_data=0.0, lambda_ms=0.0, lambda_cls=0.0, lambda_nll=0.0, use_v6_losses=False)

    # 1. Zero-at-exact even on abutting composite coal+ore body
    _, st_exact = crit(
        m_true.clone(), m_true,
        aux={"is_v9": True, "is_v9_1": True, "probs": p_exact, "logits": logits_exact, "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert st_exact["loss_coherence"] < 1e-6, f"Expected ~0 coherence loss at exact composite match, got {st_exact['loss_coherence']}"

    # 2. V9 (not v9.1) keeps coherence disabled by default
    p_bad = p_exact.clone()
    p_bad[0, :, 6:8, 14:16, 10:12] = 0.0
    p_bad[0, 3, 6:8, 14:16, 10:12] = 1.0  # coal speckle inside ore body
    _, st_v9_off = crit(
        m_true.clone(), m_true,
        aux={"is_v9": True, "is_v9_1": False, "probs": p_bad, "logits": logits_exact, "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert st_v9_off["loss_coherence"] == 0.0

    # 3. Incompatible speckle (coal inside ore) penalized MUCH harder than adjacent same-sign speckle (mafic inside ore)
    p_adj = p_exact.clone()
    p_adj[0, :, 6:8, 14:16, 10:12] = 0.0
    p_adj[0, 5, 6:8, 14:16, 10:12] = 1.0  # mafic speckle inside ore body (same sign, adjacent band)

    p_bad_req = p_bad.detach().clone().requires_grad_(True)
    loss_incompat, st_incompat = crit(
        m_true.clone(), m_true,
        aux={"is_v9": True, "is_v9_1": True, "probs": p_bad_req, "logits": logits_exact, "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    _, st_adj = crit(
        m_true.clone(), m_true,
        aux={"is_v9": True, "is_v9_1": True, "probs": p_adj, "logits": logits_exact, "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert st_incompat["loss_coherence"] > st_adj["loss_coherence"] * 1.5, (
        f"Expected incompatible coal-in-ore ({st_incompat['loss_coherence']:.4f}) >> "
        f"adjacent mafic-in-ore ({st_adj['loss_coherence']:.4f})"
    )
    loss_incompat.backward()
    assert p_bad_req.grad is not None and torch.isfinite(p_bad_req.grad).all()
    assert float(p_bad_req.grad.abs().sum()) > 0.0
    w_sum = sum(v for k, v in st_incompat.items() if k.startswith("wloss_"))
    assert abs(w_sum - st_incompat["loss_total"]) < 1e-5, f"wloss sum {w_sum} != total {st_incompat['loss_total']}"

    # 4. Empty core prediction inside a true body incurs coherence penalty & non-vanishing gradient pushing p_empty down
    p_empty = p_exact.clone()
    p_empty[0, :, 4:10, 12:18, 8:14] = 0.0
    p_empty[0, 0, 4:10, 12:18, 8:14] = 1.0  # entire ore unit predicted empty (host rock)
    p_empty_req = p_empty.detach().clone().requires_grad_(True)
    loss_emp, st_emp = crit(
        m_true.clone(), m_true,
        aux={"is_v9": True, "is_v9_1": True, "probs": p_empty_req, "logits": logits_exact, "mask": torch.ones_like(m_true)},
        unit_id=unit_id,
    )
    assert st_emp["loss_coherence"] > 0.08, f"Expected non-trivial coherence penalty on empty body core, got {st_emp['loss_coherence']}"
    loss_emp.backward()
    core_p0_grad = float(p_empty_req.grad[0, 0, 6:8, 14:16, 10:12].mean())
    assert core_p0_grad > 0.0, f"Expected positive grad pushing p_empty down in core, got {core_p0_grad}"


def test_v9_1_train_cli_v3data_cpu_smoke():
    inv_py_dir = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="v9_1_cpu_smoke_") as td:
        cmd = [
            sys.executable, "-u", str(inv_py_dir / "train.py"),
            "--variant", "v9.1",
            "--model-size", "slim",
            "--v3-data",
            "--epochs", "1",
            "--steps", "2",
            "--batch-size", "2",
            "--n-val", "4",
            "--num-workers", "0",
            "--device", "cpu",
            "--save-dir", td,
        ]
        env = dict(os.environ)
        env["PYTHONPATH"] = str(inv_py_dir) + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
        proc = subprocess.run(cmd, env=env, capture_output=True, text=True, check=True)
        out = proc.stdout + proc.stderr
        assert "Architecture: v9.1 (slim)" in out
        assert "coherence" in out
        assert (Path(td) / "best_swin3d.pt").is_file()
        ck = torch.load(Path(td) / "best_swin3d.pt", map_location="cpu")
        assert ck["arch"]["variant"] == "v9.1"
        with open(Path(td) / "training_history.json", encoding="utf-8") as f:
            hist = json.load(f)
        ep0 = hist["epochs"][0]
        assert ep0.get("train_coherence", 0.0) > 0.0 and ep0.get("val_coherence", 0.0) > 0.0
        for cal_k in ("bd_sig_err_ratio", "aurc", "unc_spearman", "fg_1sig_cov"):
            assert cal_k in ep0 and math.isfinite(ep0[cal_k]), f"Expected finite {cal_k} in history, got {ep0.get(cal_k)}"
        tr_wsum = sum(v for k, v in ep0.items() if k.startswith("train_wloss_"))
        va_wsum = sum(v for k, v in ep0.items() if k.startswith("val_wloss_"))
        assert ep0.get("train_wloss_coherence", 0.0) > 0.0 and ep0.get("val_wloss_coherence", 0.0) > 0.0
        assert abs(tr_wsum - ep0["train_loss"]) < 1e-4, f"train wloss sum {tr_wsum} != {ep0['train_loss']}"
        assert abs(va_wsum - ep0["val_loss"]) < 1e-4, f"val wloss sum {va_wsum} != {ep0['val_loss']}"
        dash_html = (Path(td) / "dashboard.html").read_text(encoding="utf-8")
        assert "val_coherence" in dash_html and "train_coherence" in dash_html
        assert "svgLossComp" in dash_html and "cardLossComp" in dash_html


if __name__ == "__main__":
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok  {name}")
    print(f"{len(tests)} tests passed")
