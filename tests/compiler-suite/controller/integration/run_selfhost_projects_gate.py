from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(601, 696)]
EXPECTED_RANGES = [
    (601, 607, "CSH-PRJ-001", "unit"),
    (608, 614, "CSH-PRJ-002", "component"),
    (615, 622, "CSH-PRJ-003", "component"),
    (623, 630, "CSH-PRJ-004", "unit"),
    (631, 638, "CSH-PRJ-005", "integration"),
    (639, 645, "CSH-PRJ-006", "unit"),
    (646, 653, "CSH-PRJ-007", "integration"),
    (654, 661, "CSH-PRJ-008", "component"),
    (662, 669, "CSH-PRJ-009", "unit"),
    (670, 678, "CSH-PRJ-010", "component"),
    (679, 687, "CSH-PRJ-011", "integration"),
    (688, 695, "CSH-PRJ-012", "unit"),
]
RANGE_ROW = re.compile(
    r'\{(\d+), (\d+), "(CSH-PRJ-\d{3})", "(unit|component|integration)", "([^"]+)"'
)


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


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostProjectsModelModule.cpp",
        root / "SelfHostProjectsIdentityModule.cpp",
        root / "SelfHostProjectsArtifactsModule.cpp",
        root / "SelfHostProjectsGraphModule.cpp",
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
        if row["component"] == "SelfHost.Projects"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Projects"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Projects"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00601..TSH-00695")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00601..TSH-00695")
    if len(contracts) != 12:
        raise RuntimeError("contract inventory does not contain exactly twelve Projects contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 82:
        raise RuntimeError("SelfHost.Projects allocation no longer contains exactly 82 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 13:
        raise RuntimeError("SelfHost.Projects allocation no longer contains exactly 13 NIGHTLY cases")
    if sum(row["layer"] == "unit" for row in allocation) != 38:
        raise RuntimeError("SelfHost.Projects allocation no longer contains exactly 38 unit cases")
    if sum(row["layer"] == "component" for row in allocation) != 32:
        raise RuntimeError("SelfHost.Projects allocation no longer contains exactly 32 component cases")
    if sum(row["layer"] == "integration" for row in allocation) != 25:
        raise RuntimeError("SelfHost.Projects allocation no longer contains exactly 25 integration cases")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Projects allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed Projects range, layer, or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostProjectsTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Projects modules do not preserve InfrastructurePending disposition")
    if "std::array<int, 13>" not in support:
        raise RuntimeError("Projects support does not preserve the exact NIGHTLY denominator")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-projects.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Projects executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Projects" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.Projects cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
