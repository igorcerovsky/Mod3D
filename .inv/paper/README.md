# arXiv Preprint Package (`Mod3D/.inv/paper`)

**Title:** *3D Gravity Inversion Using Physics-Coupled Shifted-Window Transformers*  
**Author:** Igor Cerovský (`igor.cerovsky@gmail.com`)  
**Target Category:** `physics.geo-ph`  
**Code & Live Demo:** [Mod3D `.inv`](https://github.com/igorcerovsky/Mod3D/tree/main/.inv) | [Interactive 3D/2D WebGL2 Viewer](https://igorcerovsky.github.io/Mod3D/)

---

## Directory Layout

### Flat arXiv Upload Root (`.inv/paper/` excluding `models/`, `scripts/`, and `results/`)
- `main.tex` — Complete arXiv manuscript (`\pdfoutput=1`, fixed date, standard TeX Live packages only, no `minted`, no `shell-escape`)
- `main.bbl` — Compiled BibTeX bibliography (required by arXiv AutoTeX)
- `references.bib` — Verified BibTeX bibliography
- `macros_results.tex` — Auto-generated numeric macros (`\val...`) injected into `main.tex`
- `table_v91_buckets.tex`, `table_ablation.tex`, `table_canonical.tex`, `table_ood.tex`, `table_noise.tex`, `table_depth_clearance.tex`, `table_timing.tex` — Auto-generated `booktabs` LaTeX tables
- `figures/fig1_architecture.pdf` .. `figures/fig7_uncertainty_calib.pdf` — High-resolution vector PDF figures

### Frozen Checkpoints & Reproducibility Tools (Excluded from arXiv Tarball)
- `models/` — Frozen PyTorch checkpoints (`v9.1_hard_best.pt` ep 498, `v9.1_clean_best.pt` ep 153, and historical `v3`–`v9` checkpoints), compact `training_history_v9.1_hard.json`, and SHA-256 provenance in [`models/MODEL_METADATA.json`](models/MODEL_METADATA.json)
- `scripts/classical_baselines.py` — Pure-Python/PyTorch FFT implementation of depth-weighted Tikhonov $L_2$ ($\gamma=0.50$) and Portniaguine–Zhdanov Focusing IRLS ($\gamma=0.75$)
- `scripts/eval_paper_v9_1.py` — Deterministic held-out CPU evaluation suite (disjoint test seed `20260`, 4-bucket test, chronological progression `v3`–`v9.1`, 4 canonical benchmarks, 3 independent non-inverse-crime OOD benchmarks, unified noise/clearance sweeps with paired Focusing IRLS, and matched-thread ONNX/CoreML runtime benchmarks)
- `scripts/generate_tables_and_figures.py` — Generates `macros_results.tex`, 7 `table_*.tex` files, and `figures/fig1`..`fig7` PDFs (+ preview PNGs in `results/preview_png/`)
- `scripts/verify_latex.py` — Static syntax, citation, cross-reference, macro, and figure verification
- `scripts/update_paper.sh` — One-command regeneration, PDF compile, and clean arXiv tarball packaging

---

## Reproducing All Metrics, Tables, Figures, and PDF

Using the archived frozen checkpoints in `.inv/paper/models/`:

```bash
./.inv/paper/scripts/update_paper.sh
```

To evaluate a custom flagship checkpoint:

```bash
./.inv/paper/scripts/update_paper.sh --checkpoint-hard /path/to/best_swin3d.pt
```

## Preparing a Clean AppleDouble-Free arXiv Tarball (macOS)

Always pass `COPYFILE_DISABLE=1` and `--no-xattrs` on macOS so AppleDouble `._*` resource-fork files are excluded (they break arXiv AutoTeX), and include `main.bbl`:

```bash
cd .inv/paper
tectonic --keep-intermediates main.tex
COPYFILE_DISABLE=1 tar --no-xattrs -czvf arxiv_submission.tar.gz \
  main.tex main.bbl references.bib macros_results.tex table_*.tex figures/fig*.pdf
```

Or with TeX Live / MacTeX:

```bash
cd .inv/paper
pdflatex main.tex && bibtex main && pdflatex main.tex && pdflatex main.tex
```
