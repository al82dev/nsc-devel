from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(216, 228)]
EXPECTED_CONTRACTS = [
    *("CSH-DOC-001" for _ in range(6)),
    *("CSH-DOC-002" for _ in range(6)),
]
CASE_ROW = re.compile(
    r'\{"(TSH-\d{5})", "(CSH-DOC-\d{3})", "([^"]+)",\s*\n\s*'
    r'(k[A-Za-z]+Requirement)\}'
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


def module_rows(repository_root: Path) -> list[tuple[str, str, str, str]]:
    module_root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        module_root / "SelfHostDocumentationArchitectureModule.cpp",
        module_root / "SelfHostDocumentationPolicyModule.cpp",
    ]
    rows: list[tuple[str, str, str, str]] = []
    for path in paths:
        rows.extend(CASE_ROW.findall(path.read_text(encoding="utf-8")))
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
        if row["component"] == "SelfHost.Documentation"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Documentation"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Documentation"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00216..TSH-00227")
    if [row["contract_id"] for row in design] != EXPECTED_CONTRACTS:
        raise RuntimeError("design changed the canonical 6/6 contract split")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00216..TSH-00227")
    if len(contracts) != 2:
        raise RuntimeError("contract inventory does not contain exactly two Documentation contracts")
    if any(row["frequency"] != "PR" for row in design):
        raise RuntimeError("design-side PR profile changed without reconciling the known conflict")
    if any(row["default_profile"] != "NIGHTLY" for row in allocation):
        raise RuntimeError("allocation-side NIGHTLY profile changed without reconciliation")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Documentation allocation changed its canonical executor")

    rows = module_rows(repository_root)
    if [row[0] for row in rows] != EXPECTED_IDS:
        raise RuntimeError("consolidated modules do not materialize every case exactly once")
    if [row[1] for row in rows] != EXPECTED_CONTRACTS:
        raise RuntimeError("consolidated modules changed contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostDocumentationTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("modules do not preserve InfrastructurePending disposition")

    conflict_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostDocumentationPlanContradictions.jsonl"
    )
    conflicts = executable_rows(conflict_path)
    if len(conflicts) != 1 or conflicts[0].get("affected_case_ids") != EXPECTED_IDS:
        raise RuntimeError("profile metadata conflict is not recorded for the exact twelve cases")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-documentation.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Documentation executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Documentation" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.Documentation cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
