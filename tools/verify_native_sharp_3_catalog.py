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

LIMITATION_RESOLVED_IDS = {
    *(f"NS3-LIM-CV-{item:03d}" for item in range(1, 14)),
    "NS3-LIM-LINK-001",
    *(f"NS3-LIM-DBG-{item:03d}" for item in range(1, 9)),
    "NS3-LIM-PKG-001",
    "NS3-LIM-IO-001",
}

LIMITATION_OPEN_IDS = {"NS3-LIM-PKG-002", "NS3-LIM-OWN-001"}


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


def verify_master_backlogs(master: Path) -> None:
    text = master.read_text(encoding="utf-8-sig")
    expected = expected_ids_by_component()
    observed = {
        "NSC-Rendimiento": set(
            "NS3-" + item for item in re.findall(r"^### (P\d{2}) —", text, re.MULTILINE)
        ),
        "Runtime/STL": {
            "NS3-R" + item
            for item in re.findall(
                r"^\| E(\d{3})(?: / NS3-R\d{3})? \| [^|\n]+ \|$", text, re.MULTILINE
            )
        },
        "Integración": set(
            re.findall(r"^\| (NS3-I(?:\d{2}|035)) \| [^|\n]+ \| [^|\n]+ \|$", text, re.MULTILINE)
        ),
        "Producto transversal": set(
            re.findall(r"^\| (NS3-X\d{2}) \| [^|\n]+ \|$", text, re.MULTILINE)
        ),
        "Limitaciones importadas": set(
            re.findall(
                r"^\| (NS3-LIM-[A-Z]+-\d{3}) \| [^|\n]+ \| [^|\n]+ \| \d+ \|$",
                text,
                re.MULTILINE,
            )
        ),
    }
    errors = []
    for component, actual in observed.items():
        wanted = expected[component]
        if actual != wanted:
            errors.append(
                f"{component} master backlog mismatch: missing={sorted(wanted - actual)}, "
                f"unexpected={sorted(actual - wanted)}"
            )
    if errors:
        raise VerificationError("; ".join(errors))


def verify_limitation_ledger(ledger: Path) -> None:
    text = ledger.read_text(encoding="utf-8-sig")
    rows: dict[int, list[str]] = {}
    duplicates: list[int] = []
    malformed: list[str] = []
    for line in text.splitlines():
        cells = catalog.split_markdown_row(line)
        if not cells or not re.fullmatch(r"\d+", catalog.plain_cell(cells[0])):
            continue
        number = int(catalog.plain_cell(cells[0]))
        if not 1 <= number <= 156:
            continue
        if number in rows:
            duplicates.append(number)
        rows[number] = cells
        if len(cells) != 5 or any(not catalog.plain_cell(cell) for cell in cells[1:]):
            malformed.append(line)

    expected = set(range(1, 157))
    errors: list[str] = []
    if set(rows) != expected:
        errors.append(
            f"limitation ledger sections differ: missing={sorted(expected - set(rows))}, "
            f"unexpected={sorted(set(rows) - expected)}"
        )
    if duplicates:
        errors.append(f"duplicate limitation ledger sections: {sorted(set(duplicates))}")
    if malformed:
        errors.append(f"malformed limitation ledger rows: {len(malformed)}")
    for heading in ("### Brotli", "### Bzip2", "### Libpng", "### Zlib"):
        if heading not in text:
            errors.append(f"limitation ledger missing port family: {heading[4:]}")
    for source in (
        "limitaciones-compilador-v1.md",
        "limitaciones-compilador-v2.md",
        "limitaciones-conversion-v3.md",
        "limitaciones-conversion-v4.md",
    ):
        if source not in text:
            errors.append(f"limitation ledger missing source acknowledgement: {source}")
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
        if not item.title or not item.contract or not item.evidence:
            errors.append(f"{item.item_id}: empty title, closure contract or evidence/source")
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
    by_id = {item.item_id: item for item in items}
    distribution_contract = by_id["NS3-DST-01"].contract.replace("\\", "/")
    for required in ("C:/native-sharp", "C:/native-sharp-2.0", "A:"):
        if required not in distribution_contract:
            errors.append(
                f"NS3-DST-01: active/rollback policy is missing {required!r}"
            )
    saru_sources = {
        "NS3-SARU-01": "A:/porting-to-3.0.0/SaruFrameworkGLNativeSharp",
        "NS3-SARU-02": "A:/porting-to-3.0.0/SaruFrameworkNativeSharp",
    }
    for item_id, required in saru_sources.items():
        evidence = by_id[item_id].evidence.replace("\\", "/")
        if required not in evidence:
            errors.append(f"{item_id}: canonical Saru source is missing {required!r}")
    for item_id in LIMITATION_RESOLVED_IDS:
        if by_id[item_id].state not in {"TERMINADA", "CERTIFICADA"}:
            errors.append(f"{item_id}: ledger records a directed closure but catalogue is {by_id[item_id].state}")
    for item_id in LIMITATION_OPEN_IDS:
        if by_id[item_id].state in TERMINAL_STATES:
            errors.append(f"{item_id}: ledger still leaves this route open but catalogue is terminal")
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


def verify_component_summary(master: Path, items: list[catalog.CatalogItem]) -> None:
    text = master.read_text(encoding="utf-8-sig")
    begin = text.find(catalog.COMPONENT_SUMMARY_BEGIN)
    end = text.find(catalog.COMPONENT_SUMMARY_END)
    if begin < 0 or end <= begin:
        raise VerificationError("component summary markers are missing from the master document")
    end += len(catalog.COMPONENT_SUMMARY_END)
    observed = text[begin:end]
    expected = catalog.component_summary_section(items)
    if observed != expected:
        raise VerificationError(
            "component summary differs from the canonical catalogue; regenerate with "
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
            Path("A:/native-sharp-3.0-4.0.md"),
            Path("A:/vsBeef.md"),
            Path("A:/NATIVESHARP-3.0-TRACKING - Copy.md"),
            Path("A:/nsc-function-namespace.md"),
            Path("A:/nsc-historia/NativeSharp-3.0-Unified-Language-Compiler-Runtime-Roadmap.md"),
            repo_root / "mkdown" / "NativeSharp-Language-3.0-Roadmap.md",
        ],
    )
    parser.add_argument(
        "--limitation-ledger",
        type=Path,
        default=Path("A:/NATIVESHARP-3.0-LEDGER-LIMITACIONES.md"),
    )
    args = parser.parse_args(argv)
    try:
        verify_roadmap(args.roadmap)
        verify_master_backlogs(args.master)
        verify_limitation_ledger(args.limitation_ledger)
        items = verify_catalogue(args.master, repo_root)
        verify_component_summary(args.master, items)
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
