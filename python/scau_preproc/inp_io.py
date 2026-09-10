"""Whitelist SWMM INP reader, writer, and semantic comparator for N1-A."""
from __future__ import annotations

import math
from pathlib import Path

ALLOWED_SECTIONS = ("TITLE", "OPTIONS", "JUNCTIONS", "OUTFALLS", "CONDUITS", "XSECTIONS", "COORDINATES", "REPORT")


def _section_lines(text: str) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = {}
    current = None
    for raw in text.replace("\r\n", "\n").replace("\r", "\n").splitlines():
        line = raw.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].strip().upper()
            if current in sections:
                raise ValueError(f"duplicate SWMM section: {current}")
            sections[current] = []
        elif current is None:
            raise ValueError("content before first SWMM section")
        else:
            sections[current].append(line)
    unknown = sorted(set(sections) - set(ALLOWED_SECTIONS))
    if unknown:
        raise ValueError(f"non-whitelist SWMM section(s): {', '.join(unknown)}")
    return sections


def parse_inp(path_or_text: str | Path) -> dict[str, list[list[str]]]:
    text = Path(path_or_text).read_text(encoding="utf-8") if isinstance(path_or_text, Path) else path_or_text
    sections = _section_lines(text)
    return {name: [line.split() for line in sections.get(name, [])] for name in ALLOWED_SECTIONS if name in sections}


def _number(value: str) -> float | str:
    try:
        return float(value)
    except ValueError:
        return value.upper()


def semantic_model(path_or_text: str | Path) -> dict[str, list[tuple[object, ...]]]:
    parsed = parse_inp(path_or_text)
    return {section: [tuple(_number(token) for token in rows) for rows in rows]
            for section, rows in parsed.items()}


def semantic_equal(left: str | Path, right: str | Path, *, rel_tol: float = 1e-9, abs_tol: float = 1e-9) -> bool:
    a, b = semantic_model(left), semantic_model(right)
    if a.keys() != b.keys():
        return False
    for section in a:
        if len(a[section]) != len(b[section]):
            return False
        for row_a, row_b in zip(a[section], b[section]):
            if len(row_a) != len(row_b):
                return False
            for x, y in zip(row_a, row_b):
                if isinstance(x, float) and isinstance(y, float):
                    if not math.isclose(x, y, rel_tol=rel_tol, abs_tol=abs_tol):
                        return False
                elif x != y:
                    return False
    return True


def write_inp(model: dict[str, list[list[object]]], path: str | Path) -> None:
    unknown = set(model) - set(ALLOWED_SECTIONS)
    if unknown:
        raise ValueError(f"non-whitelist SWMM section(s): {', '.join(sorted(unknown))}")
    chunks: list[str] = []
    for section in ALLOWED_SECTIONS:
        if section not in model:
            continue
        chunks.append(f"[{section}]\n")
        for row in model[section]:
            chunks.append(" ".join(format(v, ".15g") if isinstance(v, float) else str(v) for v in row) + "\n")
        chunks.append("\n")
    Path(path).write_text("".join(chunks), encoding="utf-8", newline="\n")
