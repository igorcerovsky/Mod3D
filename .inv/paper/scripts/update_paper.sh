#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAPER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
INV_PY_DIR="$(cd "${PAPER_DIR}/../python" && pwd)"
PYTHON_BIN="${INV_PY_DIR}/.venv/bin/python"

if [[ ! -x "${PYTHON_BIN}" ]]; then
    PYTHON_BIN="$(command -v python3)"
fi

echo "[1/3] Running CPU evaluation suite (eval_paper_v9_1.py)..."
"${PYTHON_BIN}" -u "${SCRIPT_DIR}/eval_paper_v9_1.py" --device cpu "$@"

echo "[2/3] Generating LaTeX macros, tables, and vector PDF figures..."
"${PYTHON_BIN}" -u "${SCRIPT_DIR}/generate_tables_and_figures.py"

echo "[3/3] Verifying arXiv LaTeX integrity..."
"${PYTHON_BIN}" -u "${SCRIPT_DIR}/verify_latex.py"

if command -v pdflatex >/dev/null 2>&1; then
    echo "[Bonus] pdflatex detected; compiling main.tex..."
    (
        cd "${PAPER_DIR}"
        pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
        bibtex main >/dev/null || true
        pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
        pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
        mv -f main.pdf cerovsky-Transformers-Inversion.pdf
        echo "Compiled ${PAPER_DIR}/cerovsky-Transformers-Inversion.pdf successfully."
    )
elif command -v tectonic >/dev/null 2>&1; then
    echo "[Bonus] tectonic detected; compiling main.tex..."
    (
        cd "${PAPER_DIR}"
        tectonic main.tex
        mv -f main.pdf cerovsky-Transformers-Inversion.pdf
        echo "Compiled ${PAPER_DIR}/cerovsky-Transformers-Inversion.pdf successfully."
    )
else
    echo "[Info] Neither pdflatex nor tectonic found on PATH; skipping PDF compile (verify_latex.py passed)."
fi
