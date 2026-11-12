# 3D Potential Field Neural Inversion: Complete Training Guide

> **A Comprehensive Guide to Training Physics-Informed 3D Swin Transformers for Mod3D**  
> *Isolated Bodies & Cavities, Draped Terrain Relief, 3D Distance Penalties, Opposite-Sign Halo Suppression, Differentiable Nagy-Prism Physics & Live Training GUI*

---

## 1. Executive Training Strategy

Inverting surface potential field data (gravity $\Delta g$ and magnetics $\Delta T$) into 3D subsurface physical properties ($\Delta \rho, \chi$) is ill-posed and non-unique. Because the Earth has no vast database of true subterranean 3D density volumes with matching surveys, naive deep learning approaches fail:
* Training purely on simple geometric shapes without terrain produces models that fail on real relief (*Out-of-Distribution breakdown*).
* Training purely unsupervised on data misfit $\|G\mathbf{m} - \mathbf{d}\|_2^2$ causes the network to collapse into the non-uniqueness trap, placing all anomalies directly at the ground surface (*Depth attenuation bias*).
* Symmetric L1/L2 penalties produce unphysical opposite-sign "compensating halos" (e.g. negative density halos wrapping a positive orebody to artificially cancel gravity tails) and classic L2 Soft Dice leads to spiky core overshoots on partial volume coverage.
* Global contrast normalization causes weak deep rock bodies to be ignored when high-contrast cavities are present in the same domain.

To solve this, `.inv` employs a **Three-Phase Hybrid Training Strategy**:

```
+---------------------------------------------------------------------------------------------------------+
|                                    3-PHASE HYBRID TRAINING FRAMEWORK                                    |
+---------------------------------------------------------------------------------------------------------+
|                                                                                                         |
|   PHASE 1: SYNTHETIC DATA GENERATION            PHASE 2: PHYSICS-GUIDED TRAINING      PHASE 3: TEST-TIME    |
|   (Stratified Bodies, Cavities & Terrain)       (Differentiable Forward Loop)         ADAPTATION (Mod3D)    |
|                                                                                                         |
|   +--------------------------+                  +---------------------------+         +-------------------+ |
|   | On-The-Fly Geometry      |                  | Draped Nagy Operator G    |         | Real Survey Data  | |
|   | - Waste & ore rock bands |                  | (2B batched forward/step) |         | from Mod3D JSON   | |
|   | - 50/50 air & water cav. |                  +-------------+-------------+         +---------+---------+ |
|   | - 35/35/30% depth strat. |                                |                                 |           |
|   | - Smooth/sloped terrain  |                                v                                 v           |
|   | - 30-40 cm sensor clear. |                  +---------------------------+         +-------------------+ |
|   | - 3D noise + D4 augm.    |                  | Compound Inversion Loss   |         | Pre-trained v3    | |
|   +------------+-------------+                  | - Depth-wtd L1+L2 + gap   | ------> | Domain-Scaled     | |
|                |                                | - Class-bal CE + bdy Dice |         | Input (50m/L_eff) | |
|                v                                | - Fixed-denom face jumps  |         +---------+---------+ |
|   +--------------------------+                  | - Signed L1-IoU + EDT aux |                   |           |
|   | v3 U-Net + Swin/Axial +  |                  | - Opposite-halo penalty   |                   v           |
|   | Bidir Prototypes + Subpx |                  | - Charbonnier RMSE + grad |         +-------------------+ |
|   | 9-ch D4 Stem [gz, terr]  |                  +---------------------------+         | Focused IRLS      | |
|   +--------------------------+                                                        | Refinement        | |
|                                                                                       | (with pfld mesh)  | |
|                                                                                       +-------------------+ |
+---------------------------------------------------------------------------------------------------------+
```

---

## 2. Prerequisites & Environment Setup

The training pipeline requires **Python 3.10+** and **PyTorch 2.1+**. It natively supports **Apple Silicon Metal (MPS)**, **NVIDIA CUDA**, and CPU.

### 2.1 Installation

```bash
# Navigate to the python training directory
cd .inv/python

# Create and activate a clean virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install dependencies
pip install --upgrade pip
pip install -r requirements.txt
```

### 2.2 Verify Device Acceleration

Run the following one-liner to verify your acceleration backend:

```bash
python -c "import torch; print('CUDA Available:', torch.cuda.is_available(), '| MPS (Apple Silicon):', torch.backends.mps.is_available())"
```

---

## 3. Step-by-Step Training Pipeline

### Step 1: Isolated Bodies, Cavities & Terrain Data Generation

Rather than hand-labeling or hunting for paired data, `.inv/python/dataset.py` synthesizes thousands of 3D volumes containing isolated geological bodies and subsurface cavities strictly below random terrain relief:

1. **Physical Target Classes & Lithology Bounds**:
   * **Cavities (24% of bodies)**: Shallow, compact voids or elongated tunnels with **exact physical contrasts** — **air cavities** (`RHO_AIR = -2.67 g/cm³`, 50% of cavities) and **water-filled cavities** (`RHO_WATER = -1.67 g/cm³`, 50% of cavities).
   * **Non-Cavity Geological Lithologies (76% of bodies)**: Rotated compact ellipsoids/blocks and flat or dipping tabular seams/beds/sills across 4 distinct continuous contrast bands:
     * **Light Rock / Coal / Loose Clay & Fill** (`[-1.55, -0.85] g/cm³`, 21% of rocks): coal seams (lignite/bituminous/anthracite), peat, soft porous clay, unconsolidated overburden (55% tabular seam geometry).
     * **Sediments / Compacted Clay / Waste Rock** (`[-0.80, -0.10] g/cm³`, 32% of rocks): sandstone, shale, compacted clay, evaporites/halite, mining waste (45% tabular beds).
     * **Mafic / Ultramafic / Disseminated Ore** (`[+0.15, +0.75] g/cm³`, 32% of rocks): basalt, gabbro, peridotite/dunite/pyroxenite, skarn, weak mineralization (30% dipping sills/dikes).
     * **High-Grade Mineral Ore** (`[+0.80, +1.85] g/cm³`, 15% of rocks): massive sulfide, magnetite/hematite, chromite pods & veins.
2. **Depth Stratification, Hard Contact/Composite Scenes & Multi-Body SNR Guard**:
   * **Mixed Regime (`hard_prob = 0.50`, configurable via `--hard-prob`)**: Half of training scenes follow the standard isolated distribution (`40%` single body, `60%` 2–4 bodies with $\ge 2$-cell separation and lateral margin $\ge 3$ cells), and half sample 5 physically well-posed harder geological archetypes while avoiding full-domain DC Bouguer slab ambiguity and sub-noise companions:
     * `large_composite`: multi-lobe non-convex intrusions, bent seams, and L-shapes (`~1–2.4 km` span, bounded away from lateral edges so anomaly flanks remain inside the observation window, including ridge-hosted crops).
     * `nested_halo`: `ore` cores embedded inside `mafic` halos or `air`/`water` cavities carved inside rock hosts with analytical gravity parity (`0.20–5.0×`).
     * `abutting_contact`: zero-gap adjacent lithology boundaries, fault-thrown offset blocks, and steep dyke feeders intersecting shallower sills.
     * `vertical_stack`: shallow overburden/water lenses directly overlying deeper dense targets (`0.25–4.0×` mGal parity).
     * `tight_cluster`: `3–5` closely spaced (`1`-cell minimum clearance) mixed-lithology targets.
   * Non-cavity rock tops follow explicit **35% shallow / 35% mid / 30% deep** depth stratification below topography.
   * Depth-proportional minimum radii ($r_x, r_y \ge 0.20 \cdot \text{gap} \cdot (n_x/64)$, $r_z \ge 0.14 \cdot \text{gap} \cdot (n_z/32)$) and deep rock contrast floor ($|\Delta\rho| \ge 0.32\text{ g/cm}^3$ when $\text{gap} > 0.22\,n_z$).
   * **Multi-Body Analytical SNR Guard**: Estimates peak surface gravity contribution $\propto |\Delta\rho|\cdot V / z_{\text{eff}}^2$; boosts or skips weaker bodies below $0.10\text{–}0.14\times$ the scene peak so shallow air cavities do not drown weaker targets below noise.
4. **Random Topography Relief**:
   * Multi-scale Gaussian-filtered random terrain depth $t(x, y)$ with optional regional tilt.
   * Relief up to 25% of total domain depth (15% flat terrain cases for baseline stability).
   * Density strictly zeroed above relief surface (air cells carry zero density contrast).
4. **Realistic Gravimeter Sensor Clearance**:
   * Stations are draped directly on the terrain with realistic relative gravity meter tripod clearance: **$30\text{–}40\text{ cm}$ ($0.30\text{–}0.40\text{ m}$)** above the ground.
5. **3D Petrophysical Voxel Noise (Denoising Supervision)**:
   * Real host rocks have internal density variations and 3D subsurface clutter.
   * During forward simulation (`simulate_batch`), 3D voxel density noise ($\sigma_{\text{body}} = 0.08 |\rho|$ internal heterogeneity + $\sigma_{\text{bg}}$ host-rock clutter below terrain) is added to the density volume that produces the surface gravity field `obs_noisy`.
   * **Crucial distinction**: The supervision target `rho` remains strictly clean in the background. The network is trained as a true **geological denoiser** that rejects 3D subsurface clutter and surface sensor noise ($\sigma = 0.5\%\text{–}4\% \cdot \max|g_z|$) while outputting clean, sharp bodies.
6. **Dihedral $D_4$ Horizontal Symmetry Augmentation**:
   * Gravity physics is strictly equivariant to horizontal rotations and reflections.
   * `augment_d4` applies random $90^\circ, 180^\circ, 270^\circ$ rotations and horizontal reflections on the GPU batch during training, providing $8\times$ effective directional coverage with zero forward modeling overhead.

#### On-The-Fly Generation (Zero Disk Footprint, Recommended)
`BodyTerrainDataset` generates fresh random geometry in DataLoader workers (numpy) while `simulate_batch` runs exact draped-station Nagy prism forward modeling batched on GPU/MPS (~2 ms per sample):
```python
from dataset import BodyTerrainDataset, simulate_batch
from forward import TerrainGravityOperator

# Generates infinite stochastic samples on the fly
dataset = BodyTerrainDataset(n_samples=2000, nx=64, ny=64, nz=32, cell=50.0)
op = TerrainGravityOperator(64, 64, 32, 50.0, 50.0, 50.0).to("mps")
```

---

### Step 2: v3 Terrain-Aware Axial+Swin3D & Lithology Prototype Architecture

The default v3 model (`PotentialFieldTransformer3DV3` in `.inv/python/model_swin3d.py`, `2.70 M` params, instantiated alongside legacy `PotentialFieldTransformer3D` v2 and `PotentialFieldTransformer3DV1` v1 via `create_model`) targets narrow-gap separation, rough topography, and long-range bodies:

1. **9-Channel Physics Input + Scale Conditioning**:
   * Expands `[gz (mGal), terrain_depth / depth_max]` into 9 D4-equivariant channels: `gn = gz / s`, `asinh(8 * gn)`, four local multi-scale residuals (`3×3`, `9×9`, `17×17`, `25×25`), **horizontal gradient magnitude** $|\nabla_{xy} g_n|$, smooth **local tilt proxy**, and scaled terrain relief.
   * `scale_mlp` embeds `[log(s/SLAB), s/(s+SLAB), max(gz)/s, min(gz)/s]` into global amplitude conditioning `se`.
2. **2D U-Net Encoder + Alternating 3D Swin / Axial-Global Bottleneck**:
   * A 3-stage 2D U-Net encoder (`64 → 32 → 16`, `c0=32, c1=64, c=128`) feeds global 2D self-attention before depth lifting.
   * Alternates `SwinBlock3D` (local `8×4×4` joint 3D shifted windows) and `AxialGlobalBlock3D` (full-grid `16×16` horizontal XY + full-depth `Z` column attention) so both `Large Composite` intrusions and `Vertical Stack` columns mix end-to-end.
3. **Bidirectional Lithology Prototypes (`proto_emb`)**:
   * 7 semantic classes (**0: bg**, **1: air `-2.67`**, **2: water `-1.67`**, **3: light/coal `[-1.55, -0.85]`**, **4: sediment/waste `[-0.80, -0.10]`**, **5: mafic `[+0.15, +0.75]`**, **6: ore `[+0.80, +1.85]`**) × `n_sub=3` sub-prototypes with bidirectional cross-attention (`voxels ↔ prototypes`) and a learnable logit temperature scale.
4. **Terrain-Conditioned Subpixel Decoder (`SepSubpixelUp3D` + `DepthConditionedSkip`)**:
   * Dual-stage upsampling (`16 → 32 → 64`, `ch_mid=40, ch_vox=24`) pairs a smooth trilinear base with learned XY `pixel_shuffle(2)` + Z `2×` detail, injecting 2D skips dynamically gated by exact terrain-relative depth `(z - terr)` (`DepthConditionedSkip`) and `SepConv25DBlock` refinement.
5. **Bounded Density Head & Training-Only Surface-Distance Head**:
   * Output contrast combines prototype probabilities with bounded `rock_head` bands, masked above terrain; an auxiliary `edt_head` (`aux["edt"]`) supervises interior distance-to-surface during training only (omitted on ONNX export).
6. **Differentiable Draped-Station Forward Physics**:
   * `TerrainGravityOperator` (`forward.py`) runs batched (`2B` in a single call inside `simulate_batch`) Nagy-prism gravity forward modeling at $z_{\text{obs}} = t - \text{clearance}$.

---

### Step 3: Compound Inversion Loss, Lithology Supervision & Boundary Sharpening

The loss formulation in `.inv/python/losses.py` (`CompoundInversionLoss`) addresses the core physical challenges of potential field inversion:

$$\mathcal{L}_{\text{total}} = \mathcal{L}_{\text{sup}} + \lambda_{\text{cls}} \mathcal{L}_{\text{cls}} + \mathcal{L}_{\text{edt}} + \lambda_{\text{data}} \mathcal{L}_{\text{data}}$$

Every structural term is **scale-normalized per sample** by $a = \max |m_{\text{true}}|$, ensuring that a weak mineral/waste rock body and a $-2.67\text{ g/cm}^3$ air cavity contribute proportionally without collapsing to zero:

$$\mathcal{L}_{\text{sup}} = 0.23\left(\mathcal{L}_{\text{fg}} + \mathcal{L}_{\text{dist-L1}} + \mathcal{L}_{\text{Tanimoto}} + \mathcal{L}_{\text{grad}}\right) + 0.12\,\mathcal{L}_{\text{band}} + \mathcal{L}_{\text{halo}} + \lambda_{\text{ms}}\,\mathcal{L}_{\text{MGS}}$$

#### 1. Depth-Weighted & Local-Contrast Foreground Error ($\mathcal{L}_{\text{fg}}$)
$$\mathcal{L}_{\text{fg}} = \frac{\sum_{j \in \text{body}} w_z(z_j) \left(e_{\text{loc}, j} + 0.35\,e_{\text{loc}, j}^2 + 0.20\,\text{Overshoot}_j\right)}{\sum_{j \in \text{body}} w_z(z_j)}$$
where:
* **Local Contrast Normalization**: $e_{\text{loc}, j} = \frac{|p_{n,j} - t_{n,j}|}{\max(|t_{n,j}|, 0.05)}$ normalizes error by the body's actual local contrast. In scenes containing both a $-2.67\text{ g/cm}^3$ cavity and a $+0.2\text{ g/cm}^3$ deep rock body, this prevents the cavity from dominating the supervised gradient.
* **Mild Core Overshoot Guard**: $\text{Overshoot}_j = \frac{\text{ReLU}(|p_{n,j}| - |t_{n,j}|)}{\max(|t_{n,j}|, 0.05)}$ guards against spiky cones without systematically pulling interior amplitudes down.
* **Depth Ramp**: $w_z(z) = 1.0 + \text{depth\_boost} \cdot z / z_{\max} \in [1.0, 1.5]$ (default `depth_boost = 0.5`) counteracts the natural $1/z^2$ decay of gravity sensitivity, specifically sharpening deep targets and bottom contacts.

#### 2. Multi-Scale 3D Distance & Inter-Body Gap Saddle Penalty ($\mathcal{L}_{\text{dist-L1}}$)
Using separable 3D max-pooling dilations ($r = 1, 3, 7$) plus directional same-axis gap detection (`_gap_saddle_mask`, background voxels bracketed on opposite sides within `1–3` cells):
$$w_{\text{dist}} = 1.0 + 1.5\,(1 - m_{\text{fg}}) + 1.5\,(1 - d_1) + 2.0\,(1 - d_3) + 4.0\,(1 - d_7) + 6.0\,m_{\text{gap}}$$
* **Inside true body ($m_{\text{fg}} = 1$)**: $1.0\times$; **Immediate outer shell ($r = 1$)**: $2.5\times$; **Near ($r \in [2, 3]$)**: $4.0\times$; **Far ($r > 7$)**: $10.0\times$; **Inter-body gap saddle ($m_{\text{gap}} = 1$)**: $+6.0\times$ bonus penalty to prevent tightly clustered or stacked bodies from bridging.

#### 3. Opposite-Sign Compensating Halo Penalty ($\mathcal{L}_{\text{halo}}$)
Sign-aware proximity zones identify regions around positive and negative bodies; opposite-sign compensating halos receive a dedicated proximity-weighted penalty with multiplier $\beta_{\text{halo}} = 3.0$.

#### 4. Fixed-Denominator 3D Face-Jump Gradient & Interior Flatness ($\mathcal{L}_{\text{grad}}$)
To avoid denominator blow-up when predictions are flat inside bodies while enforcing razor-sharp step boundaries:
$$\mathcal{L}_{\text{grad}} = 0.40\,\frac{\sum_f |\Delta_f p - \Delta_f t| w_z}{\sum_f |\Delta_f t| w_z + 10^{-4}} + 0.35\,\frac{\sum_{f \in \text{contact}} |\Delta_f p - \Delta_f t| w_z}{\sum_{f \in \text{contact}} w_z} + 0.25\,\frac{\sum_{f \in \text{interior}} |\Delta_f p| w_z}{\sum_{f \in \text{interior}} w_z}$$
Using only the fixed true gradient norm in the denominator preserves steepness right on contact faces while driving interior body gradients to zero.

#### 5. Multi-Band Mass Conservation ($\mathcal{L}_{\text{band}}$) & Zhdanov MGS ($\mathcal{L}_{\text{MGS}}$)
* **$\mathcal{L}_{\text{band}}$**: Enforces regional mass balance separately across strong negative cavities ($t_n \le -0.5$), weak negative waste rock ($-0.5 < t_n < 0$), strong ore ($t_n \ge 0.5$), and weak ore ($0 < t_n < 0.5$) so weaker targets do not disappear beside cavities.
* **$\mathcal{L}_{\text{MGS}}$**: Evaluates `ZhdanovFocusingLoss.minimum_gradient_support` on predicted vs. true normalized models (weighted by `--lambda-ms`, default `0.05`).

#### 6. Class-Balanced Lithology Loss ($\mathcal{L}_{\text{cls}}$) & Surface-Distance ($\mathcal{L}_{\text{edt}}$)
When training v2/v3 (`aux["logits"]`, weighted by `--lambda-cls`, default `0.25`), `_lithology_loss` supervises the 7 classes `{bg, air, water, light, sediment, mafic, ore}` with:
* Class-balanced foreground CE (`50%` voxel-averaged + `50%` per-present-class mean so rare `air`/`water` cavities match `sediment`/`mafic` gradient strength), `gap_mask`-weighted boundary CE (`0.35`), and far background CE (`0.15`).
* Soft Dice over present foreground classes **plus boundary-shell background (`class 0` inside $d_3$)** and absent-class leakage suppression.
* **v3 Surface-Distance Head ($\mathcal{L}_{\text{edt}}$)**: Supervises `aux["edt"]` against iterative 3D morphological interior depth (`_interior_edt_proxy`) with gap-saddle weighting (`+3×`) so body cores peak separately from narrow gaps.

#### 7. Signed L1-IoU / Tanimoto Loss ($\mathcal{L}_{\text{Tanimoto}}$)
$$\mathcal{L}_{\text{Tanimoto}} = 1.0 - \frac{2 \sum \min\left(\text{ReLU}(p_n \cdot \text{sgn}(t_n)), |t_n|\right)}{\|p_n\|_1 + \|t_n\|_1 + \epsilon}$$
Guarantees the **unique global optimum is strictly $p_n = t_n$** regardless of partial spatial coverage.

#### 8. Charbonnier RMSE & Horizontal-Gradient Data Misfit ($\mathcal{L}_{\text{data}}$)
$$\mathcal{L}_{\text{data}} = 0.65\,\mathcal{L}_{\text{amp}} + 0.35\,\mathcal{L}_{\nabla d}$$
where $\mathcal{L}_{\text{amp}}$ and $\mathcal{L}_{\nabla d}$ use zero-at-exact Charbonnier relative RMSE ($\sqrt{\|r\|_2^2 / \|d\|_2^2 + 10^{-6}} - 10^{-3}$) over surface $g_z$ and its horizontal $(x, y)$ differences from `TerrainGravityOperator`, keeping physics gradients strong through late fine-tuning epochs.

#### 9. Domain Input Scaling & Linear Amplitude Projection at Inference
* Both `infer_prior.py` (`amplitude_scale`) and C++ `inv::NeuralPriorInference::predict` (`neural_prior.hpp`) rescale input surface `gz` by $50\text{ m} / L_{\text{eff}}$ before ONNX/PyTorch inference so real domains of arbitrary cell size map into calibrated $50\text{ m}$ training gravity units and true $\text{g/cm}^3$ contrast bounds.
* For legacy **v1** models, `infer_prior.py` (`project=None`, auto-enabled for v1) applies 1-step `project_amplitude` scaling; for calibrated **v2/v3** models, raw bounded predictions are preserved unless `--project` / `project=True` is explicitly requested.

---

### Step 4: Executing the Training Run & Live Monitoring

Run the training loop using `train.py`:

```bash
# Basic run with automatic device selection (MPS on Mac, CUDA on Linux)
python train.py --epochs 20 --steps 200 --batch-size 8

# Live macOS GUI window with 6 visual panels updated every epoch
python train.py --epochs 20 --steps 200 --batch-size 8 --gui

# Smoke test (~1 minute)
python train.py --epochs 2 --steps 20 --n-val 8

# Extended multi-hour training run
python train.py --epochs 40 --steps 300 --batch-size 8 --lr 5e-4 --n-val 64 --gui

# Resuming / Fine-tuning with automatic Cosine Annealing decay
python train.py --resume checkpoints/best_swin3d.pt --epochs 40 --lr 1.5e-4 --n-val 128 --gui
```

#### Training Arguments

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--epochs` | `20` | Number of training epochs. |
| `--steps` | `200` | Optimizer steps per epoch (batches generated on the fly). |
| `--batch-size` | `8` | Mini-batch size (8 fits comfortably in Apple Silicon unified memory). |
| `--lr` | `5e-4` | Learning rate (`OneCycleLR` with `div_factor=10, final_div_factor=50` from scratch; `CosineAnnealingLR` on `--resume`). |
| `--lambda-data` | `0.25` | Weight on draped Charbonnier RMSE + gradient data misfit $\mathcal{L}_{\text{data}}$. |
| `--lambda-ms` | `0.05` | Weight on Zhdanov Minimum Gradient Support structural match. |
| `--lambda-cls` | `0.25` | Weight on 7-class lithology prototype cross-entropy + soft Dice ($\mathcal{L}_{\text{cls}}$). |
| `--depth-boost`| `0.5` | Linear vertical sensitivity ramp $w_z(z) = 1.0 + \text{depth\_boost} \cdot z/z_{\max}$. |
| `--ema-decay` | `0.995` | Exponential moving average (`ModelEMA`) decay for validation and `best_swin3d.pt` weights (`0` disables). |
| `--n-val` | `64` | Fixed seeded standard validation samples (`hard_prob=0.0`, `seed=2024`, plus `n_val//2` hard-only samples `seed=3024` when `--hard-prob > 0`). |
| `--hard-prob` | `0.50` | Fraction of training batches using harder composite, nested, abutting, stacked, and tight-cluster scenes `[0.0-1.0]`. |
| `--num-workers` | `0` | PyTorch DataLoader workers (set `2–4` for fast on-the-fly CPU synthesis). |
| `--gui` | `False` | Opens a live native GUI window with 6 real-time training graphs. |
| `--save-dir` | `./checkpoints` | Checkpoint directory (`best_swin3d.pt`, `last_swin3d.pt`). |
| `--resume` | `""` | Checkpoint path to resume from (auto-selects `v1`, `v2`, or `v3` via `ck["arch"]` and loads `raw_model_state_dict` + `ema_state_dict` when present). |

#### Live Monitoring (GUI & Web Dashboard)
Even when running headless or via SSH, `train.py` continuously updates:
1. **Interactive GUI Window** (`--gui`): Native window with live loss curves, metrics, and geological sections.
2. **HTML5 Direct Canvas Web Dashboard** (`checkpoints/dashboard.html`): Open in any browser (`open checkpoints/dashboard.html`); renders raw numerical voxel arrays directly onto HTML5 `<canvas>` without image-caching artifacts and auto-refreshes every 5 seconds. Supports both real-time epoch history and full run replays.
3. **High-Resolution Snapshot** (`checkpoints/training_monitor.png`): 6-panel summary chart updated every epoch:
   * **Panel 1 (Losses)**: Train Loss, Val Loss, Relative Data Misfit, Opposite-Sign Halo loss.
   * **Panel 2 (3D Recovery)**: 3D Volumetric IoU ($\ge 50\%$ peak), Amplitude Ratio ($\sum p / \sum t$), 1 - Body Rel L1.
   * **Panel 3 (Depth Accuracy)**: 3D Depth Centroid Error [m] with 1-cell reference line.
   * **Panel 4 (Draped Gravity Map)**: Surface $g_z$ anomaly [mGal] with profile slice marker.
   * **Panel 5 (Ground Truth X-Z Section)**: Vertical density cross-section below relief with shaded air cells.
   * **Panel 6 (Swin-3D Prediction X-Z Section)**: Predicted vertical density cross-section below relief.

#### Validation Metrics Reference

| Metric | Meaning | Ideal Value | Zero Model |
| :--- | :--- | :--- | :--- |
| `l1_rel` | Mean whole-volume L1 error / $\max|m_{\text{true}}|$ | `0.000` | $\approx$ Body volume fraction ($\approx 0.003$) |
| `body_l1_rel` | Mean relative L1 error inside true bodies normalized per voxel by $|t_n|$ | `0.000` | `1.000` |
| `data_rel_misfit` | Gravity misfit $\|G m_{\text{pred}} - d\| / \|d\|$ | `0.000` | `1.000` |
| `iou` | 3D Volumetric IoU with per-body local contrast thresholds | `1.000` | `0.000` |
| `centroid_dz` | Mass-weighted per-body vertical-cylinder depth centroid error $|z_{\text{pred}} - z_{\text{true}}|$ | `0 m` | `NaN` (no body) |
| `amp_ratio` | Mass recovery ratio $\sum_{\text{body}} p \cdot \text{sgn}(t) / \sum |t|$ | `1.000` | `0.000` |
| `halo_opp` | Mass fraction sitting in opposite-sign compensating halos | `0.000` | `0.000` |

#### Standalone Evaluation CLI
To evaluate any trained checkpoint on independent standard or hard-only test sets:
```bash
python evaluate.py --checkpoint checkpoints/best_swin3d.pt --samples 64
python evaluate.py --checkpoint checkpoints/best_swin3d.pt --samples 64 --hard-prob 1.0
```

---

### Step 5: Exporting to ONNX for Mod3D Deployment

Once training is complete, convert the PyTorch model checkpoint into a portable ONNX runtime model:

```bash
python export_onnx.py \
  --checkpoint ./checkpoints/best_swin3d.pt \
  --output ./models/inv_swin3d.onnx
```

* **Model Input**: `(1, 2, 64, 64)` [surface gravity gz scaled by `50 m / L_eff`, normalized terrain depth below grid top].
* **Model Output**: `(1, 1, 32, 64, 64)` density contrast volume (`g/cm^3`, calibrated in `[-2.67, +1.85]` for 7-class v2).
* **Relief Masking**: Built-in — cells above the terrain relief surface are automatically zeroed.
* **C++ Runtime**: Mod3D loads `inv_swin3d.onnx` via `inv::NeuralPriorInference::predict` (`.inv/include/inv/neural_prior.hpp`), which rescales input channel 0 by `kTrainCell / cbrt(eff)` when `kC >= 2` so legacy `v1` linear models and calibrated `v2/v3` lithology models output physical `g/cm^3` on domains of any voxel spacing.

#### Generating Neural Priors for Mod3D Inversion JSON
To inject a neural prior into a Mod3D setup JSON:
```bash
python infer_prior.py \
  --setup setup.json \
  --checkpoint checkpoints/best_swin3d.pt \
  --output setup_prior.json
```

---

## 4. Phase 3: Test-Time Adaptation on Real Field Surveys

When applying the model to real field data in Mod3D:

1. **Instant Prior Prediction ($< 50\text{ ms}$)**:
   The pre-trained Transformer takes the survey's residual gravity anomaly and predicts an initial 3D density volume $\mathbf{m}_0$.
2. **Test-Time Optimization (TTO / 10–20 Steps)**:
   Because real surveys have local baseline shifts and regional trends, Mod3D performs a quick gradient descent directly on the project's data:
   * Uses `pfld::InterfaceMesh` (`PfldInterfaceMeshOperator`) as the primary forward operator for exact terrain-conforming forward evaluations with triangular DEM relief facets and horizontal shared quad interfaces (skipping vertical planar facets for maximum speed).
   * Fast FFT block-Toeplitz operator (`ToeplitzGravityOperator`) is retained solely as an optional accelerator and benchmark baseline for flat grids (`use_fft_solver_benchmark = true`).
   * Drives the data misfit residual below $1\%$.
3. **Drillhole & Well Locking**:
   If wells (`mod3d::Well`) or seismic guidelines (`mod3d::Guideline`) exist in the project, known geological contacts and density logs are clamped as fixed Dirichlet boundaries:
   $$\mathbf{m}(\mathbf{r}_{\text{well}}) \equiv \rho_{\text{measured}}$$
   The inversion seamlessly fits the field around the known control points.

---

## 5. Hardware Recommendations & Training Times

| Platform | VRAM / RAM | Grid Resolution | Epochs | Approx. Training Time |
| :--- | :--- | :--- | :--- | :--- |
| **Apple Silicon (M1/M2/M3 Pro/Max)** | 16–36 GB Unified | $64 \times 64 \times 32$ | 35 | $\approx 1.0\text{–}1.5\text{ hours}$ |
| **NVIDIA RTX 3080 / 4080** | 10–16 GB VRAM | $64 \times 64 \times 32$ | 35 | $\approx 35\text{–}45\text{ minutes}$ |
| **NVIDIA RTX 4090 / A100** | 24–80 GB VRAM | $128 \times 128 \times 64$ | 50 | $\approx 2.5\text{–}4.0\text{ hours}$ |
| **Modern Multi-core CPU** | 32 GB RAM | $32 \times 32 \times 16$ | 20 | $\approx 2.0\text{ hours}$ |

---

## 6. Troubleshooting & Best Practices

1. **Out-of-Memory (OOM) Errors**:
   * Reduce `--batch-size` to `4` or `2`.
   * Lower the base grid resolution in `dataset.py` (e.g. from $128 \times 128 \times 64$ to $64 \times 64 \times 32$).
2. **Predictions Are Too Smooth (Blurred Blobs)**:
   * Increase `--lambda-ms` from `0.05` to `0.10` or `--lambda-cls` from `0.25` to `0.35` to sharpen prototype edges.
   * Verify that `--ema-decay` is active and gradients are not masked by missing `--lambda-data`.
3. **Artifacts Near Domain Boundaries**:
   * Ensure `dataset.py` applies zero-padding or periodic reflection along domain borders.
   * Enable 3-level coarse-to-fine hierarchy in `InversionConfig::coarse_to_fine_levels = 3`.

---

## 7. References & Bibliography

### Focusing Inversion & Regularization Theory
1. **Zhdanov, M. S. (2002)**: *Geophysical Inverse Theory and Regularization Problems*. Methods in Geochemistry and Geophysics, Vol. 36, Elsevier, Amsterdam, 628 pp.
2. **Portniaguine, O. & Zhdanov, M. S. (1999)**: *Focusing geophysical inversion images*. *Geophysics*, 64(3), 874–887. [doi:10.1190/1.1444596](https://doi.org/10.1190/1.1444596).
3. **Portniaguine, O. & Zhdanov, M. S. (2002)**: *3-D magnetic inversion with data compression and image focusing*. *Geophysics*, 67(5), 1532–1541. [doi:10.1190/1.1512749](https://doi.org/10.1190/1.1512749).
4. **Li, Y. & Oldenburg, D. W. (1996)**: *3-D inversion of magnetic data*. *Geophysics*, 61(2), 394–408. [doi:10.1190/1.1443968](https://doi.org/10.1190/1.1443968).
5. **Li, Y. & Oldenburg, D. W. (1998)**: *3-D inversion of gravity data*. *Geophysics*, 63(1), 109–119. [doi:10.1190/1.1444302](https://doi.org/10.1190/1.1444302).

### Vision Transformers & Deep Learning Inversion
6. **Liu, Z., Lin, Y., Cao, Y., Hu, H., Wei, Y., Zhang, Z., Lin, S., & Guo, B. (2021)**: *Swin Transformer: Hierarchical Vision Transformer using Shifted Windows*. In *Proceedings of the IEEE/CVF International Conference on Computer Vision (ICCV)*, pp. 10012–10022.
7. **Yu, P., Zhou, L., Zhou, S., Jiao, J., Huang, G., & Lu, P. (2025)**: *Physics-Constrained Three-Dimensional Swin Transformer for Gravity Data Inversion*. *Remote Sensing*, 17(2), 245. [doi:10.3390/rs17020245](https://doi.org/10.3390/rs17020245).

### AI Weather Modeling & Multi-Scale Earth Representation
8. **Bi, K., Xie, L., Zhang, H., Chen, X., Gu, X., & Tian, Q. (2023)**: *Accurate medium-range global weather forecasting with 3D neural networks (Pangu-Weather)*. *Nature*, 619, 533–538. [doi:10.1038/s41586-023-06185-3](https://doi.org/10.1038/s41586-023-06185-3).
9. **Lam, R., Sanchez-Gonzalez, A., Willson, M., Wirnsberger, P., Fortunato, M., Alet, F., et al. (2023)**: *Learning skillful medium-range global weather forecasting (GraphCast)*. *Science*, 382(6677), 1416–1421. [doi:10.1126/science.adi2336](https://doi.org/10.1126/science.adi2336).
10. **Nguyen, T., Brandstetter, J., Kapoor, A., Gupta, J. K., & Grover, A. (2023)**: *ClimaX: A foundation model for weather and climate*. In *International Conference on Machine Learning (ICML)*, PMLR 202:25909–25938.

### Analytical Potential Field Kernels & Reference Implementations
11. **Nagy, D. (1966)**: *The gravitational attraction of a right rectangular prism*. *Geophysics*, 31(2), 362–371. [doi:10.1190/1.1439779](https://doi.org/10.1190/1.1439779).
12. **Cerovský, I.**: *Inv3D: 3D Potential Field Inversion Research Repository*. [https://github.com/igorcerovsky/Inv3D](https://github.com/igorcerovsky/Inv3D).
