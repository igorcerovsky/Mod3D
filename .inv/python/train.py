"""Train the 3D gravity-inversion transformer on isolated bodies / cavities below random terrain.

Usage:
    python train.py --gui                             # live macOS GUI window + graphs every epoch
    python train.py                                   # saves checkpoints/training_monitor.png + dashboard.html
    python train.py --epochs 2 --steps 20 --n-val 8   # smoke test (~1 min)
    python export_onnx.py --checkpoint checkpoints/best_swin3d.pt

Data is generated ON THE FLY (infinite, never repeats): geometry in DataLoader workers (numpy),
exact draped-station forward model batched on the training device (~2 ms/sample on MPS).
Validation uses a FIXED seeded set; the best checkpoint is chosen by the validation loss.
"""

import argparse
import json
import os
import time

import numpy as np
import torch
from torch.utils.data import DataLoader

from dataset import CELL, NX, NY, NZ, BodyTerrainDataset, augment_d4, simulate_batch, terrain_channel
from evaluate import compute_metrics, make_category_showcase, make_fixed_set
from forward import TerrainGravityOperator
from losses import CompoundInversionLoss
from model_swin3d import PotentialFieldTransformer3D, PotentialFieldTransformer3DV3, create_model

ARCH = {"variant": "v3", "embed_dim": 128, "num_heads": 4, "depth": 6, "ch_mid": 40, "ch_vox": 24,
        "n_sub": 3, "num_classes": 7}


class ModelEMA:
    """Exponential moving average weights with early-step decay warmup."""

    def __init__(self, model: torch.nn.Module, decay: float, create_fn, initial_updates: int = 0):
        self.decay = float(decay)
        self.updates = int(initial_updates)
        self.ema = create_fn().eval() if self.decay > 0.0 else None
        if self.ema is not None:
            self.ema.load_state_dict(model.state_dict())
            for p in self.ema.parameters():
                p.requires_grad_(False)

    @torch.no_grad()
    def update(self, model: torch.nn.Module) -> None:
        if self.ema is None:
            return
        self.updates += 1
        d = min(self.decay, (1.0 + self.updates) / (10.0 + self.updates))
        for ev, mv in zip(self.ema.state_dict().values(), model.state_dict().values()):
            if ev.is_floating_point():
                ev.mul_(d).add_(mv.detach(), alpha=1.0 - d)
            else:
                ev.copy_(mv)


def parse_args():
    p = argparse.ArgumentParser(description="Train the 3D gravity-inversion transformer")
    p.add_argument("--epochs", type=int, default=20)
    p.add_argument("--steps", type=int, default=200, help="optimizer steps per epoch")
    p.add_argument("--batch-size", type=int, default=8)
    p.add_argument("--lr", type=float, default=5e-4)
    p.add_argument("--n-val", type=int, default=64)
    p.add_argument("--hard-prob", type=float, default=0.50,
                   help="fraction of training samples using harder/larger composite & contact scenes [0.0-1.0]")
    p.add_argument("--num-workers", type=int, default=0)
    p.add_argument("--lambda-data", type=float, default=0.25)
    p.add_argument("--lambda-ms", type=float, default=0.05)
    p.add_argument("--lambda-cls", type=float, default=0.25, help="lithology cross-entropy weight")
    p.add_argument("--depth-boost", type=float, default=0.5, help="depth sensitivity ramp in loss [0.0 = uniform]")
    p.add_argument("--ema-decay", type=float, default=0.995, help="EMA decay for eval/best weights (0 disables)")
    p.add_argument("--save-dir", type=str, default="./checkpoints")
    p.add_argument("--resume", type=str, default="", help="continue from a checkpoint")
    p.add_argument("--gui", action="store_true", help="open a live native GUI window with training graphs")
    default_dev = "cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu")
    p.add_argument("--device", type=str, default=default_dev)
    return p.parse_args()


def build_dashboard_html():
    return """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Mod3D .inv — Interactive Voxel Training Monitor</title>
<style>
  :root {
    --bg-dark: #0b0f19;
    --card-bg: #131c2e;
    --control-bg: #1a2538;
    --border: #24324d;
    --text-primary: #f8fafc;
    --text-secondary: #94a3b8;
    --accent: #38bdf8;
    --accent-glow: rgba(56, 189, 248, 0.4);
    --green: #4ade80;
    --amber: #fbbf24;
    --red: #f87171;
    --purple: #c084fc;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background-color: var(--bg-dark);
    color: var(--text-primary);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    padding: 8px 14px;
    display: flex;
    flex-direction: column;
    align-items: center;
    min-height: 100vh;
  }
  header {
    width: 100%;
    max-width: 1360px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 8px;
    margin-bottom: 8px;
    padding-bottom: 6px;
    border-bottom: 1px solid var(--border);
  }
  h1 { font-size: 1.05rem; font-weight: 700; color: var(--accent); display: flex; align-items: center; gap: 8px; }
  h1 span.tag { font-size: 0.65rem; padding: 2px 6px; border-radius: 4px; background: rgba(56,189,248,0.15); color: var(--accent); border: 1px solid rgba(56,189,248,0.3); font-weight: 600; text-transform: uppercase; }
  .status-badge {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    background: #022c22;
    color: var(--green);
    border: 1px solid #065f46;
    padding: 2px 8px;
    border-radius: 9999px;
    font-size: 0.72rem;
    font-weight: 600;
  }
  .status-dot {
    width: 6px;
    height: 6px;
    background: var(--green);
    border-radius: 50%;
    box-shadow: 0 0 6px var(--green);
    animation: pulse 1.8s infinite;
  }
  @keyframes pulse { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(0.8); } }

  /* Toolbar */
  .controls-panel {
    width: 100%;
    max-width: 1360px;
    background: var(--control-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 6px 12px;
    margin-bottom: 8px;
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 10px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.35);
    position: sticky;
    top: 6px;
    z-index: 100;
  }
  .btn-group { display: flex; align-items: center; gap: 3px; }
  .playback-btn {
    background: var(--card-bg);
    color: var(--text-primary);
    border: 1px solid var(--border);
    border-radius: 5px;
    padding: 4px 9px;
    font-size: 0.76rem;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s;
    min-height: 26px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
  }
  .playback-btn:hover { background: #24324d; border-color: var(--accent); }
  .playback-btn:active { transform: translateY(1px); }
  .playback-btn.active { background: #0284c7; color: #fff; border-color: #38bdf8; }
  .slider-group {
    display: flex;
    align-items: center;
    gap: 8px;
    flex: 1;
    min-width: 180px;
  }
  input[type="range"] {
    flex: 1;
    -webkit-appearance: none;
    appearance: none;
    background: #24324d;
    height: 6px;
    border-radius: 3px;
    outline: none;
    cursor: pointer;
  }
  input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 14px;
    height: 14px;
    border-radius: 50%;
    background: var(--accent);
    cursor: pointer;
    box-shadow: 0 0 6px var(--accent-glow);
    transition: transform 0.1s, background 0.1s;
  }
  input[type="range"]::-webkit-slider-thumb:hover { transform: scale(1.2); background: #7dd3fc; }
  input[type="range"]::-moz-range-thumb {
    width: 14px;
    height: 14px;
    border: none;
    border-radius: 50%;
    background: var(--accent);
    cursor: pointer;
    box-shadow: 0 0 6px var(--accent-glow);
  }
  .epoch-label {
    font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
    font-size: 0.8rem;
    font-weight: 700;
    color: var(--accent);
    background: #0f172a;
    border: 1px solid var(--border);
    padding: 2px 8px;
    border-radius: 5px;
    white-space: nowrap;
  }
  .toggle-group {
    display: flex;
    align-items: center;
    gap: 10px;
    font-size: 0.78rem;
    color: var(--text-secondary);
    user-select: none;
  }
  .toggle-group label { display: flex; align-items: center; gap: 4px; cursor: pointer; }
  select {
    background: var(--card-bg);
    color: var(--text-primary);
    border: 1px solid var(--border);
    padding: 3px 6px;
    border-radius: 5px;
    font-size: 0.76rem;
    cursor: pointer;
    outline: none;
  }

  /* Metrics Grid */
  .metrics-grid {
    width: 100%;
    max-width: 1360px;
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(110px, 1fr));
    gap: 6px;
    margin-bottom: 8px;
  }
  .metric-card {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 4px 8px;
    text-align: center;
  }
  .metric-name {
    font-size: 0.62rem;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-secondary);
    margin-bottom: 2px;
    font-weight: 600;
  }
  .metric-value {
    font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
    font-size: 0.95rem;
    font-weight: 700;
    color: var(--text-primary);
  }

  /* Voxel Canvas Display */
  .canvas-container {
    width: 100%;
    max-width: 1360px;
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 8px 12px;
    margin-bottom: 8px;
    box-shadow: 0 4px 14px rgba(0,0,0,0.3);
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .profile-bar-card {
    background: #0f172a;
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 4px 8px;
  }
  .section-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 6px;
    margin-bottom: 3px;
  }
  .section-title {
    font-size: 0.76rem;
    font-weight: 600;
    color: var(--text-secondary);
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .section-title span.val { color: var(--accent); font-family: ui-monospace, monospace; }
  .hover-pill {
    font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
    font-size: 0.72rem;
    padding: 2px 8px;
    background: #1e293b;
    border: 1px solid var(--border);
    border-radius: 4px;
    color: var(--accent);
  }
  canvas.profile-canvas {
    width: 100%;
    height: 38px;
    display: block;
    background: #0f172a;
    border-radius: 4px;
  }
  .voxel-grid-card {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    width: 100%;
  }
  @media (max-width: 900px) { .voxel-grid-card { grid-template-columns: 1fr; } }
  .overview-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 8px;
    width: 100%;
  }
  .overview-grid.grid-16 { gap: 6px; }
  @media (max-width: 1100px) { .overview-grid { grid-template-columns: repeat(2, 1fr); } }
  @media (max-width: 600px) { .overview-grid { grid-template-columns: 1fr; } }
  .overview-grid.grid-16 .sample-card-item { padding: 5px; gap: 3px; }
  .overview-grid.grid-16 .sample-card-header { font-size: 0.68rem; }
  .overview-grid.grid-16 .sample-card-header span.tag-pill { font-size: 0.64rem; }
  .sample-card-item {
    background: #0f172a;
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 6px;
    display: flex;
    flex-direction: column;
    gap: 4px;
    cursor: pointer;
    transition: border-color 0.15s, transform 0.1s;
  }
  .sample-card-item:hover { border-color: var(--accent); transform: translateY(-1px); }
  .sample-card-item.selected { border-color: var(--accent); box-shadow: 0 0 8px rgba(56, 189, 248, 0.4); }
  .sample-card-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 0.72rem;
    font-weight: 600;
    color: var(--text-secondary);
  }
  .sample-card-header span.tag-pill {
    color: var(--accent);
    font-family: ui-monospace, monospace;
    font-size: 0.68rem;
  }
  .sample-subpair {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 4px;
  }
  .sample-subcol {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .sample-subcol-title {
    font-size: 0.62rem;
    color: var(--text-secondary);
    text-align: center;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  canvas.sample-mini-canvas {
    width: 100%;
    aspect-ratio: 2 / 1;
    image-rendering: pixelated;
    border-radius: 3px;
    background: #000;
  }
  .canvas-panel {
    background: #0f172a;
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 6px 8px;
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  .canvas-panel h3 {
    font-size: 0.76rem;
    font-weight: 600;
    margin-bottom: 4px;
    color: var(--text-secondary);
    width: 100%;
    text-align: left;
    display: flex;
    justify-content: space-between;
  }
  .canvas-panel h3 span.badge {
    color: var(--accent);
    font-family: ui-monospace, monospace;
    font-weight: 700;
  }
  canvas.voxel-canvas {
    width: 100%;
    max-height: 175px;
    aspect-ratio: 2 / 1;
    image-rendering: pixelated;
    border-radius: 4px;
    background: #000;
    cursor: crosshair;
  }
  .colorbar-row {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
    font-size: 0.7rem;
    color: var(--text-secondary);
    margin-top: 2px;
  }
  canvas#cvsColorbar { border-radius: 3px; border: 1px solid var(--border); }

  /* Charts */
  .charts-grid {
    width: 100%;
    max-width: 1360px;
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 8px;
    margin-bottom: 6px;
  }
  .chart-card {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 6px 10px;
  }
  .chart-card h4 {
    font-size: 0.72rem;
    color: var(--text-secondary);
    font-weight: 600;
    margin-bottom: 4px;
    display: flex;
    justify-content: space-between;
  }
  svg.chart-svg {
    width: 100%;
    height: 85px;
    display: block;
    cursor: pointer;
  }

  .shortcut-footer {
    font-size: 0.68rem;
    color: var(--text-secondary);
    margin-top: 2px;
    margin-bottom: 8px;
    text-align: center;
  }
  kbd {
    background: #1e293b;
    border: 1px solid var(--border);
    border-radius: 3px;
    padding: 1px 4px;
    font-size: 0.65rem;
    font-family: inherit;
  }
</style>
</head>
<body>

<header>
  <h1>Mod3D .inv &mdash; Swin 3D Voxel Inversion Monitor <span class="tag">Direct Numerical Voxel Rendering</span></h1>
  <div class="status-badge"><span class="status-dot"></span> <span id="statusText">LIVE DATA STREAM</span></div>
</header>

<div class="controls-panel">
  <div class="btn-group">
    <button id="btnFirst" class="playback-btn" title="First Epoch (Home)">&laquo; First</button>
    <button id="btnPrev" class="playback-btn" title="Previous Epoch (&larr;)">&lsaquo; Prev</button>
    <button id="btnPlay" class="playback-btn" title="Play/Pause Timelapse (Space)">&#9654; Play</button>
    <button id="btnNext" class="playback-btn" title="Next Epoch (&rarr;)">Next &rsaquo;</button>
    <button id="btnLatest" class="playback-btn" title="Latest Epoch (End)">Latest &raquo;</button>
  </div>

  <div class="slider-group">
    <span style="font-size:0.85rem;color:var(--text-secondary);font-weight:600;">Epoch</span>
    <input type="range" id="epochSlider" min="1" max="1" value="1">
    <span class="epoch-label" id="epochDisplay">Epoch --/--</span>
  </div>

  <div class="slider-group" id="sampleSliderGroup">
    <span style="font-size:0.85rem;color:var(--text-secondary);font-weight:600;">Val Sample</span>
    <input type="range" id="sampleSlider" min="0" max="15" value="0">
    <span class="epoch-label" id="sampleDisplay" style="color:var(--accent);">Sample #0</span>
  </div>

  <select id="speedSelect" title="Playback speed">
    <option value="1">1 fps (slow)</option>
    <option value="2" selected>2 fps (normal)</option>
    <option value="4">4 fps (fast)</option>
    <option value="8">8 fps (turbo)</option>
  </select>

  <div class="toggle-group">
    <div style="display:flex;align-items:center;background:#0f172a;border:1px solid var(--border);border-radius:6px;padding:2px 4px;gap:2px;" title="Number of validation samples displayed">
      <label style="cursor:pointer;padding:2px 7px;border-radius:4px;" id="lblView1" title="Single side-by-side view with Truth, Prediction, and Difference">
        <input type="radio" name="viewMode" value="1" id="radView1" checked style="cursor:pointer;"> 1
      </label>
      <label style="cursor:pointer;padding:2px 7px;border-radius:4px;" id="lblView8" title="8 validation sample pairs (use Val Sample slider to flip #0-#7 vs #8-#15)">
        <input type="radio" name="viewMode" value="8" id="radView8" style="cursor:pointer;"> 8
      </label>
      <label style="cursor:pointer;padding:2px 7px;border-radius:4px;" id="lblView16" title="All 16 validation & category showcase pairs at once">
        <input type="radio" name="viewMode" value="16" id="radView16" style="cursor:pointer;"> 16
      </label>
    </div>
    <label title="Automatically advance to new epochs during training">
      <input type="checkbox" id="chkFollowLive" checked> Follow Live
    </label>
    <label title="Toggle residual difference map (Predicted - Ground Truth)" id="lblDiffToggle">
      <input type="checkbox" id="chkShowDiff"> Difference Map
    </label>
  </div>
</div>

<div class="metrics-grid">
  <div class="metric-card"><div class="metric-name">Train Loss</div><div class="metric-value" id="mTrainLoss">--</div></div>
  <div class="metric-card"><div class="metric-name">Val Loss</div><div class="metric-value" id="mValLoss">--</div></div>
  <div class="metric-card"><div class="metric-name">Data Misfit</div><div class="metric-value" id="mDataMisfit" style="color:var(--purple);">--</div></div>
  <div class="metric-card"><div class="metric-name">3D IoU (50%)</div><div class="metric-value" id="mIoU" style="color:var(--green);">--</div></div>
  <div class="metric-card"><div class="metric-name">Centroid dz</div><div class="metric-value" id="mCentroidDz" style="color:var(--red);">--</div></div>
  <div class="metric-card"><div class="metric-name">Amp Ratio</div><div class="metric-value" id="mAmpRatio" style="color:var(--accent);">--</div></div>
  <div class="metric-card"><div class="metric-name">Opp Halo</div><div class="metric-value" id="mHaloOpp" style="color:var(--amber);">--</div></div>
</div>

<div class="canvas-container">
  <div class="profile-bar-card">
    <div class="section-header">
      <div class="section-title">Surface Gravity Anomaly gz [mGal] along Profile <span class="val" id="profileYVal">Y = -- km</span></div>
      <div id="profileHoverVal" class="hover-pill">Move mouse over cross-section to inspect coordinates</div>
    </div>
    <canvas id="cvsProfile" class="profile-canvas" width="1280" height="70"></canvas>
  </div>

  <div class="section-header">
    <div class="section-title">Subsurface Density Contrast Cross-Sections <span class="val" id="gridDimVal">64 &times; 32 Voxels</span></div>
    <div id="voxelHoverPill" class="hover-pill">Hover over any voxel cell to inspect numerical values</div>
  </div>

  <div class="voxel-grid-card" id="voxelCardGrid">
    <div class="canvas-panel">
      <h3>Ground Truth Density Contrast <span class="badge">&plusmn;True</span></h3>
      <canvas id="cvsTruth" class="voxel-canvas" width="640" height="320"></canvas>
    </div>
    <div class="canvas-panel">
      <h3>Swin-3D Inversion Prediction <span class="badge" id="predEpochBadge">Epoch --</span></h3>
      <canvas id="cvsPred" class="voxel-canvas" width="640" height="320"></canvas>
    </div>
    <div class="canvas-panel" id="diffPanel" style="display:none;grid-column:1 / -1;">
      <h3>Inversion Residual Error (Predicted &minus; Ground Truth) <span class="badge">&Delta;&rho; Error</span></h3>
      <canvas id="cvsDiff" class="voxel-canvas" width="640" height="320" style="max-height:220px;"></canvas>
    </div>
  </div>

  <div class="overview-grid" id="overviewGrid" style="display:none;"></div>

  <div class="colorbar-row">
    <span id="cbarMin">-0.50</span>
    <canvas id="cvsColorbar" width="260" height="12"></canvas>
    <span id="cbarMax">+0.50</span>
    <span style="margin-left:6px;color:var(--text-secondary);">g/cm&sup3; (Blue: Cavity / Negative contrast &bull; White: 0 &bull; Red: Dense rock)</span>
  </div>
</div>

<div class="charts-grid">
  <div class="chart-card">
    <h4>
      <span>Loss &amp; Data Misfit</span>
      <span style="display:flex;align-items:center;gap:6px;">
        <span style="display:inline-flex;align-items:center;background:#0f172a;border:1px solid var(--border);border-radius:4px;padding:1px 2px;gap:2px;font-size:0.68rem;">
          <label id="lblLossLog" style="cursor:pointer;padding:1px 5px;border-radius:3px;background:#0284c7;color:#fff;" title="Logarithmic Y axis">
            <input type="radio" name="lossScale" value="log" id="radLossLog" checked style="display:none;">Log
          </label>
          <label id="lblLossLin" style="cursor:pointer;padding:1px 5px;border-radius:3px;" title="Linear Y axis">
            <input type="radio" name="lossScale" value="lin" id="radLossLin" style="display:none;">Lin
          </label>
        </span>
        <span style="font-size:0.72rem;color:var(--text-secondary);">Click to seek</span>
      </span>
    </h4>
    <svg id="svgLoss" class="chart-svg"></svg>
  </div>
  <div class="chart-card">
    <h4>3D Volumetric IoU &amp; Amplitude Recovery <span style="font-size:0.75rem;color:var(--text-secondary);">Click chart to seek</span></h4>
    <svg id="svgIoU" class="chart-svg"></svg>
  </div>
  <div class="chart-card">
    <h4>Centroid Depth Error |dz| [m] <span style="font-size:0.75rem;color:var(--text-secondary);">Click chart to seek</span></h4>
    <svg id="svgDz" class="chart-svg"></svg>
  </div>
</div>

<div class="shortcut-footer">
  Keyboard shortcuts: <kbd>&larr;</kbd> / <kbd>&rarr;</kbd> step epochs &bull; <kbd>Space</kbd> play/pause &bull; <kbd>Home</kbd> / <kbd>End</kbd> first/latest &bull; Hover over any voxel for exact density values
</div>

<script src="training_history.js"></script>
<script>
(function() {
  function normalizeTrainingData(raw) {
    if (!raw) return { meta: null, epochs: [] };
    if (!Array.isArray(raw)) {
      if (raw.epochs && Array.isArray(raw.epochs)) return raw;
      return { meta: null, epochs: [] };
    }
    const epochs = raw;
    let sliceEntry = null;
    for (let i = epochs.length - 1; i >= 0; --i) {
      if (epochs[i].slice) { sliceEntry = epochs[i].slice; break; }
    }
    const truthMatrix = sliceEntry ? (sliceEntry.truth || sliceEntry.truth_slice || []) : [];
    const meta = {
      nx: (truthMatrix.length && truthMatrix[0]) ? truthMatrix[0].length : 64,
      ny: 64,
      nz: truthMatrix.length ? truthMatrix.length : 32,
      cell: 50.0,
      iy: sliceEntry ? (sliceEntry.iy != null ? sliceEntry.iy : 30) : 30,
      y_km: sliceEntry ? (sliceEntry.y_km != null ? sliceEntry.y_km : 1.5) : 1.5,
      vmax: sliceEntry ? (sliceEntry.vmax != null ? sliceEntry.vmax : 0.6) : 0.6,
      truth_slice: truthMatrix,
      terrain: sliceEntry ? (sliceEntry.terrain || []) : [],
      obs_profile: []
    };
    epochs.forEach(ep => {
      if (ep.slice && ep.slice.pred && !ep.pred_slice) {
        ep.pred_slice = ep.slice.pred;
      }
    });
    return { meta, epochs };
  }

  let trainingData = normalizeTrainingData(window.TRAINING_DATA || window.TRAINING_HISTORY || null);
  let currentIndex = -1;
  let currentSampleIndex = 0;
  let viewMode = '1'; // '1', '8', or '16'
  let lossLogScale = true;
  let isPlaying = false;
  let playTimer = null;
  let hoverCell = null;

  const slider = document.getElementById('epochSlider');
  const epochDisplay = document.getElementById('epochDisplay');
  const sampleSlider = document.getElementById('sampleSlider');
  const sampleDisplay = document.getElementById('sampleDisplay');
  const sampleSliderGroup = document.getElementById('sampleSliderGroup');
  const radView1 = document.getElementById('radView1');
  const radView8 = document.getElementById('radView8');
  const radView16 = document.getElementById('radView16');
  const lblView1 = document.getElementById('lblView1');
  const lblView8 = document.getElementById('lblView8');
  const lblView16 = document.getElementById('lblView16');
  const lblDiffToggle = document.getElementById('lblDiffToggle');
  const voxelCardGrid = document.getElementById('voxelCardGrid');
  const overviewGrid = document.getElementById('overviewGrid');
  const btnFirst = document.getElementById('btnFirst');
  const btnPrev = document.getElementById('btnPrev');
  const btnPlay = document.getElementById('btnPlay');
  const btnNext = document.getElementById('btnNext');
  const btnLatest = document.getElementById('btnLatest');
  const speedSelect = document.getElementById('speedSelect');
  const chkFollowLive = document.getElementById('chkFollowLive');
  const chkShowDiff = document.getElementById('chkShowDiff');
  const diffPanel = document.getElementById('diffPanel');
  const predEpochBadge = document.getElementById('predEpochBadge');
  const voxelHoverPill = document.getElementById('voxelHoverPill');
  const profileHoverVal = document.getElementById('profileHoverVal');
  const profileYVal = document.getElementById('profileYVal');
  const gridDimVal = document.getElementById('gridDimVal');
  const cbarMin = document.getElementById('cbarMin');
  const cbarMax = document.getElementById('cbarMax');

  // Canvases
  const cvsProfile = document.getElementById('cvsProfile');
  const cvsTruth = document.getElementById('cvsTruth');
  const cvsPred = document.getElementById('cvsPred');
  const cvsDiff = document.getElementById('cvsDiff');
  const cvsColorbar = document.getElementById('cvsColorbar');

  function init() {
    setupControls();
    drawColorbar();
    syncData(trainingData);
    setInterval(pollData, 3000);
  }

  function setupControls() {
    slider.addEventListener('input', e => {
      stopPlayback();
      showEpoch(parseInt(e.target.value, 10) - 1);
    });

    if (sampleSlider) {
      sampleSlider.addEventListener('input', e => {
        showSample(parseInt(e.target.value, 10));
      });
    }

    function setViewMode(mode) {
      if (mode === 'single') mode = '1';
      else if (mode === 'all8') mode = '8';
      viewMode = (mode === '8' || mode === '16') ? mode : '1';
      const isGrid = viewMode !== '1';
      voxelCardGrid.style.display = isGrid ? 'none' : 'grid';
      overviewGrid.style.display = isGrid ? 'grid' : 'none';
      overviewGrid.classList.toggle('grid-16', viewMode === '16');

      const meta = getMeta();
      const count = (meta && meta.samples && meta.samples.length) ? meta.samples.length : 1;
      updateSampleSliderVisibility(count);
      if (lblDiffToggle) lblDiffToggle.style.opacity = isGrid ? '0.35' : '1';

      for (const [m, rad, lbl] of [['1', radView1, lblView1], ['8', radView8, lblView8], ['16', radView16, lblView16]]) {
        const active = viewMode === m;
        if (rad) rad.checked = active;
        if (lbl) {
          lbl.style.background = active ? '#0284c7' : 'transparent';
          lbl.style.color = active ? '#fff' : 'inherit';
        }
      }
      renderCurrentVoxelData();
    }

    if (radView1) radView1.addEventListener('change', () => setViewMode('1'));
    if (radView8) radView8.addEventListener('change', () => setViewMode('8'));
    if (radView16) radView16.addEventListener('change', () => setViewMode('16'));
    setViewMode(viewMode);

    const radLossLog = document.getElementById('radLossLog');
    const radLossLin = document.getElementById('radLossLin');
    const lblLossLog = document.getElementById('lblLossLog');
    const lblLossLin = document.getElementById('lblLossLin');
    function setLossScale(useLog) {
      lossLogScale = useLog;
      if (lblLossLog) { lblLossLog.style.background = useLog ? '#0284c7' : 'transparent'; lblLossLog.style.color = useLog ? '#fff' : 'inherit'; }
      if (lblLossLin) { lblLossLin.style.background = useLog ? 'transparent' : '#0284c7'; lblLossLin.style.color = useLog ? 'inherit' : '#fff'; }
      renderAllCharts();
    }
    if (radLossLog) radLossLog.addEventListener('change', () => setLossScale(true));
    if (radLossLin) radLossLin.addEventListener('change', () => setLossScale(false));

    btnFirst.addEventListener('click', () => { stopPlayback(); showEpoch(0); });
    btnPrev.addEventListener('click', () => { stopPlayback(); if (currentIndex > 0) showEpoch(currentIndex - 1); });
    btnNext.addEventListener('click', () => { stopPlayback(); if (currentIndex < getEpochs().length - 1) showEpoch(currentIndex + 1); });
    btnLatest.addEventListener('click', () => { stopPlayback(); showEpoch(getEpochs().length - 1); });
    btnPlay.addEventListener('click', togglePlay);

    chkShowDiff.addEventListener('change', () => {
      diffPanel.style.display = chkShowDiff.checked ? 'block' : 'none';
      if (currentIndex >= 0) renderCurrentVoxelData();
    });

    [cvsTruth, cvsPred, cvsDiff, cvsProfile].forEach(cvs => {
      cvs.addEventListener('mousemove', onCanvasHover);
      cvs.addEventListener('mouseleave', () => {
        hoverCell = null;
        voxelHoverPill.textContent = "Hover over any voxel cell to inspect numerical values";
        profileHoverVal.textContent = "Move mouse over cross-section to inspect coordinates";
        renderCurrentVoxelData();
        renderProfileCanvas();
      });
    });

    window.addEventListener('keydown', e => {
      if (e.target.tagName === 'INPUT' && e.target.type !== 'range') return;
      if (e.key === 'ArrowLeft' || e.key === 'a' || e.key === 'A') {
        e.preventDefault(); stopPlayback();
        if (currentIndex > 0) showEpoch(currentIndex - 1);
      } else if (e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') {
        e.preventDefault(); stopPlayback();
        if (currentIndex < getEpochs().length - 1) showEpoch(currentIndex + 1);
      } else if (e.key === ' ') {
        e.preventDefault(); togglePlay();
      } else if (e.key === 'Home') {
        e.preventDefault(); stopPlayback(); showEpoch(0);
      } else if (e.key === 'End') {
        e.preventDefault(); stopPlayback(); showEpoch(getEpochs().length - 1);
      }
    });
  }

  function getEpochs() {
    if (!trainingData) return [];
    return trainingData.epochs || [];
  }

  function getMeta() {
    if (!trainingData) return null;
    return trainingData.meta || null;
  }

  function getActiveSampleMeta() {
    const meta = getMeta();
    if (!meta) return null;
    if (meta.samples && Array.isArray(meta.samples) && meta.samples.length > 0) {
      const idx = Math.max(0, Math.min(currentSampleIndex, meta.samples.length - 1));
      return meta.samples[idx];
    }
    return meta;
  }

  function formatSampleLabel(idx, sampleMeta) {
    return (sampleMeta && sampleMeta.tag) ? `#${idx} \u00b7 ${sampleMeta.tag}` : `Sample #${idx}`;
  }

  function updateSampleSliderVisibility(sampleCount) {
    if (!sampleSliderGroup) return;
    const show = (viewMode === '1' && sampleCount > 1) || (viewMode === '8' && sampleCount > 8);
    sampleSliderGroup.style.display = show ? 'flex' : 'none';
  }

  function showSample(idx) {
    const meta = getMeta();
    const samples = (meta && meta.samples && meta.samples.length) ? meta.samples : null;
    const count = samples ? samples.length : 1;
    currentSampleIndex = Math.max(0, Math.min(idx, count - 1));
    if (sampleSlider) sampleSlider.value = currentSampleIndex;
    const activeSm = samples ? samples[currentSampleIndex] : meta;
    if (sampleDisplay) sampleDisplay.textContent = formatSampleLabel(currentSampleIndex, activeSm);
    updateMetaDisplay();
    renderCurrentVoxelData();
    renderProfileCanvas();
  }

  function updateMetaDisplay() {
    const meta = getMeta();
    const sm = getActiveSampleMeta();
    if (sm) {
      if (profileYVal) profileYVal.textContent = sm.tag ? `Y = ${sm.y_km} km (${sm.tag})` : `Y = ${sm.y_km} km`;
      if (gridDimVal) gridDimVal.textContent = `${meta ? meta.nx : 64} \u00d7 ${meta ? meta.nz : 32} Voxels (dx = ${meta ? meta.cell : 50} m, dz = ${meta ? meta.cell : 50} m)`;
      const vm = sm.vmax || 0.6;
      if (cbarMin) cbarMin.textContent = `-${vm.toFixed(2)}`;
      if (cbarMax) cbarMax.textContent = `+${vm.toFixed(2)}`;
    }
  }

  function syncData(data) {
    if (!data) return;
    trainingData = normalizeTrainingData(data);
    const epochs = getEpochs();
    const meta = getMeta();
    if (!epochs || epochs.length === 0) return;

    slider.min = 1;
    slider.max = epochs.length;

    if (sampleSlider && sampleSliderGroup) {
      if (meta && meta.samples && meta.samples.length > 1) {
        sampleSlider.min = 0;
        sampleSlider.max = meta.samples.length - 1;
        if (currentSampleIndex >= meta.samples.length) currentSampleIndex = 0;
        sampleSlider.value = currentSampleIndex;
        if (sampleDisplay) sampleDisplay.textContent = formatSampleLabel(currentSampleIndex, meta.samples[currentSampleIndex]);
        updateSampleSliderVisibility(meta.samples.length);
      } else {
        updateSampleSliderVisibility(1);
      }
    }

    updateMetaDisplay();

    renderAllCharts();

    if (currentIndex === -1 || chkFollowLive.checked) {
      showEpoch(epochs.length - 1);
    } else {
      showEpoch(currentIndex);
    }
  }

  function pollData() {
    if (location.protocol.startsWith('http')) {
      fetch('training_history.json?t=' + Date.now())
        .then(r => r.json())
        .then(d => syncData(d))
        .catch(() => reloadScript());
    } else {
      reloadScript();
    }
  }

  function reloadScript() {
    const old = document.getElementById('histDynScript');
    if (old) old.remove();
    const s = document.createElement('script');
    s.id = 'histDynScript';
    s.src = 'training_history.js?t=' + Date.now();
    s.onload = () => {
      const d = window.TRAINING_DATA || window.TRAINING_HISTORY;
      if (d) syncData(d);
    };
    document.head.appendChild(s);
  }

  function showEpoch(idx) {
    const epochs = getEpochs();
    if (!epochs || epochs.length === 0) {
      epochDisplay.textContent = 'Epoch --/--';
      return;
    }
    idx = Math.max(0, Math.min(idx, epochs.length - 1));
    currentIndex = idx;
    slider.value = idx + 1;
    const entry = epochs[idx];
    const ep = entry.epoch || (idx + 1);
    epochDisplay.textContent = `Epoch ${ep}/${epochs[epochs.length - 1].epoch || epochs.length}`;
    if (predEpochBadge) predEpochBadge.textContent = `Epoch ${ep}`;

    // Update KPI cards
    document.getElementById('mTrainLoss').textContent = entry.train_loss != null ? entry.train_loss.toFixed(3) : '--';
    document.getElementById('mValLoss').textContent = entry.val_loss != null ? entry.val_loss.toFixed(3) : '--';
    document.getElementById('mDataMisfit').textContent = entry.data_rel_misfit != null ? (entry.data_rel_misfit * 100).toFixed(1) + '%' : '--';
    document.getElementById('mIoU').textContent = entry.iou != null ? (entry.iou * 100).toFixed(1) + '%' : '--';
    document.getElementById('mCentroidDz').textContent = entry.centroid_dz != null ? Math.round(entry.centroid_dz) + ' m' : '--';
    document.getElementById('mAmpRatio').textContent = entry.amp_ratio != null ? entry.amp_ratio.toFixed(2) : '--';
    document.getElementById('mHaloOpp').textContent = entry.halo_opp != null ? entry.halo_opp.toFixed(3) : '--';

    renderCurrentVoxelData();
    renderProfileCanvas();
    updateChartScrubbers(currentIndex);
  }

  function togglePlay() {
    isPlaying = !isPlaying;
    btnPlay.innerHTML = isPlaying ? '&#10074;&#10074; Pause' : '&#9654; Play';
    btnPlay.classList.toggle('active', isPlaying);
    if (isPlaying) {
      const epochs = getEpochs();
      if (currentIndex >= epochs.length - 1) currentIndex = -1;
      const fps = parseFloat(speedSelect.value) || 2;
      playTimer = setInterval(() => {
        const epList = getEpochs();
        if (currentIndex < epList.length - 1) {
          showEpoch(currentIndex + 1);
        } else {
          stopPlayback();
        }
      }, 1000 / fps);
    } else {
      stopPlayback();
    }
  }

  function stopPlayback() {
    if (isPlaying) {
      isPlaying = false;
      btnPlay.innerHTML = '&#9654; Play';
      btnPlay.classList.remove('active');
      clearInterval(playTimer);
      playTimer = null;
    }
  }

  speedSelect.addEventListener('change', () => {
    if (isPlaying) { togglePlay(); togglePlay(); }
  });

  // --- Scientific Colormap & Canvas Drawing ---
  function getCoolwarmColor(val, vmax) {
    const u = Math.max(-1, Math.min(1, vmax > 0 ? val / vmax : 0));
    let r, g, b;
    if (u < 0) {
      const t = -u; // 0..1
      r = Math.round(245 * (1 - t) + 49 * t);
      g = Math.round(245 * (1 - t) + 54 * t);
      b = Math.round(245 * (1 - t) + 149 * t);
    } else {
      const t = u; // 0..1
      r = Math.round(245 * (1 - t) + 165 * t);
      g = Math.round(245 * (1 - t) + 0 * t);
      b = Math.round(245 * (1 - t) + 38 * t);
    }
    return `rgb(${r},${g},${b})`;
  }

  function drawColorbar() {
    const ctx = cvsColorbar.getContext('2d');
    const w = cvsColorbar.width, h = cvsColorbar.height;
    for (let x = 0; x < w; ++x) {
      const u = (x / (w - 1)) * 2 - 1; // -1 to +1
      ctx.fillStyle = getCoolwarmColor(u, 1.0);
      ctx.fillRect(x, 0, 1, h);
    }
  }

  function drawPlaceholder(canvas, msg) {
    const ctx = canvas.getContext('2d');
    const cw = canvas.width, ch = canvas.height;
    ctx.clearRect(0, 0, cw, ch);
    ctx.fillStyle = "#1e293b";
    ctx.fillRect(0, 0, cw, ch);
    ctx.strokeStyle = "rgba(255,255,255,0.08)";
    ctx.strokeRect(0, 0, cw, ch);
    ctx.fillStyle = "#94a3b8";
    ctx.font = "12px -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif";
    ctx.textAlign = "center";
    ctx.fillText(msg, cw / 2, ch / 2);
  }

  function renderCurrentVoxelData() {
    const meta = getMeta();
    const sm = getActiveSampleMeta();
    const epochs = getEpochs();
    if (!meta || !epochs.length || currentIndex < 0 || !epochs[currentIndex]) return;
    const entry = epochs[currentIndex];

    if (viewMode === '8' || viewMode === '16') {
      renderOverviewGrid(meta, entry);
      return;
    }

    const vmax = (sm && sm.vmax) || (entry.slice && entry.slice.vmax) || (meta && meta.vmax) || 0.6;
    const terrain = (sm && sm.terrain) || (entry.slice && entry.slice.terrain) || (meta && meta.terrain) || [];
    const truthSlice = (sm && sm.truth_slice) || (entry.slice && entry.slice.truth) || (meta && meta.truth_slice) || [];
    let predSlice = [];
    if (entry.pred_slices && Array.isArray(entry.pred_slices) && entry.pred_slices[currentSampleIndex]) {
      predSlice = entry.pred_slices[currentSampleIndex];
    } else if (currentSampleIndex === 0) {
      predSlice = entry.pred_slice || (entry.slice && entry.slice.pred) || [];
    }

    if (truthSlice.length) {
      drawVoxelSection(cvsTruth, truthSlice, terrain, vmax, hoverCell, false);
    } else {
      drawPlaceholder(cvsTruth, "No ground truth cross-section available");
    }

    if (predSlice.length) {
      drawVoxelSection(cvsPred, predSlice, terrain, vmax, hoverCell, false);
      if (chkShowDiff.checked && truthSlice.length) {
        const diffSlice = [];
        for (let iz = 0; iz < truthSlice.length; ++iz) {
          const row = [];
          for (let ix = 0; ix < truthSlice[0].length; ++ix) {
            row.push((predSlice[iz] ? predSlice[iz][ix] : 0) - truthSlice[iz][ix]);
          }
          diffSlice.push(row);
        }
        drawVoxelSection(cvsDiff, diffSlice, terrain, vmax * 0.7, hoverCell, true);
      }
    } else {
      const msg = (currentSampleIndex > 0)
        ? `No multi-sample predictions recorded for Sample #${currentSampleIndex} in Epoch ${entry.epoch || (currentIndex + 1)}`
        : `No slice recorded for Epoch ${entry.epoch || (currentIndex + 1)} (slices recorded from Epoch 61)`;
      drawPlaceholder(cvsPred, msg);
      if (chkShowDiff.checked) {
        drawPlaceholder(cvsDiff, `No slice data for Epoch ${entry.epoch || (currentIndex + 1)}`);
      }
    }
  }

  function renderOverviewGrid(meta, entry) {
    if (!overviewGrid) return;
    const samples = (meta && meta.samples && meta.samples.length) ? meta.samples : [meta];
    const pageSize = viewMode === '16' ? 16 : 8;
    const pageStart = (pageSize === 8 && samples.length > 8)
      ? Math.floor(currentSampleIndex / 8) * 8
      : 0;
    const start = Math.min(pageStart, Math.max(0, samples.length - pageSize));
    const end = Math.min(samples.length, start + pageSize);
    const rangeKey = `${start}:${end}:${viewMode}:${samples.length}`;

    // Build or reuse mini card elements for the visible range
    if (overviewGrid.dataset.rangeKey !== rangeKey) {
      overviewGrid.innerHTML = '';
      overviewGrid.dataset.rangeKey = rangeKey;
      for (let i = start; i < end; ++i) {
        const s = samples[i];
        const card = document.createElement('div');
        card.className = 'sample-card-item' + (i === currentSampleIndex ? ' selected' : '');
        card.dataset.index = i;
        const cardTitle = s.tag ? `#${i} \u00b7 ${s.tag}` : `Sample #${i}`;
        card.innerHTML = `
          <div class="sample-card-header">
            <span style="overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">${cardTitle}</span>
            <span class="tag-pill" id="cardY_${i}">Y=-- km</span>
          </div>
          <div class="sample-subpair">
            <div class="sample-subcol">
              <span class="sample-subcol-title">Truth</span>
              <canvas class="sample-mini-canvas" id="miniTruth_${i}" width="320" height="160"></canvas>
            </div>
            <div class="sample-subcol">
              <span class="sample-subcol-title">Predicted</span>
              <canvas class="sample-mini-canvas" id="miniPred_${i}" width="320" height="160"></canvas>
            </div>
          </div>
        `;
        card.addEventListener('click', () => {
          showSample(i);
          overviewGrid.querySelectorAll('.sample-card-item').forEach(c => {
            c.classList.toggle('selected', Number(c.dataset.index) === i);
          });
        });
        overviewGrid.appendChild(card);
      }
    } else {
      overviewGrid.querySelectorAll('.sample-card-item').forEach(c => {
        c.classList.toggle('selected', Number(c.dataset.index) === currentSampleIndex);
      });
    }

    // Paint each visible sample's truth and predicted cross-sections
    for (let i = start; i < end; ++i) {
      const s = samples[i];
      const yTag = document.getElementById(`cardY_${i}`);
      if (yTag) yTag.textContent = `Y=${s.y_km != null ? s.y_km : '--'} km`;

      const cvsT = document.getElementById(`miniTruth_${i}`);
      const cvsP = document.getElementById(`miniPred_${i}`);
      const vmax = s.vmax || 0.6;
      const terrain = s.terrain || [];
      const tSlice = s.truth_slice || [];
      let pSlice = [];
      if (entry.pred_slices && Array.isArray(entry.pred_slices) && entry.pred_slices[i]) {
        pSlice = entry.pred_slices[i];
      } else if (i === 0) {
        pSlice = entry.pred_slice || (entry.slice && entry.slice.pred) || [];
      }

      if (cvsT && tSlice.length) {
        drawVoxelSection(cvsT, tSlice, terrain, vmax, null, false);
      }
      if (cvsP) {
        if (pSlice.length) {
          drawVoxelSection(cvsP, pSlice, terrain, vmax, null, false);
        } else {
          drawPlaceholder(cvsP, "No pred");
        }
      }
    }
  }

  function drawVoxelSection(canvas, matrix, terrain, vmax, hCell, isDiff) {
    const ctx = canvas.getContext('2d');
    const cw = canvas.width, ch = canvas.height;
    const nz = matrix.length, nx = matrix[0].length;
    const cellW = cw / nx, cellH = ch / nz;
    const cellM = 50.0;
    const totalDepth = nz * cellM;

    ctx.clearRect(0, 0, cw, ch);

    // 1. Draw raw numerical voxel cells
    for (let iz = 0; iz < nz; ++iz) {
      for (let ix = 0; ix < nx; ++ix) {
        ctx.fillStyle = getCoolwarmColor(matrix[iz][ix], vmax);
        ctx.fillRect(ix * cellW, iz * cellH, cellW + 0.5, cellH + 0.5);
      }
    }

    // 2. Air above terrain relief (shaded overlay)
    if (terrain && terrain.length >= nx) {
      ctx.beginPath();
      ctx.moveTo(0, 0);
      for (let ix = 0; ix < nx; ++ix) {
        const tDepth = terrain[ix] || 0.0;
        const ty = (tDepth / totalDepth) * ch;
        ctx.lineTo((ix + 0.5) * cellW, ty);
      }
      ctx.lineTo(cw, 0);
      ctx.closePath();
      ctx.fillStyle = "rgba(100, 116, 139, 0.85)";
      ctx.fill();

      // Terrain surface line
      ctx.beginPath();
      for (let ix = 0; ix < nx; ++ix) {
        const tDepth = terrain[ix] || 0.0;
        const ty = (tDepth / totalDepth) * ch;
        if (ix === 0) ctx.moveTo(0, ty);
        else ctx.lineTo((ix + 0.5) * cellW, ty);
      }
      ctx.strokeStyle = "#0f172a";
      ctx.lineWidth = 2.0;
      ctx.stroke();
    }

    // 3. Grid line indicators & depth marks
    ctx.strokeStyle = "rgba(255,255,255,0.08)";
    ctx.lineWidth = 1.0;
    ctx.strokeRect(0, 0, cw, ch);

    // 4. Hover crosshair & cell highlight
    if (hCell) {
      const { ix, iz } = hCell;
      ctx.strokeStyle = "#38bdf8";
      ctx.lineWidth = 1.5;
      ctx.strokeRect(ix * cellW, iz * cellH, cellW, cellH);

      ctx.setLineDash([3, 3]);
      ctx.beginPath();
      ctx.moveTo((ix + 0.5) * cellW, 0);
      ctx.lineTo((ix + 0.5) * cellW, ch);
      ctx.moveTo(0, (iz + 0.5) * cellH);
      ctx.lineTo(cw, (iz + 0.5) * cellH);
      ctx.strokeStyle = "rgba(56, 189, 248, 0.6)";
      ctx.stroke();
      ctx.setLineDash([]);
    }
  }

  function renderProfileCanvas() {
    const meta = getMeta();
    const sm = getActiveSampleMeta();
    const ctx = cvsProfile.getContext('2d');
    const w = cvsProfile.width, h = cvsProfile.height;
    const prof = (sm && sm.obs_profile) || (meta && meta.obs_profile) || [];
    if (!prof.length) {
      ctx.clearRect(0, 0, w, h);
      ctx.fillStyle = "rgba(100, 116, 139, 0.4)";
      ctx.font = "11px -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif";
      ctx.textAlign = "center";
      ctx.fillText("Gravity traverse profile available on new training runs", w / 2, h / 2 + 4);
      return;
    }
    const N = prof.length;
    const padL = 40, padR = 20, padT = 8, padB = 18;
    const pw = w - padL - padR, ph = h - padT - padB;

    ctx.clearRect(0, 0, w, h);

    let pMin = Math.min(0, Math.min(...prof));
    let pMax = Math.max(0, Math.max(...prof));
    const span = Math.max(1e-3, pMax - pMin);
    pMin -= span * 0.1; pMax += span * 0.1;

    const xCoord = (ix) => padL + (ix / (N - 1)) * pw;
    const yCoord = (val) => padT + ph - ((val - pMin) / (pMax - pMin)) * ph;

    // Zero line
    const yZero = yCoord(0);
    ctx.strokeStyle = "rgba(255,255,255,0.15)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(padL, yZero);
    ctx.lineTo(padL + pw, yZero);
    ctx.stroke();

    // Shaded fill under curve
    ctx.beginPath();
    ctx.moveTo(xCoord(0), yZero);
    for (let i = 0; i < N; ++i) ctx.lineTo(xCoord(i), yCoord(prof[i]));
    ctx.lineTo(xCoord(N - 1), yZero);
    ctx.closePath();
    ctx.fillStyle = "rgba(56, 189, 248, 0.15)";
    ctx.fill();

    // Line curve
    ctx.beginPath();
    for (let i = 0; i < N; ++i) {
      const cx = xCoord(i), cy = yCoord(prof[i]);
      if (i === 0) ctx.moveTo(cx, cy);
      else ctx.lineTo(cx, cy);
    }
    ctx.strokeStyle = "#38bdf8";
    ctx.lineWidth = 2.0;
    ctx.stroke();

    // Axis labels
    ctx.fillStyle = "#64748b";
    ctx.font = "9px monospace";
    ctx.textAlign = "right";
    ctx.fillText(pMax.toFixed(1) + " mGal", padL - 4, padT + 8);
    ctx.fillText(pMin.toFixed(1) + " mGal", padL - 4, padT + ph);

    // Hover indicator line
    if (hoverCell) {
      const hx = xCoord(hoverCell.ix);
      ctx.strokeStyle = "rgba(255, 255, 255, 0.7)";
      ctx.lineWidth = 1;
      ctx.setLineDash([2, 2]);
      ctx.beginPath();
      ctx.moveTo(hx, padT);
      ctx.lineTo(hx, padT + ph);
      ctx.stroke();
      ctx.setLineDash([]);
    }
  }

  function onCanvasHover(e) {
    const meta = getMeta();
    const sm = getActiveSampleMeta();
    const epochs = getEpochs();
    if (!meta || !epochs.length || currentIndex < 0 || !epochs[currentIndex]) return;
    const entry = epochs[currentIndex];
    const rect = e.target.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const nx = meta.nx, nz = meta.nz;
    const cellM = meta.cell || 50.0;
    const totalDepth = nz * cellM;

    let ix = Math.min(nx - 1, Math.max(0, Math.floor((x / rect.width) * nx)));
    let iz = Math.min(nz - 1, Math.max(0, Math.floor((y / rect.height) * nz)));

    if (e.target === cvsProfile) {
      const padL = 40, padR = 20;
      const frac = (x - (padL / cvsProfile.width * rect.width)) / ((rect.width * (cvsProfile.width - padL - padR)) / cvsProfile.width);
      ix = Math.min(nx - 1, Math.max(0, Math.round(frac * (nx - 1))));
      iz = 0;
    }

    hoverCell = { ix, iz };

    const tSlice = (sm && sm.truth_slice) || meta.truth_slice || (entry.slice && entry.slice.truth) || [];
    let pSlice = [];
    if (entry.pred_slices && Array.isArray(entry.pred_slices) && entry.pred_slices[currentSampleIndex]) {
      pSlice = entry.pred_slices[currentSampleIndex];
    } else if (currentSampleIndex === 0) {
      pSlice = entry.pred_slice || (entry.slice && entry.slice.pred) || [];
    }
    const tVal = (tSlice[iz] && tSlice[iz][ix] != null) ? tSlice[iz][ix] : 0.0;
    const pVal = (pSlice[iz] && pSlice[iz][ix] != null) ? pSlice[iz][ix] : 0.0;
    const diffVal = pVal - tVal;

    const xKm = ((ix + 0.5) * cellM / 1000.0).toFixed(2);
    const zM = Math.round((iz + 0.5) * cellM);
    const terr = (sm && sm.terrain) || meta.terrain;
    const terrM = terr ? Math.round(terr[ix]) : 0;
    const isAir = zM < terrM;

    if (isAir) {
      voxelHoverPill.textContent = `X = ${xKm} km | Depth = ${zM} m (AIR above relief: surface at ${terrM} m)`;
    } else if (!pSlice.length) {
      voxelHoverPill.textContent = `X = ${xKm} km, Depth = ${zM} m | Truth: ${tVal > 0 ? '+' : ''}${tVal.toFixed(3)} g/cm\u00b3`;
    } else {
      voxelHoverPill.textContent = `X = ${xKm} km, Depth = ${zM} m | Pred: ${pVal > 0 ? '+' : ''}${pVal.toFixed(3)} g/cm\u00b3 | Truth: ${tVal > 0 ? '+' : ''}${tVal.toFixed(3)} g/cm\u00b3 | Diff: ${diffVal > 0 ? '+' : ''}${diffVal.toFixed(3)}`;
    }

    const obsProf = (sm && sm.obs_profile) || meta.obs_profile;
    if (obsProf && obsProf.length) {
      const gzVal = obsProf[ix] != null ? obsProf[ix].toFixed(2) : '--';
      profileHoverVal.textContent = `Profile X = ${xKm} km | gz = ${gzVal} mGal | Relief Depth = ${terrM} m`;
    }

    renderCurrentVoxelData();
    renderProfileCanvas();
  }

  // --- SVG Convergence Charts ---
  function renderAllCharts() {
    renderSvgChart('svgLoss', [
      { key: 'train_loss', color: '#38bdf8', label: 'Train Loss' },
      { key: 'val_loss', color: '#4ade80', label: 'Val Loss' },
      { key: 'data_rel_misfit', color: '#c084fc', label: 'Data Misfit' }
    ], null, null, lossLogScale);
    renderSvgChart('svgIoU', [
      { key: 'iou', color: '#4ade80', label: '3D IoU' },
      { key: 'amp_ratio', color: '#38bdf8', label: 'Amp Ratio' }
    ], 0.0, 1.0, false);
    renderSvgChart('svgDz', [
      { key: 'centroid_dz', color: '#f87171', label: '|dz| [m]' }
    ], null, null, false);
  }

  function formatChartTick(val, useLog) {
    if (useLog) {
      if (val >= 10) return val.toFixed(0);
      if (val >= 1) return val.toFixed(1);
      if (val >= 0.05) return val.toFixed(2);
      return val.toFixed(3);
    }
    return val.toFixed(val < 10 ? 2 : 0);
  }

  function renderSvgChart(svgId, series, fixedMin = null, fixedMax = null, useLog = false) {
    const svg = document.getElementById(svgId);
    const epochs = getEpochs();
    if (!svg || !epochs.length) return;
    const N = epochs.length;
    const w = 420, h = 125, padL = 42, padR = 14, padT = 10, padB = 22;
    const pw = w - padL - padR, ph = h - padT - padB;

    let yMin = fixedMin != null ? fixedMin : Infinity;
    let yMax = fixedMax != null ? fixedMax : -Infinity;
    const LOG_FLOOR = 1e-4;

    if (fixedMin == null || fixedMax == null) {
      series.forEach(s => {
        epochs.forEach(r => {
          const v = r[s.key];
          if (v != null && isFinite(v)) {
            const tv = useLog ? Math.max(v, LOG_FLOOR) : v;
            if (fixedMin == null && (!useLog || v > 0) && tv < yMin) yMin = tv;
            if (fixedMax == null && tv > yMax) yMax = tv;
          }
        });
      });
      if (!isFinite(yMin) || !isFinite(yMax)) {
        yMin = useLog ? 1e-2 : 0;
        yMax = 1;
      }
      if (useLog) {
        let lMin = Math.log10(Math.max(yMin, LOG_FLOOR));
        let lMax = Math.log10(Math.max(yMax, LOG_FLOOR * 10));
        if (lMax <= lMin) { lMin -= 0.5; lMax += 0.5; }
        const pad = (lMax - lMin) * 0.08;
        yMin = Math.pow(10, lMin - pad);
        yMax = Math.pow(10, lMax + pad);
      } else {
        if (yMin === yMax) { yMin = 0; yMax = 1; }
        const margin = (yMax - yMin) * 0.08;
        yMin = Math.max(0, yMin - margin);
        yMax += margin;
      }
    }

    const lMin = useLog ? Math.log10(Math.max(yMin, LOG_FLOOR)) : 0;
    const lMax = useLog ? Math.log10(Math.max(yMax, LOG_FLOOR * 10)) : 1;
    const xCoord = (idx) => padL + (N > 1 ? (idx / (N - 1)) * pw : pw * 0.5);
    const yCoord = (val) => {
      if (!useLog) return padT + ph - ((val - yMin) / (yMax - yMin)) * ph;
      const lv = Math.log10(Math.max(val, yMin));
      return padT + ph - ((lv - lMin) / (lMax - lMin)) * ph;
    };

    let gridSvg = '';
    if (useLog) {
      const k0 = Math.ceil(lMin + 1e-6), k1 = Math.floor(lMax - 1e-6);
      for (let k = k0; k <= k1; ++k) {
        const tick = Math.pow(10, k);
        const gy = yCoord(tick).toFixed(1);
        gridSvg += `<line x1="${padL}" y1="${gy}" x2="${w - padR}" y2="${gy}" stroke="#1e293b" stroke-width="1" stroke-dasharray="2,2"/>`;
        if (gy > padT + 14 && gy < padT + ph - 10) {
          gridSvg += `<text x="${padL - 4}" y="${(+gy + 3).toFixed(1)}" fill="#475569" font-size="8" text-anchor="end" font-family="monospace">${formatChartTick(tick, true)}</text>`;
        }
      }
    }

    let content = `
      <rect width="${w}" height="${h}" fill="#0f172a" rx="4"/>
      ${gridSvg}
      <line x1="${padL}" y1="${padT + ph}" x2="${w - padR}" y2="${padT + ph}" stroke="#334155" stroke-width="1"/>
      <line x1="${padL}" y1="${padT}" x2="${padL}" y2="${padT + ph}" stroke="#334155" stroke-width="1"/>
      <text x="${padL - 4}" y="${padT + 9}" fill="#64748b" font-size="9" text-anchor="end" font-family="monospace">${formatChartTick(yMax, useLog)}</text>
      <text x="${padL - 4}" y="${padT + ph}" fill="#64748b" font-size="9" text-anchor="end" font-family="monospace">${formatChartTick(yMin, useLog)}</text>
    `;

    // Polylines
    series.forEach(s => {
      const pts = [];
      epochs.forEach((r, idx) => {
        const v = r[s.key];
        if (v != null && isFinite(v)) pts.push(`${xCoord(idx).toFixed(1)},${yCoord(v).toFixed(1)}`);
      });
      if (pts.length) {
        content += `<polyline fill="none" stroke="${s.color}" stroke-width="1.8" points="${pts.join(' ')}"/>`;
      }
    });

    // Vertical Scrubber Line
    const curX = xCoord(currentIndex);
    content += `<line id="${svgId}_scrubber" x1="${curX}" y1="${padT}" x2="${curX}" y2="${padT + ph}" stroke="#f8fafc" stroke-width="1.5" stroke-dasharray="3,3"/>`;

    svg.setAttribute('viewBox', `0 0 ${w} ${h}`);
    svg.innerHTML = content;

    svg.onclick = (e) => {
      const rect = svg.getBoundingClientRect();
      const clickX = e.clientX - rect.left;
      const frac = Math.max(0, Math.min(1, (clickX - (padL / w * rect.width)) / ((pw / w) * rect.width)));
      const targetIdx = Math.round(frac * (epochs.length - 1));
      stopPlayback();
      showEpoch(targetIdx);
    };
  }

  function updateChartScrubbers(idx) {
    const epochs = getEpochs();
    if (!epochs.length) return;
    const N = epochs.length;
    const w = 420, padL = 42, padR = 14;
    const pw = w - padL - padR;
    const x = padL + (N > 1 ? (idx / (N - 1)) * pw : pw * 0.5);

    ['svgLoss_scrubber', 'svgIoU_scrubber', 'svgDz_scrubber'].forEach(id => {
      const el = document.getElementById(id);
      if (el) { el.setAttribute('x1', x); el.setAttribute('x2', x); }
    });
  }

  init();
})();
</script>
</body>
</html>
"""


class TrainingMonitor:
    """Renders a 6-panel training monitor (loss curves, 3D IoU/amplitude, depth error, and live
    gravity + vertical cross-section under terrain) to a native GUI window and an interactive
    HTML/JS web dashboard that directly paints the actual numerical voxel data on HTML5 canvas."""

    def __init__(self, save_dir, gui=False):
        import matplotlib
        if not gui:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        self.plt = plt
        self.save_dir = save_dir
        self.gui = gui
        self.meta = None
        hist_path = os.path.join(self.save_dir, "training_history.json")
        if os.path.exists(hist_path):
            try:
                with open(hist_path) as f:
                    data = json.load(f)
                    if isinstance(data, dict):
                        self.meta = data.get("meta")
                        self.hist = data.get("epochs", [])
                    elif isinstance(data, list):
                        self.hist = data
            except Exception:
                self.hist = []
        else:
            self.hist = []
        js_path = os.path.join(self.save_dir, "training_history.js")
        if not os.path.exists(js_path):
            with open(js_path, "w") as f:
                f.write("window.TRAINING_DATA = {meta: null, epochs: []};\n")
        if gui:
            plt.ion()
        self.fig, self.axes = plt.subplots(2, 3, figsize=(15, 8.5), constrained_layout=True)
        if gui and hasattr(self.fig.canvas, "manager") and self.fig.canvas.manager is not None:
            try:
                self.fig.canvas.manager.set_window_title("Mod3D .inv — 3D Swin Transformer Training Monitor")
            except Exception:
                pass
        self._write_html()

    def _write_html(self):
        html_path = os.path.join(self.save_dir, "dashboard.html")
        tmp_path = html_path + ".tmp"
        with open(tmp_path, "w") as f:
            f.write(build_dashboard_html())
        os.replace(tmp_path, html_path)

    def update(self, rec, vx, vtrue, vpred, vclean, vz, sample_labels=None):
        # Extract slices across up to 16 representative validation samples (val + curated categories)
        n_samples = min(16, vtrue.shape[0])
        x_km = np.arange(NX) * CELL / 1000.0
        y_km = np.arange(NY) * CELL / 1000.0
        z_m = np.arange(NZ) * CELL

        existing_samples = (self.meta or {}).get("samples") or []
        needs_meta = (
            self.meta is None
            or len(existing_samples) != n_samples
            or (sample_labels is not None and any("tag" not in s for s in existing_samples))
        )
        if needs_meta:
            samples_meta = []
            for s_idx in range(n_samples):
                t_vol_s = vtrue[s_idx, 0].detach().cpu().numpy()
                obs_map_s = vx[s_idx, 0].detach().cpu().numpy()
                terr_m_s = vx[s_idx, 1].detach().cpu().numpy() * (NZ * CELL)
                iz_pk_s, ix_pk_s, iy_pk_s = np.unravel_index(np.argmax(np.abs(t_vol_s)), t_vol_s.shape)
                item = {
                    "id": s_idx,
                    "iy": int(iy_pk_s),
                    "y_km": round(float(y_km[iy_pk_s]), 2),
                    "vmax": round(float(np.abs(t_vol_s[:, :, iy_pk_s]).max()), 3),
                    "truth_slice": np.round(t_vol_s[:, :, iy_pk_s], 3).tolist(),
                    "terrain": np.round(terr_m_s[:, iy_pk_s], 1).tolist(),
                    "obs_profile": np.round(obs_map_s[:, iy_pk_s], 3).tolist(),
                }
                if sample_labels is not None and s_idx < len(sample_labels):
                    item["tag"] = str(sample_labels[s_idx])
                samples_meta.append(item)
            self.meta = {
                "nx": NX,
                "ny": NY,
                "nz": NZ,
                "cell": CELL,
                "iy": samples_meta[0]["iy"],
                "y_km": samples_meta[0]["y_km"],
                "vmax": samples_meta[0]["vmax"],
                "truth_slice": samples_meta[0]["truth_slice"],
                "terrain": samples_meta[0]["terrain"],
                "obs_profile": samples_meta[0]["obs_profile"],
                "samples": samples_meta,
            }

        # Pick validation sample 0 for matplotlib figures & legacy fields
        t_vol = vtrue[0, 0].detach().cpu().numpy()                 # (nz, nx, ny)
        p_vol = vpred[0, 0].detach().cpu().numpy()
        obs_map = vx[0, 0].detach().cpu().numpy()                  # (nx, ny)
        terr_m = vx[0, 1].detach().cpu().numpy() * (NZ * CELL)     # terrain depth [m]
        iy_pk = self.meta["samples"][0]["iy"]
        vmax_r = max(0.1, float(max(np.abs(t_vol[:, :, iy_pk]).max(), np.abs(p_vol[:, :, iy_pk]).max())))

        # Extract predictions for all tracked validation samples
        pred_slices = []
        for s_idx in range(n_samples):
            p_vol_s = vpred[s_idx, 0].detach().cpu().numpy()
            iy_s = self.meta["samples"][s_idx]["iy"]
            pred_slices.append(np.round(p_vol_s[:, :, iy_s], 3).tolist())

        # Store numerical voxel slices for the current epoch (no image files)
        rec_entry = dict(rec)
        rec_entry["pred_slice"] = pred_slices[0]
        rec_entry["pred_slices"] = pred_slices
        rec_entry["vmax_pred"] = round(float(np.abs(p_vol[:, :, iy_pk]).max()), 3)
        self.hist.append(rec_entry)

        payload = {"meta": self.meta, "epochs": self.hist}
        with open(os.path.join(self.save_dir, "training_history.json"), "w") as f:
            json.dump(payload, f)
        with open(os.path.join(self.save_dir, "training_history.js"), "w") as f:
            f.write("window.TRAINING_DATA = " + json.dumps(payload) + ";\n")

        ep = [r["epoch"] for r in self.hist]
        self.fig.clf()
        ax = self.fig.subplots(2, 3)

        # 1. Losses (log y-axis so late fine-tuning drops remain clearly visible)
        ax[0, 0].plot(ep, [max(1e-4, r["train_loss"]) for r in self.hist], "o-", label="Train Total")
        ax[0, 0].plot(ep, [max(1e-4, r["val_loss"]) for r in self.hist], "s-", label="Val Total")
        ax[0, 0].plot(ep, [max(1e-4, r["data_rel_misfit"]) for r in self.hist], "^--", label="Val Data Misfit ||Gm-d||/||d||")
        ax[0, 0].plot(ep, [max(1e-4, r.get("halo_opp", 0.0)) for r in self.hist], "d:", label="Val Opposite-Sign Halo")
        ax[0, 0].set_yscale("log")
        ax[0, 0].set_title("Loss, Data Misfit & Opposite Halo (Log)")
        ax[0, 0].set_xlabel("Epoch")
        ax[0, 0].grid(True, which="both", alpha=0.25)
        ax[0, 0].legend(fontsize=8)

        # 2. 3D IoU & Amplitude Recovery
        ax[0, 1].plot(ep, [r["iou"] for r in self.hist], "o-", color="#10b981", label="3D Volumetric IoU")
        ax[0, 1].plot(ep, [r["amp_ratio"] for r in self.hist], "s-", color="#3b82f6", label="Amplitude Ratio")
        ax[0, 1].plot(ep, [1.0 - r["body_l1_rel"] for r in self.hist], "^--", color="#f59e0b", label="1 - Body Rel L1")
        ax[0, 1].set_ylim(0.0, 1.05)
        ax[0, 1].set_title("3D Body IoU & Amplitude Recovery")
        ax[0, 1].set_xlabel("Epoch")
        ax[0, 1].grid(True, alpha=0.3)
        ax[0, 1].legend(fontsize=8)

        # 3. Depth Centroid Error
        ax[0, 2].plot(ep, [r["centroid_dz"] for r in self.hist], "o-", color="#ef4444", label="|dz centroid| [m]")
        ax[0, 2].axhline(CELL, color="#6b7280", linestyle=":", label=f"1 cell ({CELL:.0f} m)")
        ax[0, 2].set_title("3D Depth Centroid Error [m]")
        ax[0, 2].set_xlabel("Epoch")
        ax[0, 2].set_ylabel("Metres")
        ax[0, 2].grid(True, alpha=0.3)
        ax[0, 2].legend(fontsize=8)

        # 4. Surface Draped Gravity Anomaly Map
        vlim_g = max(1e-3, float(np.abs(obs_map).max()))
        im0 = ax[1, 0].imshow(obs_map.T, origin="lower", extent=[x_km[0], x_km[-1], y_km[0], y_km[-1]],
                              cmap="RdBu_r", vmin=-vlim_g, vmax=vlim_g, aspect="equal")
        ax[1, 0].axhline(y_km[iy_pk], color="k", linestyle="--", linewidth=1.0, label=f"Profile Y={y_km[iy_pk]:.2f} km")
        ax[1, 0].set_title(f"Val #0 Draped Gravity gz [mGal] (max {vlim_g:.2f})")
        ax[1, 0].set_xlabel("X [km]")
        ax[1, 0].set_ylabel("Y [km]")
        ax[1, 0].legend(loc="upper right", fontsize=8)
        self.fig.colorbar(im0, ax=ax[1, 0], fraction=0.046)

        # 5 & 6. True vs Predicted X-Z Section under Terrain Relief
        for col_idx, (sec, title) in enumerate([(t_vol[:, :, iy_pk], "Ground Truth Density [g/cm³]"),
                                                (p_vol[:, :, iy_pk], f"Swin-3D Prediction (Epoch {rec['epoch']})")]):
            a = ax[1, 1 + col_idx]
            im = a.imshow(sec, origin="upper", extent=[x_km[0], x_km[-1], z_m[-1] + CELL, 0.0],
                          cmap="coolwarm", vmin=-vmax_r, vmax=vmax_r, aspect="auto")
            a.fill_between(x_km, 0.0, terr_m[:, iy_pk], color="#d1d5db", alpha=0.85, label="Air above relief")
            a.plot(x_km, terr_m[:, iy_pk], color="#111827", linewidth=1.5, label="Terrain surface")
            a.set_title(title)
            a.set_xlabel("X [km]")
            a.set_ylabel("Depth below grid top [m]")
            a.legend(loc="lower right", fontsize=8)
            self.fig.colorbar(im, ax=a, fraction=0.046)

        out_png = os.path.join(self.save_dir, "training_monitor.png")
        self.fig.savefig(out_png, dpi=130)

        if self.gui:
            self.fig.canvas.draw_idle()
            self.plt.pause(0.05)


def main():
    args = parse_args()
    os.makedirs(args.save_dir, exist_ok=True)
    dev = args.device
    hard_prob = min(1.0, max(0.0, float(args.hard_prob)))
    grid = (NX, NY, NZ, CELL, CELL, CELL)
    depth_total = NZ * CELL
    print(f"Device {dev} | {args.epochs} epochs x {args.steps} steps x batch {args.batch_size} | "
          f"grid {NX}x{NY}x{NZ} @ {CELL} m | hard-prob {hard_prob:.2f} | ground clearance 0.30-0.40 m")
    print(f"Monitor: {os.path.join(args.save_dir, 'training_monitor.png')} "
          f"| HTML: {os.path.join(args.save_dir, 'dashboard.html')} | GUI window: {'ON' if args.gui else 'OFF (pass --gui)'}")

    monitor = TrainingMonitor(args.save_dir, gui=args.gui)
    op = TerrainGravityOperator(*grid).to(dev)
    train_ds = BodyTerrainDataset(args.steps * args.batch_size, *grid, hard_prob=hard_prob)
    loader = DataLoader(train_ds, batch_size=args.batch_size, num_workers=args.num_workers,
                        persistent_workers=args.num_workers > 0, drop_last=True)
    vx, vtrue, vclean, vz = make_fixed_set(args.n_val, 2024, op, dev, grid, hard_prob=0.0)
    hard_val = None
    if hard_prob > 0.0:
        n_hard = max(8, args.n_val // 2)
        hard_val = make_fixed_set(n_hard, 3024, op, dev, grid, hard_prob=1.0)
    show_x, show_true, show_clean, show_z, show_labels = make_category_showcase(op, dev, grid, seed=4024)
    if hard_val is not None:
        mon_labels = ("Std",) * min(4, len(vx)) + ("Hard",) * min(4, len(hard_val[0])) + show_labels
    else:
        mon_labels = ("Std",) * min(8, len(vx)) + show_labels

    arch = ARCH
    start_epoch, best, ck_resumed = 1, float("inf"), None
    if args.resume:
        ck_resumed = torch.load(args.resume, map_location="cpu")
        arch = ck_resumed.get("arch", {"variant": "v1", "embed_dim": 96, "num_heads": 4, "depth": 4})
        if "epoch" in ck_resumed:
            start_epoch = ck_resumed["epoch"] + 1
        if "val_loss" in ck_resumed:
            best = ck_resumed["val_loss"]
        print(f"Resumed weights from {args.resume} (checkpoint epoch {ck_resumed.get('epoch')}, starting epoch {start_epoch})")

    model = create_model(NX, NY, NZ, **arch).to(dev)
    if ck_resumed is not None:
        model.load_state_dict(ck_resumed.get("raw_model_state_dict", ck_resumed["model_state_dict"]))
    init_updates = (start_epoch - 1) * args.steps if args.resume else 0
    ema = ModelEMA(model, args.ema_decay, lambda: create_model(NX, NY, NZ, **arch).to(dev), initial_updates=init_updates)
    if ck_resumed is not None and ema.ema is not None and "ema_state_dict" in ck_resumed:
        ema.ema.load_state_dict(ck_resumed["ema_state_dict"])
    eval_model = ema.ema if ema.ema is not None else model
    forward_with_aux = ((lambda m, inp: m(inp, return_aux=True)) if arch.get("variant", "v1") in ("v2", "v3")
                        else (lambda m, inp: (m(inp), None)))

    print(f"Architecture: {arch.get('variant', 'v1')} | Parameters: {sum(p.numel() for p in model.parameters()) / 1e6:.2f} M")
    crit = CompoundInversionLoss(lambda_data=args.lambda_data, lambda_ms=args.lambda_ms,
                                 lambda_cls=args.lambda_cls, depth_boost=args.depth_boost, forward_op=op)

    end_epoch = start_epoch + args.epochs - 1
    total_steps = max(1, (end_epoch - start_epoch + 1) * args.steps)
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    if args.resume:
        sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=total_steps, eta_min=args.lr * 0.02)
    else:
        pct_start = min(0.3, max(0.05, 2.0 / total_steps)) if total_steps > 2 else 0.5
        sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=args.lr, total_steps=total_steps,
                                                    pct_start=pct_start, div_factor=10.0, final_div_factor=50.0)

    def _eval_split(xs, ys, cs, zs):
        preds, aux_chunks = [], []
        with torch.no_grad():
            for i in range(0, len(xs), 8):
                p_i, a_i = forward_with_aux(eval_model, xs[i:i + 8])
                preds.append(p_i)
                if a_i is not None:
                    aux_chunks.append(a_i)
            pred = torch.cat(preds)
            vaux = ({k: torch.cat([c[k] for c in aux_chunks])
                     if isinstance(aux_chunks[0][k], torch.Tensor) else aux_chunks[0][k]
                     for k in aux_chunks[0]} if aux_chunks else None)
            sloss, _ = crit(pred, ys, cs, zs, aux=vaux)
        return float(sloss), compute_metrics(pred, ys, cs, zs, op, CELL), pred

    def _pack_monitor(parts):
        xs, ts, ps, cs, zs = zip(*parts)
        return tuple(torch.cat(seq, dim=0) for seq in (xs, ts, ps, cs, zs))

    def validate():
        eval_model.eval()
        std_loss, std_m, std_pred = _eval_split(vx, vtrue, vclean, vz)
        with torch.no_grad():
            show_pred, _ = forward_with_aux(eval_model, show_x)
        show_part = (show_x, show_true, show_pred, show_clean, show_z)
        if hard_val is not None:
            vhx, vhtrue, vhclean, vhz = hard_val
            hard_loss, hard_m, hard_pred = _eval_split(vhx, vhtrue, vhclean, vhz)
            if eval_model is model:
                model.train()
            comb_loss = (1.0 - hard_prob) * std_loss + hard_prob * hard_loss
            comb_m = {}
            for k in std_m:
                vs, vh = std_m[k], hard_m[k]
                if np.isnan(vs) and np.isnan(vh):
                    comb_m[k] = float("nan")
                elif np.isnan(vs):
                    comb_m[k] = vh
                elif np.isnan(vh):
                    comb_m[k] = vs
                else:
                    comb_m[k] = (1.0 - hard_prob) * vs + hard_prob * vh
            n_s = min(4, len(vx), len(std_pred))
            n_h = min(4, len(vhx), len(hard_pred))
            mon_tensors = _pack_monitor([
                (vx[:n_s], vtrue[:n_s], std_pred[:n_s], vclean[:n_s], vz[:n_s]),
                (vhx[:n_h], vhtrue[:n_h], hard_pred[:n_h], vhclean[:n_h], vhz[:n_h]),
                show_part,
            ])
            return comb_loss, comb_m, std_loss, std_m, hard_loss, hard_m, mon_tensors
        if eval_model is model:
            model.train()
        n_s = min(8, len(vx), len(std_pred))
        mon_tensors = _pack_monitor([
            (vx[:n_s], vtrue[:n_s], std_pred[:n_s], vclean[:n_s], vz[:n_s]),
            show_part,
        ])
        return std_loss, std_m, std_loss, std_m, None, None, mon_tensors

    if args.resume:
        best, *_ = validate()

    zero = compute_metrics(torch.zeros_like(vtrue), vtrue, vclean, vz, op, CELL)
    print("Zero predictor (std): " + " ".join(f"{k} {v:.3f}" for k, v in zero.items()))
    if hard_val is not None:
        vhx, vhtrue, vhclean, vhz = hard_val
        zero_h = compute_metrics(torch.zeros_like(vhtrue), vhtrue, vhclean, vhz, op, CELL)
        print("Zero predictor (hard): " + " ".join(f"{k} {v:.3f}" for k, v in zero_h.items()))

    for epoch in range(start_epoch, end_epoch + 1):
        t0, acc, n = time.time(), {}, 0
        for rho, t, c, nf, _ in loader:
            rho, t, c, nf = rho.to(dev), t.to(dev), c.to(dev), nf.to(dev)
            obs, clean, z_obs = simulate_batch(op, rho, t, c, nf)
            rho, t, obs, clean, z_obs = augment_d4(rho, t, obs, clean, z_obs)
            x = torch.cat([obs, terrain_channel(t, depth_total)], dim=1)
            pred, aux = forward_with_aux(model, x)
            loss, m = crit(pred, rho, clean, z_obs, aux=aux)
            opt.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            ema.update(model)
            sched.step()
            for k, v in m.items():
                acc[k] = acc.get(k, 0.0) + v
            n += 1
        vloss, vm, std_loss, std_m, hard_loss, hard_m, mon_t = validate()
        tr = " ".join(f"{k[5:]} {v / n:.3f}" for k, v in acc.items())
        msg = (f"Epoch {epoch:02d}/{end_epoch} [{time.time() - t0:.0f}s] train {tr} | "
               f"val loss {std_loss:.3f} L1 {std_m['l1_rel']:.3f} bodyL1 {std_m['body_l1_rel']:.3f} "
               f"data {std_m['data_rel_misfit']:.3f} IoU {std_m['iou']:.2f} dz {std_m['centroid_dz']:.0f} m "
               f"amp {std_m['amp_ratio']:.2f} halo {std_m['halo_opp']:.3f}")
        if hard_m is not None:
            msg += (f" | hard loss {hard_loss:.3f} data {hard_m['data_rel_misfit']:.3f} "
                    f"IoU {hard_m['iou']:.2f} dz {hard_m['centroid_dz']:.0f} m amp {hard_m['amp_ratio']:.2f}")
        print(msg, flush=True)
        ck = {"epoch": epoch, "model_state_dict": eval_model.state_dict(),
              "raw_model_state_dict": model.state_dict(), "val_loss": vloss, "metrics": std_m,
              "hard_prob": hard_prob,
              "grid": {"nx": NX, "ny": NY, "nz": NZ, "cell": CELL}, "arch": arch}
        if hard_m is not None:
            ck["val_std_loss"] = std_loss
            ck["val_hard_loss"] = hard_loss
            ck["hard_metrics"] = hard_m
        if ema.ema is not None:
            ck["ema_state_dict"] = ema.ema.state_dict()
        torch.save(ck, os.path.join(args.save_dir, "last_swin3d.pt"))
        if vloss < best:
            best = vloss
            torch.save(ck, os.path.join(args.save_dir, "best_swin3d.pt"))
        rec_mon = {"epoch": epoch, "train_loss": acc["loss_total"] / n, "val_loss": vloss, **vm}
        if hard_m is not None:
            rec_mon["val_std_loss"] = std_loss
            rec_mon["val_hard_loss"] = hard_loss
            rec_mon["hard_iou"] = hard_m["iou"]
            rec_mon["hard_dz"] = hard_m["centroid_dz"]
        monitor.update(rec_mon, *mon_t, sample_labels=mon_labels)
    print(f"Done. Best val loss {best:.4f}. Checkpoint: {os.path.join(args.save_dir, 'best_swin3d.pt')}")


if __name__ == "__main__":
    main()
