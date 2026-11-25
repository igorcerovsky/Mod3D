# Archived Frozen Model Checkpoints (`.inv/paper/models/`)

This directory stores the exact frozen PyTorch checkpoints copied from `.inv/python/checkpoints_*` used to generate every metric, table, and figure in the manuscript, together with machine-readable provenance and SHA-256 checksums in [`MODEL_METADATA.json`](MODEL_METADATA.json).

## Checkpoints

| File | Variant | Best Epoch | Params | Role in Paper |
|---|---|---:|---:|---|
| `v9.1_hard_best.pt` | `v9.1` (Base) | 498 | 2,729,729 | Flagship multi-regime model (Tables 1–6, Figs. 2–7) |
| `v6_best.pt` | `v6` | 236 | 2,731,580 | Progression Panel A (5-ch + 2-Stage Unified) |
| `v6.1_best.pt` | `v6.1` | 323 | 2,734,852 | Progression Panel A (5-ch + GVB + Smooth IoU, resumed from `v6`) |
| `v7_best.pt` | `v7` | 99 | 2,857,651 | Progression Panel A (6-ch + 3-Tier Slab) |
| `v8_best.pt` | `v8` | 97 | 2,857,752 | Progression Panel A (6-ch + 3-Tier Unrolled) |
| `v3_clean_best.pt` | `v3` | 118 | 2,699,043 | Progression Panel B (2-ch + Prototypes, clean ground) |
| `v5.2_clean_best.pt` | `v5.2` | 148 | 2,728,457 | Progression Panel B (5-ch + GVB + NLL, clean ground) |
| `v8_clean_best.pt` | `v8` | 35 | 2,857,752 | Progression Panel B (6-ch + 3-Tier Unrolled, clean ground) |
| `v9_clean_best.pt` | `v9` | 119 | 2,729,729 | Progression Panel B (6-ch + Unified Decoder, clean ground) |
| `v9.1_clean_best.pt` | `v9.1` | 153 | 2,729,729 | Progression Panel B (6-ch + Unified + Coherence, clean ground) |

- `training_history_v9.1_hard.json`: Compact scalar per-epoch training/validation history (epochs 1–514) used for Figure 2(a)–(b) and matched-epoch progression comparisons.
- `MODEL_METADATA.json`: Full SHA-256 digests, file sizes, parameter counts, `arch` / `grid` configs, and source checkpoint paths.

> Note: Checkpoints (`*.pt`) are archived here for full Git/local reproducibility and are excluded from the arXiv source tarball (`arxiv_submission.tar.gz`).
