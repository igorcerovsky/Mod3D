# 3D Potential Field Neural Inversion: Complete Training Guide

> **A Comprehensive Guide to Training Physics-Informed 3D Swin Transformers for Mod3D**  
> *Isolated Bodies & Cavities, Draped Terrain Relief, 3D Distance Penalties, Opposite-Sign Halo Suppression, Differentiable Nagy-Prism Physics & Live Training GUI*

---

## 1. Executive Training Strategy

Inverting surface potential field data (gravity $\Delta g$ and magnetics $\Delta T$) into 3D subsurface physical properties ($\Delta \rho, \chi$) is ill-posed and non-unique. Because the Earth has no vast database of true subterranean 3D density volumes with matching surveys, naive deep learning approaches fail:
* Training purely on simple geometric shapes without terrain produces models that fail on real relief (*Out-of-Distribution breakdown*).
* Training purely unsupervised on data misfit $\|G\mathbf{m} - \mathbf{d}\|_2^2$ causes the network to collapse into the non-uniqueness trap, placing all anomalies directly at the ground surface (*Depth attenuation bias*).
* Symmetric L1/L2 penalties produce unphysical opposite-sign "compensating halos" (e.g. negative density halos wrapping a positive orebody to artificially cancel gravity tails).

To solve this, `.inv` employs a **Three-Phase Hybrid Training Strategy**:

```
+---------------------------------------------------------------------------------------------------------+
|                                    3-PHASE HYBRID TRAINING FRAMEWORK                                    |
+---------------------------------------------------------------------------------------------------------+
|                                                                                                         |
|   PHASE 1: SYNTHETIC DATA GENERATION            PHASE 2: PHYSICS-GUIDED TRAINING      PHASE 3: TEST-TIME    |
|   (Isolated Bodies, Cavities & Terrain)         (Differentiable Forward Loop)         ADAPTATION (Mod3D)    |
|                                                                                                         |
|   +--------------------------+                  +---------------------------+         +-------------------+ |
|   | On-The-Fly Geometry      |                  | Draped Nagy Operator G    |         | Real Survey Data  | |
|   | - Isolated rock bodies   |                  | (~2 ms batched on MPS)    |         | from Mod3D JSON   | |
|   | - Air & water cavities   |                  +-------------+-------------+         +---------+---------+ |
|   | - Smooth/sloped terrain  |                                |                                 |           |
|   | - 30-40 cm sensor clear. |                                v                                 v           |
|   | - 3D petrophysical noise |                  +---------------------------+         +-------------------+ |
|   | - Dihedral D4 symmetries |                  | Compound Inversion Loss   |         | Pre-trained Swin3D| |
|   +------------+-------------+                  | - Depth-weighted L1+L2    | ------> | Instant Inference | |
|                |                                | - 3D Distance-weighting   |         | (< 50 ms)         | |
|                v                                | - Opposite-halo penalty   |         +---------+---------+ |
|   +--------------------------+                  | - 3D Face-jump gradient   |                   |           |
|   | 2-Channel 3D Swin        |                  | - Signed soft Dice        |                   v           |
|   | Transformer Backbone     |                  | - Draped data misfit      |         +-------------------+ |
|   | [gz, terrain/depth_max]  |                  +---------------------------+         | Linear Projection | |
|   +--------------------------+                                                        | & IRLS Refine     | |
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
python3 -m venv venv
source venv/bin/activate

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

1. **Target Types**:
   * **Rock Bodies (75% of bodies)**: High/low density anomalies ($\Delta \rho \in \pm 0.1\text{–}0.8\text{ g/cm}^3$) with random sign and rotated ellipsoidal or rectangular box geometry.
   * **Cavities (25% of bodies)**: Shallow, compact negative anomalies representing **air cavities** ($\Delta \rho \in -2.00\text{ to } -2.67\text{ g/cm}^3$) or **water-filled cavities** ($\Delta \rho \in -1.00\text{ to } -1.67\text{ g/cm}^3$).
2. **Body Multiplicity & Spacing**:
   * 40% single-body models, 60% multi-body models (2 to 4 distinct bodies).
   * Minimum separation of $\ge 2$ empty cells between bodies ensures zero overlap or merging.
   * Clear boundary margins ($\ge 3$ cells from lateral borders) prevent boundary clipping.
3. **Random Topography Relief**:
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

### Step 2: 2-Channel Network Architecture & Differentiable Forward Physics

The network architecture in `.inv/python/model_swin3d.py` combines four specialized modules:

1. **2-Channel Input Representation**:
   * Channel 0: Surface draped gravity anomaly $\Delta g(x, y)$ in mGal.
   * Channel 1: Normalized terrain depth $t(x, y) / z_{\text{total}} \in [0.0, 0.25]$.
2. **2D Patch Tokenizer & Dimension Lifting**:
   * Patch embedding converts surface observations into token representations.
   * Cross-attention lifts 2D surface observation tokens into 3D volume queries $Q(x, y, z)$ with Green's function distance decay bias.
3. **3D Swin Transformer Backbone**:
   * Uses 3D shifted-window self-attention with linear computational complexity $\mathcal{O}(M)$ rather than quadratic $\mathcal{O}(M^2)$.
   * Implements **anisotropic relative position bias**, treating vertical depth $z$ as physically distinct from horizontal strike/dip ($x, y$).
4. **MPS-Optimized Voxel Regression & Relief Masking**:
   * Uses pixel-shuffle + grouped Conv2d + 1x1 depth-mixing for fast 3D upsampling (avoiding slow PyTorch MPS `ConvTranspose3d`, running in ~0.18 s/step on Apple M4).
   * Automatically masks cells above the terrain relief surface to zero.
   * Strictly equivariant to field amplitude (scaling input data scales predicted density proportionally).
5. **Differentiable Draped-Station Forward Physics**:
   * `TerrainGravityOperator` (`forward.py`) implements batched Nagy-prism gravity forward modeling from 3D voxels to stations draped at $z_{\text{obs}} = t - \text{clearance}$.

---

### Step 3: Compound Inversion Loss, Distance Penalties & Boundary Sharpening

The loss formulation in `.inv/python/losses.py` (`CompoundInversionLoss`) addresses the core physical challenges of potential field inversion:

$$\mathcal{L}_{\text{total}} = \mathcal{L}_{\text{sup}} + \lambda_{\text{data}} \mathcal{L}_{\text{data}}$$

Every term is **scale-normalized per sample** by $a = \max |m_{\text{true}}|$, ensuring that a weak $+0.1\text{ g/cm}^3$ rock body and a $-2.67\text{ g/cm}^3$ air cavity contribute equally to gradients without collapsing to the trivial zero model:

$$\mathcal{L}_{\text{sup}} = \frac{1}{4}\left(\mathcal{L}_{\text{fg}} + \mathcal{L}_{\text{dist-L1}} + \mathcal{L}_{\text{dice}} + \mathcal{L}_{\text{grad}}\right) + \mathcal{L}_{\text{halo}}$$

#### 1. Depth-Weighted Foreground Error ($\mathcal{L}_{\text{fg}}$)
$$\mathcal{L}_{\text{fg}} = \frac{\sum_{j \in \text{body}} w_z(z_j) \left(|p_j - t_j| + \frac{1}{2}(p_j - t_j)^2\right)}{\sum_{j \in \text{body}} w_z(z_j)}$$
where $w_z(z) = 1.0 + 0.5 \cdot z / z_{\max} \in [1.0, 1.5]$ counteracts the natural $1/z^2$ decay of gravity sensitivity, specifically sharpening deep targets and bottom contacts.

#### 2. Multi-Scale 3D Distance-Weighted Background Penalty ($\mathcal{L}_{\text{dist-L1}}$)
Standard uniform L1 losses treat a false-positive voxel 500 m away identically to a 1-voxel shift at the body boundary. Using separable 3D max-pooling dilations at Chebyshev radii $r = 1, 3, 7$ voxels, distance weights $w_{\text{dist}}$ scale monotonically away from the body:
$$w_{\text{dist}} = 1.0 + 1.5\,(1 - d_1) + 2.5\,(1 - d_3) + 5.0\,(1 - d_7) \in [1.0, 10.0]$$
* **On body / contact ($r \le 1$)**: $1.0\times$ penalty.
* **Near background ($r \in [2, 3]$)**: $2.5\times$ penalty.
* **Intermediate background ($r \in [4, 7]$)**: $5.0\times$ penalty.
* **Far field ($r > 7$)**: **$10.0\times$ penalty**, suppressing far-field ghost anomalies and deep tails.

#### 3. Opposite-Sign Compensating Halo Penalty ($\mathcal{L}_{\text{halo}}$)
In unconstrained gravity inversion, networks frequently introduce negative density halos around a positive body (or positive density halos around a cavity) to artificially cancel gravity tails and minimize surface misfit.
* Sign-aware proximity zones identify regions around positive and negative bodies.
* Opposite-sign anomalies ($\text{ReLU}(-p)$ near positive bodies, $\text{ReLU}(+p)$ near negative bodies/cavities) receive a dedicated proximity-weighted penalty with weight multiplier $\beta_{\text{halo}} = 3.0$.
* A compensating opposite-sign halo incurs **$>2.5\times$ higher total loss** than a same-sign blur halo of identical amplitude.

#### 4. 3D Contact-Edge & Face-Jump Gradient Loss ($\mathcal{L}_{\text{grad}}$)
To preserve razor-sharp step-function boundaries rather than smoothed ramps, discrete gradients across all interior voxel faces $(\Delta_z, \Delta_x, \Delta_y)$ are matched to the true geometry:
$$\mathcal{L}_{\text{grad}} = \frac{1}{2}\left(\frac{\sum_f |\Delta_f p - \Delta_f t| w_z}{\sum_f (|\Delta_f p| + |\Delta_f t|) w_z + \epsilon} + \frac{\sum_{f \in \text{contact}} |\Delta_f p - \Delta_f t| w_z}{\sum_{f \in \text{contact}} w_z}\right)$$
This forces gradients to zero inside bodies and in the background (piecewise-constant geology) while requiring the full step jump $\Delta_f p = \Delta_f t$ right on the contact surfaces.

#### 5. Signed Soft Dice Loss ($\mathcal{L}_{\text{dice}}$)
$$\mathcal{L}_{\text{dice}} = 1.0 - \frac{2 \langle p, t \rangle}{\|p\|_2^2 + \|t\|_2^2 + \epsilon}$$
Simultaneously aligns volumetric spatial support, anomaly sign (+ vs -), and relative mass.

#### 6. Draped-Station Physics Data Misfit ($\mathcal{L}_{\text{data}}$)
$$\mathcal{L}_{\text{data}} = \frac{\|G(\hat{m}, z_{\text{obs}}) - d_{\text{clean}}\|_2^2}{\|d_{\text{clean}}\|_2^2 + \epsilon}$$
Computed through `TerrainGravityOperator` on the exact draped station elevations $z_{\text{obs}} = t - \text{clearance}$.

#### 7. 1-Step Optimal Linear Amplitude Projection at Inference
Because forward gravity $d = G m$ is strictly linear in density contrast, predicted priors can undergo a single-step least-squares projection:
$$\alpha^* = \frac{\langle G\hat{m}, d_{\text{obs}}\rangle}{\|G\hat{m}\|_2^2} \implies \hat{m}_{\text{proj}} = \alpha^* \hat{m}$$
This guarantees that the predicted prior's forward anomaly matches the observed field amplitude before iterative focusing begins.

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
```

#### Training Arguments

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--epochs` | `20` | Number of training epochs. |
| `--steps` | `200` | Optimizer steps per epoch (batches generated on the fly). |
| `--batch-size` | `8` | Mini-batch size (8 fits comfortably in Apple Silicon unified memory). |
| `--lr` | `5e-4` | Peak learning rate with `OneCycleLR` cosine annealing. |
| `--n-val` | `64` | Fixed seeded validation samples for deterministic tracking. |
| `--gui` | `False` | Opens a live native GUI window with 6 real-time training graphs. |
| `--save-dir` | `./checkpoints` | Checkpoint directory (`best_swin3d.pt`, `last_swin3d.pt`). |
| `--resume` | `""` | Optional checkpoint path to resume training from. |

#### Live Monitoring (GUI & Web Dashboard)
Even when running headless or via SSH, `train.py` continuously updates:
1. **Interactive GUI Window** (`--gui`): Native window with live loss curves, metrics, and geological sections.
2. **Auto-refreshing Web Dashboard** (`checkpoints/dashboard.html`): Open in any browser (`open checkpoints/dashboard.html`); auto-refreshes every 5 seconds.
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
| `body_l1_rel` | Mean L1 error restricted to true body cells | `0.000` | `1.000` |
| `data_rel_misfit` | Gravity misfit $\|G m_{\text{pred}} - d\| / \|d\|$ | `0.000` | `1.000` |
| `iou` | 3D Volumetric IoU of $|m| \ge 0.5 \max|m|$ | `1.000` | `0.000` |
| `centroid_dz` | 3D depth centroid error $|z_{\text{pred}} - z_{\text{true}}|$ | `0 m` | `NaN` (no body) |
| `amp_ratio` | Mass recovery ratio $\sum_{\text{body}} p \cdot \text{sgn}(t) / \sum |t|$ | `1.000` | `0.000` |
| `halo_opp` | Mass fraction sitting in opposite-sign compensating halos | `0.000` | `0.000` |

#### Standalone Evaluation CLI
To evaluate any trained checkpoint on independent test sets:
```bash
python evaluate.py --checkpoint checkpoints/best_swin3d.pt --samples 64
```

---

### Step 5: Exporting to ONNX for Mod3D Deployment

Once training is complete, convert the PyTorch model checkpoint into a portable ONNX runtime model:

```bash
python export_onnx.py \
  --checkpoint ./checkpoints/best_swin3d.pt \
  --output ./models/inv_swin3d.onnx
```

* **Model Input**: `(1, 2, 64, 64)` [surface gravity gz (mGal), normalized terrain depth below grid top].
* **Model Output**: `(1, 1, 32, 64, 64)` density contrast volume (g/cm³).
* **Relief Masking**: Built-in — cells above the terrain relief surface are automatically zeroed.
* **C++ Runtime**: Mod3D loads `inv_swin3d.onnx` via `inv::NeuralPriorInference::predict` (`.inv/include/inv/neural_prior.hpp`), running on Apple Silicon Metal or NVIDIA TensorRT in **under 50 milliseconds**.

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
   * Increase `--lambda-mgs` from `0.02` to `0.05` to increase edge penalties.
   * Verify that the focusing parameter $e$ is decaying to $\le 10^{-3}$ in later epochs.
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
