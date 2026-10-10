from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(945, 981)]
EXPECTED_RANGES = [
    (945, 950, "CSH-SYN-001"),
    (951, 958, "CSH-SYN-002"),
    (959, 965, "CSH-SYN-003"),
    (966, 973, "CSH-SYN-004"),
    (974, 980, "CSH-SYN-005"),
]
RANGE_ROW = re.compile(r'\{(\d+), (\d+), "(CSH-SYN-\d{3})", "([^"]+)"')


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def manifest_rows(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        payload = json.loads(line)
        if not isinstance(payload, dict):
            raise RuntimeError(f"manifest row {path}:{line_number} is not an object")
        rows.append(payload)
    return rows


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostSyntaxNodesModule.cpp",
        root / "SelfHostSyntaxTreeModule.cpp",
        root / "SelfHostSyntaxDumpModule.cpp",
    ]
    rows: list[tuple[int, int, str]] = []
    for path in paths:
        for first, last, contract, _oracle in RANGE_ROW.findall(
            path.read_text(encoding="utf-8")
        ):
            rows.append((int(first), int(last), contract))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [
        row
        for row in read_csv(inventory / "test-case-design.csv")
        if row["component"] == "SelfHost.Syntax"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Syntax"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Syntax"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00945..TSH-00980")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00945..TSH-00980")
    if len(contracts) != 5:
        raise RuntimeError("contract inventory does not contain exactly five Syntax contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 33:
        raise RuntimeError("SelfHost.Syntax allocation no longer contains exactly 33 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 3:
        raise RuntimeError("SelfHost.Syntax allocation no longer contains exactly three NIGHTLY cases")
    if any(row["layer"] != "unit" for row in allocation):
        raise RuntimeError("SelfHost.Syntax allocation no longer contains exactly 36 unit cases")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Syntax allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed Syntax range or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostSyntaxTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Syntax modules do not preserve InfrastructurePending disposition")
    if "std::array<int, 3>" not in support:
        raise RuntimeError("Syntax support does not preserve the exact NIGHTLY denominator")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-syntax.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Syntax executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Syntax" for row in manifest_rows(central)):
        raise RuntimeError("pending SelfHost.Syntax cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
