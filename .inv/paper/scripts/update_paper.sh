#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAPER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
INV_PY_DIR="$(cd "${PAPER_DIR}/../python" && pwd)"
PYTHON_BIN="${INV_PY_DIR}/.venv/bin/python"

if [[ ! -x "${PYTHON_BIN}" ]]; then
    PYTHON_BIN="$(command -v python3)"
fi

echo "[1/4] Running CPU evaluation suite (eval_paper_v9_1.py)..."
"${PYTHON_BIN}" -u "${SCRIPT_DIR}/eval_paper_v9_1.py" --device cpu "$@"

echo "[2/4] Generating LaTeX macros, tables, and vector PDF figures..."
"${PYTHON_BIN}" -u "${SCRIPT_DIR}/generate_tables_and_figures.py"

echo "[3/4] Verifying arXiv LaTeX integrity..."
"${PYTHON_BIN}" -u "${SCRIPT_DIR}/verify_latex.py"

echo "[4/4] Compiling PDF and packaging clean arXiv tarball..."
if command -v pdflatex >/dev/null 2>&1; then
    (
        cd "${PAPER_DIR}"
        pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
        bibtex main >/dev/null || true
        pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
        pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
        cp -f main.pdf cerovsky-Transformers-Inversion.pdf
        echo "Compiled ${PAPER_DIR}/cerovsky-Transformers-Inversion.pdf via pdflatex."
    )
elif command -v tectonic >/dev/null 2>&1; then
    (
        cd "${PAPER_DIR}"
        tectonic --keep-intermediates main.tex
        cp -f main.pdf cerovsky-Transformers-Inversion.pdf
        echo "Compiled ${PAPER_DIR}/cerovsky-Transformers-Inversion.pdf via tectonic."
    )
else
    echo "[Info] Neither pdflatex nor tectonic found on PATH; skipping PDF compile."
fi

if [[ -f "${PAPER_DIR}/main.bbl" ]]; then
    (
        cd "${PAPER_DIR}"
        COPYFILE_DISABLE=1 tar --no-xattrs -czvf arxiv_submission.tar.gz \
            main.tex main.bbl references.bib macros_results.tex table_*.tex figures/fig*.pdf >/dev/null
        rm -f main.aux main.blg main.log main.out main.pdf
        echo "Packaged AppleDouble-free ${PAPER_DIR}/arxiv_submission.tar.gz."
    )
fi
