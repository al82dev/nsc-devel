#!/usr/bin/env python3
"""Verify the single-source NativeSharp 3.0 master catalogue and generated tracking."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

import native_sharp_3_catalog as catalog


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
    "NSC-Rendimiento": 12,
    "Runtime/STL": 54,
    "NSLink": 9,
    "NSASM": 7,
    "NSDBG": 14,
    "NSC-LSP": 10,
    "NSC-VSCode": 7,
    "Bindings": 7,
    "Project Manager": 7,
    "Distribución": 7,
    "Saru": 4,
    "Integración": 10,
    "Producto transversal": 10,
    "Limitaciones importadas": 26,
    "Gobernanza documental": 4,
}

TERMINAL_STATES = {"TERMINADA", "CERTIFICADA", "DIFERIDA", "NO APLICA"}


class VerificationError(RuntimeError):
    pass


def expected_nsc_feature_ids() -> set[str]:
    return {
        f"NS3-T{train:02d}-F{feature:02d}"
        for train, count in FEATURE_COUNTS.items()
        for feature in range(1, count + 1)
    }


def expected_ids_by_component() -> dict[str, set[str]]:
    return {
        "NSC": expected_nsc_feature_ids(),
        "NSC-Rendimiento": {f"NS3-P{item:02d}" for item in range(12)},
        "Runtime/STL": {f"NS3-R{item:03d}" for item in range(1, 55)},
        "NSLink": {f"NS3-LNK-{item:02d}" for item in range(1, 10)},
        "NSASM": {f"NS3-ASM-{item:02d}" for item in range(1, 8)},
        "NSDBG": {f"NS3-DBG-{item:02d}" for item in range(1, 15)},
        "NSC-LSP": {f"NS3-LSP-{item:02d}" for item in range(1, 11)},
        "NSC-VSCode": {f"NS3-VSC-{item:02d}" for item in range(1, 8)},
        "Bindings": {f"NS3-BND-{item:02d}" for item in range(1, 8)},
        "Project Manager": {f"NS3-PM-{item:02d}" for item in range(1, 8)},
        "Distribución": {f"NS3-DST-{item:02d}" for item in range(1, 8)},
        "Saru": {f"NS3-SARU-{item:02d}" for item in range(1, 5)},
        "Integración": {
            "NS3-I00",
            "NS3-I01",
            "NS3-I02",
            "NS3-I03",
            "NS3-I035",
            "NS3-I04",
            "NS3-I05",
            "NS3-I06",
            "NS3-I07",
            "NS3-I08",
        },
        "Producto transversal": {f"NS3-X{item:02d}" for item in range(1, 11)},
        "Limitaciones importadas": {
            *(f"NS3-LIM-CV-{item:03d}" for item in range(1, 14)),
            "NS3-LIM-LINK-001",
            *(f"NS3-LIM-DBG-{item:03d}" for item in range(1, 9)),
            "NS3-LIM-PKG-001",
            "NS3-LIM-PKG-002",
            "NS3-LIM-IO-001",
            "NS3-LIM-OWN-001",
        },
        "Gobernanza documental": {f"NS3-DOC-{item:02d}" for item in range(1, 5)},
    }


def verify_roadmap(roadmap: Path) -> None:
    text = roadmap.read_text(encoding="utf-8-sig")
    expected = {
        "NSC": expected_nsc_feature_ids(),
        "NSLink": {f"NS3-LNK-{item:02d}" for item in range(1, 10)},
        "NSDBG": {f"NS3-DBG-{item:02d}" for item in range(1, 15)},
        "NSC-LSP": {f"NS3-LSP-{item:02d}" for item in range(1, 11)},
        "NSC-VSCode": {f"NS3-VSC-{item:02d}" for item in range(1, 8)},
        "NSASM": {f"NS3-ASM-{item:02d}" for item in range(1, 8)},
    }
    patterns = {
        "NSC": r"^## (NS3-T\d{2}-F\d{2})\b",
        "NSLink": r"^#### (NS3-LNK-\d{2})\b",
        "NSDBG": r"^#### (NS3-DBG-\d{2})\b",
        "NSC-LSP": r"^#### (NS3-LSP-\d{2})\b",
        "NSC-VSCode": r"^#### (NS3-VSC-\d{2})\b",
        "NSASM": r"^#### (NS3-ASM-\d{2})\b",
    }
    errors: list[str] = []
    for component, item_ids in expected.items():
        headings = re.findall(patterns[component], text, re.MULTILINE)
        counts = Counter(headings)
        actual = set(headings)
        if actual != item_ids:
            errors.append(
                f"{component} roadmap mismatch: missing={sorted(item_ids - actual)}, "
                f"unexpected={sorted(actual - item_ids)}"
            )
        duplicates = sorted(item for item, count in counts.items() if count != 1)
        if duplicates:
            errors.append(f"{component} duplicate roadmap IDs: {duplicates}")
    if errors:
        raise VerificationError("; ".join(errors))


def _git_commit_exists(repo_root: Path, commit: str) -> bool:
    result = subprocess.run(
        ["git", "cat-file", "-e", commit + "^{commit}"],
        cwd=repo_root,
        capture_output=True,
    )
    return result.returncode == 0


def verify_catalogue(master: Path, repo_root: Path) -> list[catalog.CatalogItem]:
    try:
        items = catalog.parse_catalogue(master)
    except RuntimeError as error:
        raise VerificationError(str(error)) from error
    expected_by_component = expected_ids_by_component()
    expected_ids = set().union(*expected_by_component.values())
    actual_ids = {item.item_id for item in items}
    errors: list[str] = []
    if actual_ids != expected_ids:
        errors.append(
            f"canonical ID mismatch: missing={sorted(expected_ids - actual_ids)}, "
            f"unexpected={sorted(actual_ids - expected_ids)}"
        )
    counts = Counter(item.component for item in items)
    if dict(counts) != COMPONENT_TOTALS:
        errors.append(f"component counts differ: {dict(counts)!r} != {COMPONENT_TOTALS!r}")
    owner = {
        item_id: component
        for component, item_ids in expected_by_component.items()
        for item_id in item_ids
    }
    for item in items:
        if owner.get(item.item_id) != item.component:
            errors.append(
                f"{item.item_id}: component {item.component!r}, expected {owner.get(item.item_id)!r}"
            )
        if item.state not in catalog.ALLOWED_STATES:
            errors.append(f"{item.item_id}: invalid state {item.state!r}")
        if not item.title or not item.contract:
            errors.append(f"{item.item_id}: empty title or closure contract")
        if item.state in TERMINAL_STATES and not item.evidence:
            errors.append(f"{item.item_id}: terminal state without evidence/decision")
        if item.component == "NSC" and item.state in {"TERMINADA", "CERTIFICADA"}:
            docs = list((repo_root / "docs" / "3.0" / "items").glob(item.item_id + "-*.md"))
            if len(docs) != 1:
                errors.append(f"{item.item_id}: expected one item document, found {len(docs)}")
            commit_match = re.search(r"\b[0-9a-f]{7,40}\b", item.evidence)
            if not commit_match:
                errors.append(f"{item.item_id}: completed NSC item has no commit hash")
            elif not _git_commit_exists(repo_root, commit_match.group(0)):
                errors.append(f"{item.item_id}: evidence commit does not exist")
    if errors:
        raise VerificationError("; ".join(errors))
    return items


def verify_tracking(tracking: Path, items: list[catalog.CatalogItem]) -> None:
    if not tracking.is_file():
        raise VerificationError(f"tracking not found: {tracking}")
    observed = tracking.read_text(encoding="utf-8-sig")
    expected = catalog.render_tracking(items)
    if observed != expected:
        raise VerificationError(
            "tracking differs from the canonical master; regenerate with "
            "tools/native_sharp_3_catalog.py"
        )


def verify_secondary_authority_notice(paths: list[Path]) -> None:
    marker = "Documento maestro §24"
    missing = [str(path) for path in paths if marker not in path.read_text(encoding="utf-8-sig")]
    if missing:
        raise VerificationError("secondary documents lack authority notice: " + ", ".join(missing))


def main(argv: list[str] | None = None) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--master", type=Path, default=Path("A:/DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md"))
    parser.add_argument("--tracking", type=Path, default=Path("A:/NATIVESHARP-3.0-TRACKING.md"))
    parser.add_argument(
        "--roadmap",
        type=Path,
        default=repo_root / "mkdown" / "NativeSharp-Language-3.0-Roadmap.md",
    )
    parser.add_argument(
        "--secondary",
        type=Path,
        action="append",
        default=[
            Path("A:/NATIVESHARP-3.0-LEDGER-LIMITACIONES.md"),
            Path("A:/NATIVESHARP-3.0-PENDIENTES-POR-COMPONENTE.md"),
        ],
    )
    args = parser.parse_args(argv)
    try:
        verify_roadmap(args.roadmap)
        items = verify_catalogue(args.master, repo_root)
        verify_tracking(args.tracking, items)
        verify_secondary_authority_notice(args.secondary)
    except (OSError, VerificationError, RuntimeError) as error:
        print(f"verify_native_sharp_3_catalog: error: {error}", file=sys.stderr)
        return 1
    counts = Counter(item.state for item in items)
    print(
        "verify_native_sharp_3_catalog: OK: "
        f"{len(items)} items/{len(COMPONENT_TOTALS)} components; "
        + ", ".join(f"{state}={counts[state]}" for state in sorted(counts))
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
