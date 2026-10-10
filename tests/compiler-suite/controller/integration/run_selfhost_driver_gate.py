from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(228, 307)]
EXPECTED_RANGES = [
    (228, 235, "CSH-DRV-001"),
    (236, 244, "CSH-DRV-002"),
    (245, 252, "CSH-DRV-003"),
    (253, 259, "CSH-DRV-004"),
    (260, 267, "CSH-DRV-005"),
    (268, 275, "CSH-DRV-006"),
    (276, 283, "CSH-DRV-007"),
    (284, 292, "CSH-DRV-008"),
    (293, 299, "CSH-DRV-009"),
    (300, 306, "CSH-DRV-010"),
]
RANGE_ROW = re.compile(r'\{(\d+), (\d+), "(CSH-DRV-\d{3})", "([^"]+)"')


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def executable_rows(path: Path) -> list[dict[str, object]]:
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
        root / "SelfHostDriverCliFrontendModule.cpp",
        root / "SelfHostDriverEmissionModule.cpp",
        root / "SelfHostDriverAuditDispatchModule.cpp",
    ]
    rows: list[tuple[int, int, str]] = []
    for path in paths:
        for first, last, contract, _oracle in RANGE_ROW.findall(path.read_text(encoding="utf-8")):
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
        if row["component"] == "SelfHost.Driver"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Driver"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Driver"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00228..TSH-00306")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00228..TSH-00306")
    if len(contracts) != 10:
        raise RuntimeError("contract inventory does not contain exactly ten Driver contracts")
    if any(row["layer"] != "process" for row in allocation):
        raise RuntimeError("SelfHost.Driver allocation changed its process layer")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Driver allocation changed its canonical executor")
    if sum(row["default_profile"] == "PR" for row in allocation) != 64:
        raise RuntimeError("SelfHost.Driver allocation no longer contains exactly 64 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 15:
        raise RuntimeError("SelfHost.Driver allocation no longer contains exactly 15 NIGHTLY cases")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed Driver range or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostDriverTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Driver modules do not preserve InfrastructurePending disposition")
    if "std::array<int, 15>" not in support:
        raise RuntimeError("Driver support does not preserve the exact NIGHTLY denominator")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-driver.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Driver executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Driver" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.Driver cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
