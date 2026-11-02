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

### Step 3: Physics-Guided Loss & Zhdanov Focusing Stabilizers ($S_{MS}$ and $S_{MGS}$)

The loss formulation in `.inv/python/losses.py` ensures both data fit and boundary sharpness:

$$\mathcal{L}_{\text{total}} = \lambda_{\text{sup}} \mathcal{L}_{\text{sup}}(\hat{m}, m_{\text{true}}) + \lambda_{\text{data}} \|G(\hat{m}) - \mathbf{d}_{\text{obs}}\|_2^2 + \lambda_{MS} S_{MS}(\hat{m}) + \lambda_{MGS} S_{MGS}(\hat{m})$$

**$S_{MS}$** and **$S_{MGS}$** are **Focusing Stabilizers** introduced by **Michael S. Zhdanov and Oleg Portniaguine (1999, 2002)**. They solve the single biggest problem with traditional potential field inversion: **the blurry, diffuse "clouds" caused by classical Tikhonov $L_2$ regularization**.

| Metric | Full Name | What it Measures | Geological Effect |
| :--- | :--- | :--- | :--- |
| **$S_{MS}$** | **Minimum Support** | **Anomalous Volume** (approximates $L_0$ norm of $m$) | Shrinks diffuse halos; produces **compact, isolated bodies** (ore pockets, plutons). |
| **$S_{MGS}$** | **Minimum Gradient Support** | **Contact Area** (approximates $L_0$ norm of $\nabla m$) | Eliminates gradual ramps; produces **sharp, blocky lithological contacts** (fault scarps, dyke walls). |

---

#### 1. Why Classical $L_2$ Regularization Fails in Geology

In standard Tikhonov $L_2$ inversion, the regularizer penalizes $\|m\|_2^2 = \sum m_i^2$ or $\|\nabla m\|_2^2 = \sum (\nabla m_i)^2$. 

Because $L_2$ squares the values, having one cell with a large density anomaly (e.g., $+0.4\text{ g/cm}^3$) incurs a penalty of $0.4^2 = 0.16$. If that same mass is smeared across 10 cells at $+0.04\text{ g/cm}^3$, the penalty drops tenfold to $10 \times 0.04^2 = 0.016$. 

Therefore, **$L_2$ regularization mathematically prefers wide, spread-out, blurry clouds** over sharp, realistic geological bodies.

---

#### 2. $S_{MS}$: Minimum Support (Compactness / Volume Minimizer)

##### Mathematical Formulation
$$S_{MS}(\mathbf{m}) = \sum_{j=1}^M \frac{(m_j - m_{0,j})^2}{(m_j - m_{0,j})^2 + e^2}$$

where:
* $m_j$ is the density contrast of voxel cell $j$.
* $m_{0,j}$ is the prior / background value (typically $0.0$).
* $e$ is a small focusing threshold parameter (e.g., $10^{-3}$).

##### How It Works Mechanically
Look at how the term $f(x) = \frac{x^2}{x^2 + e^2}$ behaves where $x = m_j - m_{0,j}$:
* **Inside the target body** ($|x| \gg e$, e.g., $\Delta \rho = 0.40$ and $e = 0.001$):
  $$\frac{0.40^2}{0.40^2 + 0.001^2} = \frac{0.16}{0.160001} \approx \mathbf{0.99999 \approx 1}$$
* **In background rock** ($|x| \ll e$, e.g., $\Delta \rho = 0.0001$ and $e = 0.001$):
  $$\frac{0.0001^2}{0.0001^2 + 0.001^2} = \frac{10^{-8}}{10^{-6}} \approx \mathbf{0.01 \approx 0}$$

##### The Mathematical Insight ($L_0$ Pseudo-Norm)
Every cell with a real anomaly contributes $\approx 1$, while near-zero background cells contribute $\approx 0$. 
Therefore, **$S_{MS}$ literally counts the number of anomalous cells** (the geometric volume of the body's mathematical *support*):
$$\lim_{e \to 0} S_{MS}(\mathbf{m}) = \text{Volume of anomalous bodies} = \|\mathbf{m} - \mathbf{m}_0\|_0$$

Minimizing $S_{MS}$ forces the inversion to concentrate mass into the **smallest possible volume** that still satisfies the observed potential field.

---

#### 3. $S_{MGS}$: Minimum Gradient Support (Sharp Blocky Boundaries)

##### Mathematical Formulation
$$S_{MGS}(\mathbf{m}) = \sum_{j=1}^M \frac{\|\nabla m_j\|^2}{\|\nabla m_j\|^2 + e_{\text{grad}}^2}$$

where $\nabla m_j = (\partial_x m, \partial_y m, \partial_z m)$ is the 3D spatial gradient vector computed across adjacent voxels.

##### How It Works Mechanically
Instead of evaluating the property value, $S_{MGS}$ evaluates the **spatial gradient** (the rate of change between adjacent cells):
* **Inside a homogeneous body or background**: $|\nabla m| \approx 0 \implies \text{term} \approx 0$.
* **Across a sharp, single-cell fault step**: $|\nabla m| \gg e_{\text{grad}} \implies \text{term} \approx 1$ (only for the cells right at the contact).
* **Across a wide, smooth transition ramp**: Many cells have small non-zero gradients $|\nabla m| > e_{\text{grad}}$. Each cell adds $\approx 1$, creating a massive penalty!

##### The Geological Insight
Minimizing $S_{MGS}$ penalizes diffuse gradient transition zones. It forces property changes to occur across **the minimum possible boundary surface area**, creating **razor-sharp, piecewise-constant blocky geology** (such as fault contacts, dykes, and layered formations).

---

#### 4. Visual Comparison of the Three Approaches

```text
TRUE GEOLOGY               TIKHONOV L2                ZHDANOV S_MS & S_MGS
(Sharp ore body)           (Smooth blur)              (Focused inversion)

    Density                    Density                    Density
   ^                          ^                          ^
   |       +-------+          |         .---.            |       +-------+
   |       |       |          |        /     \           |       |       |
   |       |       |          |       /       \          |       |       |
0.0+-------+-------+-->    0.0+------'         '-->   0.0+-------+-------+-->
   0       X1      X2         0       X1      X2         0       X1      X2

- Crisp contacts           - Spread over space        - Crisp boundaries
- Uniform core             - Underestimates peak      - True amplitude preserved
                           - Blurry halos             - Zero background noise
```

---

#### 5. The Critical Role of Threshold $e$ & Curriculum Annealing

The parameter $e$ controls how "sharp" the focusing is:
* **When $e$ is large ($e = 0.5\text{–}1.0$)**:
  $$\frac{x^2}{x^2 + e^2} \approx \frac{x^2}{e^2} \propto x^2$$
  The functional behaves like standard quadratic $L_2$ Tikhonov. Gradients are smooth and stable.
* **When $e$ is small ($e = 0.001$)**:
  The functional acts as a sharp step function ($L_0$ counting).

##### Why We Use Annealing in `train.py`
If training starts with $e = 0.001$ immediately, gradients are too non-linear and the network gets trapped in poor local minima. In `.inv/python/train.py`, we implement an **annealing curriculum**:
1. **Epochs 1–10 (Structural Warmup, $e = 0.10$)**: Smooth $L_2$-like regime to guide global location and mass of structures.
2. **Epochs 11–25 (Focusing Activation, $e: 0.10 \to 0.01$)**: Focusing activates; diffuse halos contract into compact cores ($S_{MS}$ drops).
3. **Epochs 26–35 (Contact Sharpening, $e: 0.01 \to 0.001$)**: Minimum Gradient Support ($S_{MGS}$) takes over, locking in razor-sharp geological contacts.

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

#### How to Interpret Focusing Metrics in Training Logs

During training, `train.py` outputs live metrics for each epoch and mini-batch step:
* **`Loss` decreasing ($0.184 \to 0.018$)**: Measures overall objective convergence across supervised priors, forward physics data fit, and focusing regularizers.
* **`S_MS` decreasing ($0.241 \to 0.034$)**: Confirms that the model has stopped predicting diffuse density clouds across the volume and concentrated anomalous mass into compact bodies (occupying $\approx 3.4\%$ of the domain).
* **`S_MGS` decreasing ($0.312 \to 0.045$)**: Confirms that fuzzy boundary gradients are disappearing, replaced by razor-sharp, blocky lithological contacts and fault interfaces.
* **`Focus e` decaying ($0.100 \to 0.001$)**: Verifies that the curriculum schedule is actively transitioning the network from smooth global convergence to sharp $L_0$-proximal focusing.

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
