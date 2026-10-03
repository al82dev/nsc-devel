#!/usr/bin/env python3
"""Verify the consolidated NativeSharp 3.0 roadmap and its compact tracker."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path


FEATURE_COUNTS = {
    0: 6,
    1: 7,
    2: 6,
    3: 6,
    4: 7,
    5: 7,
    6: 8,
    7: 6,
    8: 6,
    9: 6,
    10: 8,
    11: 7,
    12: 7,
    13: 8,
    14: 6,
    15: 6,
    16: 6,
    17: 7,
}

COMPONENT_TOTALS = {
    "NSC": 120,
    "NSLink": 9,
    "NSDBG": 14,
    "NSC-LSP": 10,
    "NSC-VSCode": 7,
    "NSASM": 7,
}

ALLOWED_STATES = {
    "PENDIENTE",
    "EN CURSO",
    "PRUEBAS DIRIGIDAS",
    "TERMINADA",
    "CERTIFICADA",
    "BLOQUEADA",
}


class VerificationError(RuntimeError):
    pass


def expected_nsc_feature_ids() -> set[str]:
    return {
        f"NS3-T{tanda:02d}-F{feature:02d}"
        for tanda, count in FEATURE_COUNTS.items()
        for feature in range(1, count + 1)
    }


def verify_nsc_roadmap(path: Path) -> None:
    if not path.is_file():
        raise VerificationError(f"roadmap not found: {path}")

    text = path.read_text(encoding="utf-8")
    headings = re.findall(r"^## (NS3-T\d{2}-F\d{2})\b", text, re.MULTILINE)
    counts = Counter(headings)
    actual = set(counts)
    expected = expected_nsc_feature_ids()

    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    duplicates = sorted(item for item, count in counts.items() if count != 1)
    errors: list[str] = []
    if missing:
        errors.append("missing NSC feature headings: " + ", ".join(missing))
    if unexpected:
        errors.append("unexpected NSC feature headings: " + ", ".join(unexpected))
    if duplicates:
        errors.append("duplicate NSC feature headings: " + ", ".join(duplicates))

    required_markers = (
        "## 12. Plan integral y ordenado del toolchain",
        "### 12.2 Etapa 1 — NSC",
        "#### Estado ejecutivo del catálogo NSC",
        "P00`–`P11",
        "SH00`–`SH54",
    )
    absent_markers = [marker for marker in required_markers if marker not in text]
    if absent_markers:
        errors.append("missing consolidated NSC markers: " + ", ".join(absent_markers))

    if errors:
        raise VerificationError("; ".join(errors))


def _plain(cell: str) -> str:
    return cell.replace("\\|", "|").replace("`", "").replace("**", "").strip()


def expected_component_ids() -> dict[str, set[str]]:
    return {
        "NSC": expected_nsc_feature_ids(),
        "NSLink": {f"NS3-LNK-{item:02d}" for item in range(1, 10)},
        "NSDBG": {f"NS3-DBG-{item:02d}" for item in range(1, 15)},
        "NSC-LSP": {f"NS3-LSP-{item:02d}" for item in range(1, 11)},
        "NSC-VSCode": {f"NS3-VSC-{item:02d}" for item in range(1, 8)},
        "NSASM": {f"NS3-ASM-{item:02d}" for item in range(1, 8)},
    }


def parse_tracking(path: Path) -> dict[str, tuple[str, str, str]]:
    if not path.is_file():
        raise VerificationError(f"tracking not found: {path}")

    rows: dict[str, tuple[str, str, str]] = {}
    for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if not line.startswith("|"):
            raise VerificationError("tracking must contain only one Markdown table")
        cells = [cell.strip() for cell in re.split(r"(?<!\\)\|", line.strip("|"))]
        if len(cells) != 4 or cells[0] == "Componente / tanda" or set(cells[0]) <= {"-", ":"}:
            continue
        component = _plain(cells[0])
        item_match = re.match(r"(NS3-[A-Z0-9-]+)", _plain(cells[1]))
        if not item_match:
            raise VerificationError(f"tracking row has no item ID: {line}")
        item_id = item_match.group(1)
        implementation = _plain(cells[2])
        state = _plain(cells[3])
        if item_id in rows:
            raise VerificationError(f"duplicate tracking item: {item_id}")
        rows[item_id] = (component, implementation, state)
    return rows


def verify_tracking(path: Path) -> None:
    rows = parse_tracking(path)
    expected_by_component = expected_component_ids()
    expected_ids = set().union(*expected_by_component.values())
    actual_ids = set(rows)
    missing = sorted(expected_ids - actual_ids)
    unexpected = sorted(actual_ids - expected_ids)
    errors: list[str] = []
    if missing:
        errors.append("missing tracking items: " + ", ".join(missing))
    if unexpected:
        errors.append("unexpected tracking items: " + ", ".join(unexpected))

    owner_by_id = {
        item_id: component
        for component, item_ids in expected_by_component.items()
        for item_id in item_ids
    }
    for item_id, (component, implementation, state) in rows.items():
        expected_component = owner_by_id.get(item_id)
        if expected_component and component != expected_component:
            errors.append(f"{item_id}: component {component!r}, expected {expected_component!r}")
        if not implementation:
            errors.append(f"{item_id}: empty implementation")
        if state not in ALLOWED_STATES:
            errors.append(f"{item_id}: invalid state {state!r}")

    if errors:
        raise VerificationError("; ".join(errors))


def default_tracking_path() -> Path:
    return Path("A:/NATIVESHARP-3.0-TRACKING.md")


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--roadmap",
        type=Path,
        default=repo_root / "mkdown" / "NativeSharp-Language-3.0-Roadmap.md",
    )
    parser.add_argument("--tracking", type=Path, default=default_tracking_path())
    args = parser.parse_args(argv)

    try:
        verify_nsc_roadmap(args.roadmap)
        verify_tracking(args.tracking)
    except VerificationError as error:
        print(f"verify_native_sharp_3_plan: error: {error}", file=sys.stderr)
        return 1

    print(
        "verify_native_sharp_3_plan: OK: "
        f"NSC {len(expected_nsc_feature_ids())}/120; "
        f"tracking {len(COMPONENT_TOTALS)} components/{sum(COMPONENT_TOTALS.values())} items"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
