# Mod3D `.inv` V8 Plan: Unrolled Physics Loop, Sharpness-Preserving Loss & `heavy` Capacity

**Date:** 2026-11-17  
**Baseline:** `v7 (base)` (`dbf6138`, Epoch 112: `val_loss=1.2633`, `iou=0.5436`, `val_gc_iou=0.4688`, `val_hl_iou=0.6429`, `amp_ratio=0.6711`, `centroid_dz=51.9 m`, `data_rel_misfit=0.0958`, `train_loss=1.2936`).  
**Scope Constraint:** Keeps the exact **6-channel input** `(1, 6, 64, 64)` (`[gz, terr_norm, valid_mask, log_aspect, clearance, detrend_mode]`) and **Option I output** `(1, 1, 32, 64, 64)` (`density_volume`, `log_var_volume`) — **no extra physics channels** (`g_zz`, boreholes, etc. deferred to future revisions). Preserves **CoreML MLProgram single-partition static export** (`0` CPU fallbacks, FP16-safe).

---

## 1. Diagnosis of V7 Bottlenecks
1. **Under-iterated Stage-2 physics feedback (`data_rel_misfit = 9.6%`)**: V7 checks the full-resolution gravity residual only **once** (`rho_init → g_init → dec_vox2`).
2. **Posterior-mean blurring (`amp_ratio = 0.671`, `val_gc_iou = 0.469`)**: Single-pass L1/NLL hedges depth ambiguity by smearing compact ground-survey bodies wider at ~67% peak density.
3. **Capacity headroom (`train_loss = 1.294 > val_loss = 1.263`)**: `v7 base` (`2.86M` params) is underfitting rather than overfitting, and the 3-tier slab hierarchy (`41,984` voxels vs `131,072` in V6) leaves ~3× compute headroom for deeper/wider Stage-2 refinement.

---

## 2. Core V8 Upgrades

### [V8-1] Weight-Tied Unrolled Stage-2 Gauss-Newton Physics Loop (`n_iters = 3`)
* **Where**: `.inv/python/model_swin3d.py` (`PotentialFieldTransformer3DV8`, subclassing/extending `PotentialFieldTransformer3DV7`).
* **Mechanism**:
  1. Keep V7's Stage-0 3D Swin/Axial transformer + Stage-1 coarse `(16, 32, 32)` feedback + Option A 3-tier slab hierarchy (`64×64×8`, `32×32×8`, `16×16×4` = `41,984` voxels).
  2. Replace the single Stage-2 physics correction (`rho_init → dec_vox2`) with a **weight-tied `n_iters = 3` unrolled residual loop** on the 3 tiers:
     - Maintain latent slab states `hv_tiers[0..2]` and current density estimate `rho_tiers^(k)` (`k = 0..2`, starting from `rho_init_tiers`).
     - At each iteration `k`:
       1. Evaluate tier-native `OddPyramidGravity`: `g_k = g0(rho_0^(k)) + up(g1(rho_1^(k))) + up(g2(rho_2^(k)))`.
       2. Compute border-projected normalized residual `r_bp^(k) = bp_64(gn - g_k / s) * valid_mask`.
       3. Compute sensitivity-normalized adjoint back-projection `bp_norm_tiers^(k)` across all 3 tiers (`D_i^{-1} G_i^T(r_bp)` with learned depth gain `bp_z_beta`).
       4. Update `hv_tiers` via residual gate `tanh(res_vox_gate[k])` + **seamed `CoreMLSepConv25DBlock`** (`dec_vox2_tiers`, weight-tied across iterations `k` with lightweight per-iter `iter_emb[k]` scale/shift) to decode `rho_tiers^(k+1)`.
     - **Deep supervision**: Return intermediate `rho_iter_preds` (`[rho^(1), rho^(2)]`) in `aux` for auxiliary data-misfit + tier loss weighting (`0.15, 0.30, 1.0`).
* **Why**: Zero parameter bloat (weight-tied `dec_vox2_tiers`), static unrolled graph (`n_iters = 3` static loop $\implies$ 100% CoreML MLProgram compatible), directly drives `data_rel_misfit` from `~9.6%` toward `~2–4%`.
* **Warm-Start**: 100% directly warm-startable from `checkpoints_v7/best_swin3d.pt` (`strict=True` via `_load_from_state_dict`, initializing `res_vox_gate[1..2] = 0.0` so `k=0` reproduces V7 step-for-step at epoch 0!).

### [V8-2] Sharpness & Amplitude-Preserving Loss (`CompoundInversionLoss`)
* **Where**: `.inv/python/losses.py` & `.inv/python/train.py`.
* **Additions**:
  1. **Peak / Percentile Amplitude Loss (`loss_amp`)**: Penalizes under-recovered body amplitude `|max_fg(|m_pred|) - max_fg(|m_true|)| / max_fg(|m_true|)` per instance (`unit_id > 0`) so the network stops trading peak density (`0.67`) for smeared support. Weight: `lambda_amp = 0.12`.
  2. **Differentiable Compactness / Focusing Loss (`loss_focus`)**: Soft $L_0$/Minimum-Support penalty $\sum \frac{\rho^2}{\rho^2 + \epsilon^2}$ on foreground-adjacent exterior cells (`d3` halo) + gradient-sharpness matching on body boundaries (`lambda_focus = 0.06`).
  3. **Higher Physics Data Weight**: Increase `lambda_data: 0.50 → 0.80` (with intermediate unrolled step supervision `0.15 * L_data(k=1) + 0.30 * L_data(k=2)`).
  4. **Compact-Body Sampling Rebalance**: Lower `large_prob: 0.22 → 0.14` and raise small/compact ground-survey scenes (`val_gc_iou = 0.469` target) so compact bodies receive ~35% more gradient updates.

### [V8-3] `v8 heavy` Capacity Scaling & Cosine Fine-Tune Schedule
* **Phase A (Warm-start Validation on `v8 base`, ~40 epochs)**:
  - Warm-start `v8 base` (`2.86M` params, `n_iters=3`) directly from `checkpoints_v7/best_swin3d.pt` (`--lr 8e-5`, 3-epoch warmup) to isolate the gain of **Unrolled Loop + Sharpness Loss** cleanly against V7.
* **Phase B (Full `v8 heavy` Run, ~120 epochs)**:
  - `V8_SIZE_PRESETS["heavy"]`: `embed_dim=192, num_heads=6, depth=8, ch_mid=64, ch_vox=40, n_sub=4, dec_depth=3, n_iters=3`.
  - Warm-start `v8 heavy` from `v8 base` (`_adapt_state_dict_tensor` zero-padding new channels with `1e-3` symmetry break as in V6.1→V7) for 100–120 epochs (`--lr 1.0e-4`).

---

## 3. Target Milestones & Verification Plan
| Metric | V7 Baseline (Ep 112) | V8 Target (`base` → `heavy`) |
|---|---:|---:|
| `data_rel_misfit` | `0.0958` | **`< 0.045`** |
| `amp_ratio` | `0.671` | **`0.82 – 0.90`** |
| `val_gc_iou` (Compact) | `0.469` | **`> 0.54`** |
| `iou` (Overall) | `0.544` | **`> 0.60`** |
| `centroid_dz` | `51.9 m` | **`< 42 m`** |

### Verification Steps Before Launching Training
1. **CPU Unit & Parity Suite** (`test_inv_v8_model.py`):
   - `v7 → v8 base` `strict=True` warm-start gives `corr > 0.99` at step 0 with zero regression.
   - Multi-iteration `n_iters=3` gradients flow cleanly through all unrolled physics steps and `loss_amp`/`loss_focus` are `0.0` at exact truth match.
   - ONNX opset 17 export + ONNX Runtime CPU & **CoreML MLProgram EP single-partition** parity (`< 1e-2`), plus MPS FP16 `NaN == 0` check.
2. **Existing Suites**: `test_inv_v7_model.py`, `test_inv_v6_model.py`, `test_inv_python.py`, and C++ `test_inv` (`NeuralPriorTest.*`) remain 100% green (zero C++ changes required since input `kC=6` and output `(1, 1, 32, 64, 64)` are unchanged).
