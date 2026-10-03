#!/usr/bin/env python3
"""Generate the compact item-by-item NativeSharp 3.0 tracking table."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ALLOWED_STATES = {
    "PENDIENTE",
    "EN CURSO",
    "PRUEBAS DIRIGIDAS",
    "TERMINADA",
    "CERTIFICADA",
    "BLOQUEADA",
}


def clean_cell(value: str) -> str:
    value = re.sub(r"\s+", " ", value).strip()
    value = re.sub(r"\s+-\s+", "; ", value)
    value = re.sub(r";\s*;", ";", value)
    return value.replace("|", "\\|")


def roadmap_items(text: str, component: str, pattern: str) -> list[tuple[str, str, str]]:
    matches = list(re.finditer(pattern, text, re.MULTILINE))
    items: list[tuple[str, str, str]] = []
    for match in matches:
        item_id = match.group(1)
        title = clean_cell(match.group(2))
        body_start = match.end()
        next_heading = re.search(r"^#{1,4}\s+", text[body_start:], re.MULTILINE)
        body_end = body_start + next_heading.start() if next_heading else len(text)
        body = text[body_start:body_end]
        implementation = re.search(r"^- \*\*Implementación:\*\*\s*(.+)$", body, re.MULTILINE)
        if implementation:
            plan = implementation.group(1)
        else:
            bullets = re.findall(r"^- (?!\*\*Markdown|\*\*Pruebas)(.+)$", body, re.MULTILINE)
            plan = "; ".join(bullets)
        if not plan:
            raise RuntimeError(f"{item_id} has no implementation text")
        items.append((item_id, title, clean_cell(plan)))
    if not items:
        raise RuntimeError(f"no items found for {component}")
    return items


def read_states(path: Path | None) -> dict[str, str]:
    if path is None or not path.is_file():
        return {}

    states: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
        if not raw_line.startswith("|"):
            continue
        cells = [cell.strip() for cell in re.split(r"(?<!\\)\|", raw_line.strip("|"))]
        if len(cells) != 4:
            continue
        item_match = re.search(r"NS3-[A-Z0-9-]+", cells[1])
        if not item_match:
            continue
        state = cells[3].replace("`", "").strip()
        if state not in ALLOWED_STATES:
            raise RuntimeError(f"{item_match.group(0)} has invalid state {state!r}")
        states[item_match.group(0)] = state
    return states


def generate(repo_root: Path, states: dict[str, str] | None = None) -> str:
    roadmap_path = repo_root / "mkdown" / "NativeSharp-Language-3.0-Roadmap.md"
    roadmap = roadmap_path.read_text(encoding="utf-8")
    components: list[tuple[str, list[tuple[str, str, str]]]] = [
        (
            "NSC",
            roadmap_items(
                roadmap,
                "NSC",
                r"^## (NS3-T\d{2}-F\d{2}) — (.+)$",
            ),
        ),
        (
            "NSLink",
            roadmap_items(
                roadmap,
                "NSLink",
                r"^#### (NS3-LNK-\d{2}) — (.+)$",
            ),
        ),
        (
            "NSDBG",
            roadmap_items(
                roadmap,
                "NSDBG",
                r"^#### (NS3-DBG-\d{2}) — (.+)$",
            ),
        ),
        (
            "NSC-LSP",
            roadmap_items(
                roadmap,
                "NSC-LSP",
                r"^#### (NS3-LSP-\d{2}) — (.+)$",
            ),
        ),
        (
            "NSC-VSCode",
            roadmap_items(
                roadmap,
                "NSC-VSCode",
                r"^#### (NS3-VSC-\d{2}) — (.+)$",
            ),
        ),
        (
            "NSASM",
            roadmap_items(
                roadmap,
                "NSASM",
                r"^#### (NS3-ASM-\d{2}) — (.+)$",
            ),
        ),
    ]

    expected = {
        "NSC": 120,
        "NSLink": 9,
        "NSDBG": 14,
        "NSC-LSP": 10,
        "NSC-VSCode": 7,
        "NSASM": 7,
    }
    actual = {component: len(items) for component, items in components}
    if actual != expected:
        raise RuntimeError(f"item counts differ: {actual!r} != {expected!r}")

    lines = [
        "| Componente / tanda | Ítem | Implementación | Estado |",
        "|---|---|---|---|",
    ]
    states = states or {}
    for component, items in components:
        for item_id, title, implementation in items:
            state = states.get(item_id, "PENDIENTE")
            lines.append(
                f"| {component} | `{item_id}` — {title} | {implementation} | `{state}` |"
            )
    return "\n".join(lines) + "\n"


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--set-state",
        action="append",
        default=[],
        metavar="ITEM=STATE",
        help="update an item state while preserving every other existing state",
    )
    args = parser.parse_args()
    states = read_states(args.output)
    for assignment in args.set_state:
        item_id, separator, state = assignment.partition("=")
        if not separator or state not in ALLOWED_STATES:
            raise RuntimeError(f"invalid state assignment: {assignment!r}")
        states[item_id] = state
    result = generate(repo_root, states)
    if args.output:
        args.output.write_text(result, encoding="utf-8")
    else:
        print(result, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
