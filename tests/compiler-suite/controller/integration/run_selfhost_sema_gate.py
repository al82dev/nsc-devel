from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(696, 849)]
EXPECTED_RANGES = [
    (696, 701, "CSH-SEM-001", "unit"),
    (702, 710, "CSH-SEM-002", "component"),
    (711, 719, "CSH-SEM-003", "component"),
    (720, 728, "CSH-SEM-004", "component"),
    (729, 736, "CSH-SEM-005", "unit"),
    (737, 745, "CSH-SEM-006", "component"),
    (746, 753, "CSH-SEM-007", "component"),
    (754, 760, "CSH-SEM-008", "integration"),
    (761, 769, "CSH-SEM-009", "integration"),
    (770, 777, "CSH-SEM-010", "unit"),
    (778, 783, "CSH-SEM-011", "unit"),
    (784, 791, "CSH-SEM-012", "component"),
    (792, 799, "CSH-SEM-013", "component"),
    (800, 806, "CSH-SEM-014", "component"),
    (807, 812, "CSH-SEM-015", "unit"),
    (813, 819, "CSH-SEM-016", "component"),
    (820, 826, "CSH-SEM-017", "component"),
    (827, 833, "CSH-SEM-018", "component"),
    (834, 840, "CSH-SEM-019", "integration"),
    (841, 848, "CSH-SEM-020", "unit"),
]
RANGE_ROW = re.compile(
    r'\{(\d+), (\d+), "(CSH-SEM-\d{3})", "(unit|component|integration)", "([^"]+)"'
)


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


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostSemaModelModule.cpp",
        root / "SelfHostSemaModuleSetModule.cpp",
        root / "SelfHostSemaResolutionModule.cpp",
        root / "SelfHostSemaAnalysisModule.cpp",
    ]
    rows: list[tuple[int, int, str, str]] = []
    for path in paths:
        for first, last, contract, layer, _oracle in RANGE_ROW.findall(
            path.read_text(encoding="utf-8")
        ):
            rows.append((int(first), int(last), contract, layer))
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
        if row["component"] == "SelfHost.Sema"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Sema"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Sema"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00696..TSH-00848")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00696..TSH-00848")
    if len(contracts) != 20:
        raise RuntimeError("contract inventory does not contain exactly twenty Sema contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 130:
        raise RuntimeError("SelfHost.Sema allocation no longer contains exactly 130 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 23:
        raise RuntimeError("SelfHost.Sema allocation no longer contains exactly 23 NIGHTLY cases")
    if sum(row["layer"] == "unit" for row in allocation) != 42:
        raise RuntimeError("SelfHost.Sema allocation no longer contains exactly 42 unit cases")
    if sum(row["layer"] == "component" for row in allocation) != 88:
        raise RuntimeError("SelfHost.Sema allocation no longer contains exactly 88 component cases")
    if sum(row["layer"] == "integration" for row in allocation) != 23:
        raise RuntimeError("SelfHost.Sema allocation no longer contains exactly 23 integration cases")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Sema allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed Sema range, layer, or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostSemaTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Sema modules do not preserve InfrastructurePending disposition")
    if "std::array<int, 23>" not in support:
        raise RuntimeError("Sema support does not preserve the exact NIGHTLY denominator")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-sema.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Sema executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Sema" for row in manifest_rows(central)):
        raise RuntimeError("pending SelfHost.Sema cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
