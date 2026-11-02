# 3D Potential Field Neural Inversion: Complete Training Guide

> **A Comprehensive Guide to Training Physics-Informed 3D Swin Transformers for Mod3D**  
> *Procedural Geological Priors, Differentiable FFT Forward Physics, Zhdanov Focusing Loss & Test-Time Adaptation*

---

## 1. Executive Training Strategy

Inverting surface potential field data (gravity $\Delta g$ and magnetics $\Delta T$) into 3D subsurface physical properties ($\Delta \rho, \chi$) is ill-posed and non-unique. Because the Earth has no vast database of true subterranean 3D density volumes with matching surveys, naive deep learning approaches fail:
* Training purely on simple geometric shapes (spheres/cubes) produces models that fail on real, complex geology (*Out-of-Distribution breakdown*).
* Training purely unsupervised on data misfit $\|G\mathbf{m} - \mathbf{d}\|_2^2$ causes the network to collapse into the non-uniqueness trap, placing all anomalies directly at the ground surface (*Depth attenuation bias*).

To solve this, `.inv` employs a **Three-Phase Hybrid Training Strategy**:

```
+---------------------------------------------------------------------------------------------------------+
|                                    3-PHASE HYBRID TRAINING FRAMEWORK                                    |
+---------------------------------------------------------------------------------------------------------+
|                                                                                                         |
|   PHASE 1: SYNTHETIC PRE-TRAINING           PHASE 2: PHYSICS-GUIDED TRAINING      PHASE 3: TEST-TIME    |
|   (Procedural Geological Prior)             (Differentiable Forward Loop)         ADAPTATION (Mod3D)    |
|                                                                                                         |
|   +--------------------------+              +---------------------------+         +-------------------+ |
|   | Stochastic Geology Gen   |              | Fast GPU FFT Forward G    |         | Real Survey Data  | |
|   | - Folding & Stratigraphy |              | (2 ms BTTB convolution)   |         | from Mod3D JSON   | |
|   | - Dipping Faults         |              +-------------+-------------+         +---------+---------+ |
|   | - Intrusive Bodies       |                            |                                 |           |
|   | - Topography Relief      |                            v                                 v           |
|   +------------+-------------+              +---------------------------+         +-------------------+ |
|                |                            | Physics & Focusing Losses |         | Pre-trained Swin3D| |
|                v                            | - Data Misfit ||Gm - d||² | ------> | Instant Inference | |
|   +--------------------------+              | - Zhdanov S_MS (volume)   |         | (< 50 ms)         | |
|   | 3D Swin Transformer      |              | - Zhdanov S_MGS (edges)   |         +---------+---------+ |
|   | Supervised Pre-training  |              +---------------------------+                   |           |
|   | (Learns structural prior)|                                                              v           |
|   +--------------------------+                                                    +-------------------+ |
|                                                                                   | Test-Time Refine  | |
|                                                                                   | (10-20 Adam steps | |
|                                                                                   |  with pfld mesh)  | |
|                                                                                   +-------------------+ |
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

### Step 1: Procedural Geological Dataset Generation

Rather than hand-labeling or hunting for paired data, `.inv/python/dataset.py` synthesizes thousands of geologically authentic 3D volumes combining five geological processes:

1. **Undulating Sedimentary Beds**: Multi-harmonic sinusoidal folding simulating regional deformation.
2. **Lithological Contrasts**: Physically realistic rock density contrasts ($\Delta \rho \in [-0.35, +0.55]\text{ g/cm}^3$).
3. **Planar & Listric Faults**: Tectonic shear cutting through strata with dipping fault angles ($45^\circ\text{–}75^\circ$) and variable throws.
4. **Compact Magmatic Intrusions**: High-density ellipsoidal plutons, volcanic plugs, and mineralized dykes acting as Zhdanov focusing targets.
5. **Topography & Survey Noise**: Synthetic DEM relief surfaces and Gaussian noise ($\sigma = 0.02\text{–}0.05\text{ mGal}$) with leveling stripe errors.

#### Option A: On-The-Fly Generation (Zero Disk Footprint, Recommended)
`SyntheticPotentialFieldDataset` generates random geological realizations on the fly during training:
```python
from dataset import SyntheticPotentialFieldDataset

# Generates new stochastic samples every epoch to prevent overfitting
dataset = SyntheticPotentialFieldDataset(n_samples=5000, nx=64, ny=64, nz=32)
```

#### Option B: Pre-Generate and Cache to Disk
```bash
python dataset.py --samples 10000 --nx 64 --ny 64 --nz 32 --output ./data/synthetic
```

---

### Step 2: Network Architecture & Differentiable Forward Physics

The network architecture in `.inv/python/model_swin3d.py` combines four specialized modules:

1. **2D Patch Tokenizer**: Extracts $4 \times 4$ patches from the surface potential field map ($\Delta g, g_{zz}, \Delta T$).
2. **2D-to-3D Dimension Lifting (Cross-Attention)**:
   * Initializes 3D query tokens $Q(x, y, z)$ embedded with depth coordinates.
   * Cross-attends to 2D surface observation keys $K(x_{\text{obs}}, y_{\text{obs}})$ with a **Green's function distance decay bias**:
     $$B_{\text{phys}}(i, j) \propto -\beta \ln\left(1 + \frac{\|\mathbf{r}_i^{\text{cell}} - \mathbf{r}_j^{\text{obs}}\|^2}{z_0^2}\right)$$
3. **3D Swin Transformer Backbone**:
   * Uses 3D shifted-window self-attention with linear computational complexity $\mathcal{O}(M)$ rather than quadratic $\mathcal{O}(M^2)$.
   * Implements **anisotropic relative position bias** (inspired by Pangu-Weather), treating vertical depth $z$ as physically distinct from horizontal strike/dip ($x, y$).
4. **Voxel Regression Head**:
   * Convolves 3D tokens to target grid dimensions ($N_x \times N_y \times N_z$) and applies scaled activation bounding physical densities.

---

### Step 3: Physics-Guided Loss & Zhdanov Annealing

The loss formulation in `.inv/python/losses.py` ensures both data fit and boundary sharpness:

$$\mathcal{L}_{\text{total}} = \lambda_{\text{sup}} \mathcal{L}_{\text{sup}}(\hat{m}, m_{\text{true}}) + \lambda_{\text{data}} \|G(\hat{m}) - \mathbf{d}_{\text{obs}}\|_2^2 + \lambda_{MS} S_{MS}(\hat{m}) + \lambda_{MGS} S_{MGS}(\hat{m})$$

#### Zhdanov Focusing Stabilizers
* **Minimum Support ($S_{MS}$)**:
  $$S_{MS}(\hat{m}) = \frac{1}{M} \sum_{j=1}^M \frac{\hat{m}_j^2}{\hat{m}_j^2 + e^2}$$
  Acts as a smooth proxy for the $L_0$ pseudo-norm, driving background diffuse noise to zero and shrinking anomalous sources into **compact volumes**.
* **Minimum Gradient Support ($S_{MGS}$)**:
  $$S_{MGS}(\hat{m}) = \frac{1}{M} \sum_{j=1}^M \frac{\|\nabla \hat{m}_j\|^2}{\|\nabla \hat{m}_j\|^2 + e_{\text{grad}}^2}$$
  Penalizes gradient transition zones, forcing gradual ramps into **razor-sharp, blocky lithological contacts**.

#### Curriculum Annealing Schedule
* **Epochs 1–10 (Structural Warmup)**: $e = 0.1$, stabilizing global geometry and background fit.
* **Epochs 11–25 (Focusing Activation)**: $e$ decays exponentially ($0.1 \to 0.01$). Diffuse halos contract into compact bodies.
* **Epochs 26–40 (Contact Sharpening)**: $e = 10^{-3}, e_{\text{grad}} = 10^{-3}$. Sharp geological fault steps and contact boundaries crystallize.

---

### Step 4: Executing the Training Run

Run the training loop using `train.py`:

```bash
# Basic run on Apple Silicon or CUDA
python train.py \
  --epochs 35 \
  --batch-size 8 \
  --lr 2e-4 \
  --n-samples 5000 \
  --save-dir ./checkpoints
```

#### Training Parameters

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--epochs` | `30` | Number of training epochs (30–40 recommended). |
| `--batch-size` | `8` | Mini-batch size (adjust based on VRAM: 4 for 8GB, 8 for 16GB, 16 for 24GB+). |
| `--lr` | `2e-4` | Initial learning rate with Cosine Annealing schedule. |
| `--n-samples` | `2000` | Number of procedural samples per epoch. |
| `--save-dir` | `./checkpoints`| Directory where `best_swin3d.pt` is saved. |
| `--device` | `auto` | Automatically picks `cuda`, `mps`, or `cpu`. |

#### Expected Console Output
```text
==================================================================
  .inv: Training 3D Potential Field Inversion Transformer         
  Device: mps | Epochs: 35 | Batch size: 8
==================================================================
[1] Initializing Procedural 3D Geology Dataset...
[2] Initializing 3D Swin Transformer...
[3] Starting Training Loop...
Epoch [01/35] Loss: 0.1842 | S_MS: 0.2410 | S_MGS: 0.3120 | Focus e: 0.1000 | LR: 2.00e-04
Epoch [10/35] Loss: 0.0984 | S_MS: 0.1650 | S_MGS: 0.1980 | Focus e: 0.0520 | LR: 1.85e-04
Epoch [20/35] Loss: 0.0431 | S_MS: 0.0890 | S_MGS: 0.1120 | Focus e: 0.0140 | LR: 1.10e-04
Epoch [35/35] Loss: 0.0189 | S_MS: 0.0340 | S_MGS: 0.0450 | Focus e: 0.0010 | LR: 1.00e-06

[4] Training Complete! Best Checkpoint saved to: ./checkpoints/best_swin3d.pt
```

---

### Step 5: Exporting to ONNX for Mod3D Deployment

Once training is complete, convert the PyTorch model checkpoint into a portable ONNX runtime model:

```bash
python export_onnx.py \
  --checkpoint ./checkpoints/best_swin3d.pt \
  --output ../../resources/models/inv_swin3d.onnx \
  --opset 17
```

* **Dynamic Axes**: The exported model supports variable input surface dimensions $(N_x \times N_y)$.
* **Model Size**: Approximately $165\text{ MB}$ (standard 32-bit float) or $83\text{ MB}$ (float16).
* **C++ Runtime**: Mod3D loads `inv_swin3d.onnx` via `inv::InversionEngine`, running on Apple Silicon Metal or NVIDIA TensorRT in **under 50 milliseconds**.

---

## 4. Phase 3: Test-Time Adaptation on Real Field Surveys

When applying the model to real field data in Mod3D:

1. **Instant Prior Prediction ($< 50\text{ ms}$)**:
   The pre-trained Transformer takes the survey's residual gravity anomaly and predicts an initial 3D density volume $\mathbf{m}_0$.
2. **Test-Time Optimization (TTO / 10–20 Steps)**:
   Because real surveys have local baseline shifts and regional trends, Mod3D performs a quick gradient descent directly on the project's data:
   * Uses `pfld::InterfaceMesh` for exact terrain-conforming forward evaluations (skipping vertical facets for maximum speed).
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
