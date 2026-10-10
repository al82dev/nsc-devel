from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(579, 601)]
EXPECTED_RANGES = [
    (579, 586, "CSH-PRO-001"),
    (587, 593, "CSH-PRO-002"),
    (594, 600, "CSH-PRO-003"),
]
RANGE_ROW = re.compile(r"\{\s*(\d+),\s*(\d+),\s*\"(CSH-PRO-\d{3})\"")


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
        root / "SelfHostProjectIdentityModule.cpp",
        root / "SelfHostProjectDependenciesModule.cpp",
        root / "SelfHostProjectInventoryModule.cpp",
    ]
    rows: list[tuple[int, int, str]] = []
    for path in paths:
        for first, last, contract in RANGE_ROW.findall(path.read_text(encoding="utf-8")):
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
        if row["component"] == "SelfHost.Project"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Project"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Project"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00579..TSH-00600")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00579..TSH-00600")
    if len(contracts) != 3:
        raise RuntimeError("contract inventory does not contain exactly three Project contracts")
    if any(row["default_profile"] != "NIGHTLY" for row in allocation):
        raise RuntimeError("SelfHost.Project allocation no longer assigns all 22 cases to NIGHTLY")
    if any(row["layer"] != "build-contract" for row in allocation):
        raise RuntimeError("SelfHost.Project allocation changed its build-contract layer")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Project allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed Project range or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostProjectTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Project modules do not preserve InfrastructurePending disposition")
    contradiction = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostProjectPlanContradictions.jsonl"
    )
    records = executable_rows(contradiction)
    if len(records) != 1 or records[0].get("status") != "OPEN":
        raise RuntimeError("Project plan/allocation profile contradiction is not preserved")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-project.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Project executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Project" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.Project cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
