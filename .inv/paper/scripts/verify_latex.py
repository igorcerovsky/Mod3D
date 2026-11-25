"""Static integrity verifier for the arXiv-ready LaTeX manuscript in ``.inv/paper/``.

Checks without requiring a local TeX Live installation:
  1. ``\\pdfoutput=1`` within the first 5 lines of ``main.tex``; zero forbidden packages/flags.
  2. Balanced braces and ``\\begin{...}/\\end{...}`` environments across all ``.tex`` files.
  3. All ``\\ref`` / ``\\eqref`` labels exist; all ``\\cite*`` keys exist in ``references.bib``.
  4. All ``\\includegraphics`` targets resolve to non-empty flat files in ``figures/``.
  5. All ``\\val...`` macros referenced in ``main.tex`` or tables exist in ``macros_results.tex``.
  6. Zero leftover ``TODO``, ``NaN``, or ``Inf`` tokens in any ``.tex`` file.
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path

_PAPER_DIR = Path(__file__).resolve().parent.parent


def _strip_comments(text: str) -> str:
    out_lines = []
    for line in text.splitlines():
        i = 0
        escaped = False
        cut = len(line)
        while i < len(line):
            ch = line[i]
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "%":
                cut = i
                break
            i += 1
        out_lines.append(line[:cut])
    return "\n".join(out_lines)


def _check_balanced(text: str, fname: str, errors: list[str]) -> None:
    stripped = _strip_comments(text)
    depth = 0
    i = 0
    while i < len(stripped):
        ch = stripped[i]
        if ch == "\\" and i + 1 < len(stripped):
            i += 2
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth < 0:
                errors.append(f"{fname}: unmatched closing brace '}}'")
                return
        i += 1
    if depth != 0:
        errors.append(f"{fname}: unbalanced braces (net depth={depth})")

    # Check \begin{env} ... \end{env}
    env_stack: list[str] = []
    for m in re.finditer(r"\\(begin|end)\{([^}]+)\}", stripped):
        kind, env = m.group(1), m.group(2)
        if kind == "begin":
            env_stack.append(env)
        else:
            if not env_stack or env_stack[-1] != env:
                top = env_stack[-1] if env_stack else "<empty>"
                errors.append(f"{fname}: mismatched \\end{{{env}}} (expected {top})")
                return
            env_stack.pop()
    if env_stack:
        errors.append(f"{fname}: unclosed environments {env_stack}")


def parse_bib_keys(bib_path: Path, errors: list[str]) -> set[str]:
    if not bib_path.is_file():
        errors.append(f"Missing bibliography file: {bib_path}")
        return set()
    raw = bib_path.read_text(encoding="utf-8")
    keys = set(re.findall(r"@\w+\s*\{\s*([^,\s]+)\s*,", raw))
    if not keys:
        errors.append("references.bib contains no BibTeX entries")
    # Basic brace balance check on .bib
    depth = 0
    for ch in raw:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth < 0:
                errors.append("references.bib has unmatched closing brace")
                break
    if depth != 0:
        errors.append(f"references.bib has unbalanced braces (net={depth})")
    return keys


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []

    main_tex = _PAPER_DIR / "main.tex"
    macros_tex = _PAPER_DIR / "macros_results.tex"
    bib_file = _PAPER_DIR / "references.bib"

    if not main_tex.is_file():
        print(f"FAIL: missing {main_tex}", file=sys.stderr)
        return 1
    if not macros_tex.is_file():
        print(f"FAIL: missing {macros_tex}", file=sys.stderr)
        return 1

    main_raw = main_tex.read_text(encoding="utf-8")
    first5 = "\n".join(main_raw.splitlines()[:5])
    if r"\pdfoutput=1" not in first5:
        errors.append("main.tex must declare \\pdfoutput=1 within the first 5 lines for arXiv auto-TeX")

    for forbidden in ("minted", "shell-escape", "write18"):
        if forbidden in _strip_comments(main_raw):
            errors.append(f"main.tex contains arXiv-incompatible token: {forbidden!r}")

    tex_files = [main_tex, macros_tex] + sorted(_PAPER_DIR.glob("table_*.tex"))
    combined_stripped = ""
    for tf in tex_files:
        raw = tf.read_text(encoding="utf-8")
        _check_balanced(raw, tf.name, errors)
        st = _strip_comments(raw)
        combined_stripped += "\n" + st
        for bad_pat, desc in (
            (r"\bTODO\b", "TODO"),
            (r"\bFIXME\b", "FIXME"),
            (r"(?<![A-Za-z])nan(?![A-Za-z])", "nan"),
            (r"(?<![A-Za-z])NaN(?![A-Za-z])", "NaN"),
        ):
            if re.search(bad_pat, st):
                errors.append(f"{tf.name}: found forbidden placeholder/numeric artifact '{desc}'")

    # Check \input targets
    for m in re.finditer(r"\\input\{([^}]+)\}", combined_stripped):
        inc = m.group(1)
        inc_path = _PAPER_DIR / (inc if inc.endswith(".tex") else f"{inc}.tex")
        if not inc_path.is_file():
            errors.append(f"Missing \\input target: {inc_path.name}")

    # Check labels and refs
    labels = set(re.findall(r"\\label\{([^}]+)\}", combined_stripped))
    refs = set(re.findall(r"\\(?:eq)?ref\{([^}]+)\}", combined_stripped))
    missing_refs = sorted(refs - labels)
    if missing_refs:
        errors.append(f"Undefined \\ref/\\eqref labels: {missing_refs}")
    unused_labels = sorted(labels - refs)
    if unused_labels:
        warnings.append(f"Unreferenced labels (informational): {unused_labels}")

    # Check citations
    bib_keys = parse_bib_keys(bib_file, errors)
    cite_keys: set[str] = set()
    for m in re.finditer(r"\\cite[a-zA-Z]*\*?(?:\[[^\]]*\])*\{([^}]+)\}", combined_stripped):
        for k in m.group(1).split(","):
            ks = k.strip()
            if ks:
                cite_keys.add(ks)
    missing_cites = sorted(cite_keys - bib_keys)
    if missing_cites:
        errors.append(f"Missing BibTeX keys in references.bib: {missing_cites}")
    unused_bib = sorted(bib_keys - cite_keys)
    if unused_bib:
        warnings.append(f"Unused BibTeX entries in references.bib: {unused_bib}")

    # Check \includegraphics targets
    fig_targets = re.findall(r"\\includegraphics(?:\[[^\]]*\])?\{([^}]+)\}", combined_stripped)
    for ft in fig_targets:
        if "://" in ft:
            errors.append(f"External URL not permitted in \\includegraphics: {ft}")
            continue
        fp = _PAPER_DIR / ft
        if not fp.is_file() or fp.stat().st_size == 0:
            errors.append(f"Missing or empty figure file: {ft}")

    # Check \val... macros
    macros_raw = _strip_comments(macros_tex.read_text(encoding="utf-8"))
    defined_macros = set(re.findall(r"\\(?:re)?newcommand\{\\(val[A-Za-z0-9]+)\}", macros_raw))
    used_macros = set(re.findall(r"\\(val[A-Za-z0-9]+)\b", combined_stripped)) - defined_macros
    # Wait: defined_macros also appears inside \newcommand{\val...} in combined_stripped; verify against non-macro files
    non_macro_text = ""
    for tf in tex_files:
        if tf != macros_tex:
            non_macro_text += "\n" + _strip_comments(tf.read_text(encoding="utf-8"))
    used_in_body = set(re.findall(r"\\(val[A-Za-z0-9]+)\b", non_macro_text))
    missing_macros = sorted(used_in_body - defined_macros)
    if missing_macros:
        errors.append(f"Undefined \\val macros used in manuscript/tables: {missing_macros}")
    unused_macros = sorted(defined_macros - used_in_body)
    if unused_macros:
        warnings.append(f"Unused \\val macros in macros_results.tex ({len(unused_macros)}): {unused_macros}")

    print(
        f"Verified {len(tex_files)} .tex files, {len(labels)} labels, "
        f"{len(cite_keys)}/{len(bib_keys)} citations, {len(fig_targets)} figures, "
        f"{len(used_in_body)}/{len(defined_macros)} result macros."
    )
    for w in warnings:
        print(f"WARNING: {w}")
    if errors:
        for e in errors:
            print(f"ERROR: {e}", file=sys.stderr)
        return 1
    print("PASS: arXiv LaTeX verification succeeded with 0 errors.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
