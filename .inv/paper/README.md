# arXiv Preprint Package (`Mod3D/.inv/paper`)

**Title:** *Transformers Are All You Need for Potential Field Inversion*  
**Author:** Igor Cerovský (`igor.cerovsky@gmail.com`)  
**Target Category:** `physics.geo-ph`  
**Code & Live Demo:** [Mod3D `.inv`](https://github.com/igorcerovsky/Mod3D/tree/main/.inv) | [Interactive 3D/2D WebGL2 Viewer](https://igorcerovsky.github.io/Mod3D/)

---

## Directory Layout

### Flat arXiv Upload Root (`.inv/paper/` excluding `scripts/` and `results/`)
- `main.tex` — Complete arXiv manuscript (`\pdfoutput=1`, standard TeX Live packages only, no `minted`, no `shell-escape`)
- `references.bib` — Verified BibTeX bibliography
- `macros_results.tex` — Auto-generated numeric macros (`\val...`) injected into `main.tex`
- `table_v91_buckets.tex`, `table_ablation.tex`, `table_canonical.tex`, `table_noise.tex`, `table_depth_clearance.tex`, `table_timing.tex` — Auto-generated `booktabs` LaTeX tables
- `figures/fig1_architecture.pdf` .. `figures/fig6_uncertainty_calib.pdf` — High-resolution vector PDF figures

### Reproducibility & Regeneration Tools (Excluded from arXiv Upload)
- `scripts/classical_baselines.py` — Pure-Python FFT twin of C++ `inv::FocusingSolver` (Tikhonov $L_2$, Portniaguine–Zhdanov Focusing IRLS, and Hybrid Swin3D + Focusing refinement)
- `scripts/eval_paper_v9_1.py` — Full deterministic CPU evaluation suite (`v9.1` 4-bucket eval, `v3`–`v9.1` ablation, 4 canonical models vs. classical baselines, noise/clearance/depth sweeps, and ONNX/CoreML runtime benchmarks)
- `scripts/generate_tables_and_figures.py` — Generates `macros_results.tex`, `table_*.tex`, and `figures/fig*.pdf` (+ preview PNGs in `results/preview_png/`)
- `scripts/verify_latex.py` — Static syntax, citation, cross-reference, macro, and figure verification
- `scripts/update_paper.sh` — One-command pipeline to re-run everything after training produces a new checkpoint

---

## Regenerating Results After Training Finishes

Once your `checkpoints_v9.1_hard` training finishes (or produces an updated `best_swin3d.pt`), regenerate every metric, macro, table, and figure in one command:

```bash
./.inv/paper/scripts/update_paper.sh
```

To run with a custom checkpoint path:

```bash
./.inv/paper/scripts/update_paper.sh --checkpoint-hard /path/to/best_swin3d.pt
```

## Preparing the arXiv Tarball

Create a clean arXiv upload archive containing only the LaTeX sources, bibliography, tables, and vector figures:

```bash
cd .inv/paper
tar -czvf arxiv_submission.tar.gz main.tex references.bib macros_results.tex table_*.tex figures/fig*.pdf
```

Compile locally (if TeX Live / MacTeX is installed):

```bash
cd .inv/paper
pdflatex main.tex && bibtex main && pdflatex main.tex && pdflatex main.tex
```
