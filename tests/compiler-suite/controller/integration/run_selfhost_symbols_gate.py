from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(910, 945)]
EXPECTED_RANGES = [
    (910, 916, "CSH-SYM-001"),
    (917, 923, "CSH-SYM-002"),
    (924, 930, "CSH-SYM-003"),
    (931, 937, "CSH-SYM-004"),
    (938, 944, "CSH-SYM-005"),
]
RANGE_ROW = re.compile(r'\{(\d+), (\d+), "(CSH-SYM-\d{3})", "([^"]+)"')


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
        root / "SelfHostSymbolsIdentityModule.cpp",
        root / "SelfHostSymbolsMetadataModule.cpp",
        root / "SelfHostSymbolsScopeModule.cpp",
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
        if row["component"] == "SelfHost.Symbols"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Symbols"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Symbols"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00910..TSH-00944")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00910..TSH-00944")
    if len(contracts) != 5:
        raise RuntimeError("contract inventory does not contain exactly five Symbols contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 34:
        raise RuntimeError("SelfHost.Symbols allocation no longer contains exactly 34 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 1:
        raise RuntimeError("SelfHost.Symbols allocation no longer contains exactly one NIGHTLY case")
    if any(row["layer"] != "unit" for row in allocation):
        raise RuntimeError("SelfHost.Symbols allocation no longer contains exactly 35 unit cases")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Symbols allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed Symbols range or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostSymbolsTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Symbols modules do not preserve InfrastructurePending disposition")
    if 'ordinal == 937 ? "NIGHTLY" : "PR"' not in support:
        raise RuntimeError("Symbols support does not preserve the exact NIGHTLY case")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-symbols.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Symbols executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Symbols" for row in manifest_rows(central)):
        raise RuntimeError("pending SelfHost.Symbols cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
