from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(29, 52)]
EXPECTED_CONTRACTS = [
    *("CSH-BOO-001" for _ in range(8)),
    *("CSH-BOO-002" for _ in range(8)),
    *("CSH-BOO-003" for _ in range(7)),
]
CASE_ROW = re.compile(
    r'\{"(TSH-\d{5})", "(CSH-BOO-\d{3})", "(component|integration)", '
    r'"(PR|NIGHTLY)",\s*\n\s*"([^"]+)", (k[A-Za-z]+Requirement)\}'
)


def read_csv(path: Path) -> list[dict[str, str]]:
    """Read one canonical Phase 1 inventory without changing its published order."""
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def executable_rows(path: Path) -> list[dict[str, object]]:
    """Return JSON objects while allowing documentary comments in an empty fragment."""
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


def module_rows(repository_root: Path) -> list[tuple[str, str, str, str, str, str]]:
    module_root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        module_root / "SelfHostBootstrapProfileModule.cpp",
        module_root / "SelfHostBootstrapBuildModule.cpp",
    ]
    rows: list[tuple[str, str, str, str, str, str]] = []
    for path in paths:
        rows.extend(CASE_ROW.findall(path.read_text(encoding="utf-8")))
    return rows


def main() -> int:
    """Validate denominator, lineage, pending disposition, and empty executable fragment."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"

    design = [
        row
        for row in read_csv(inventory / "test-case-design.csv")
        if row["component"] == "SelfHost.Bootstrap"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Bootstrap"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Bootstrap"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00029..TSH-00051")
    if [row["contract_id"] for row in design] != EXPECTED_CONTRACTS:
        raise RuntimeError("design does not preserve the canonical 8/8/7 contract split")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00029..TSH-00051")
    if len(contracts) != 3:
        raise RuntimeError("contract inventory does not contain exactly three Bootstrap contracts")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Bootstrap allocation changed its canonical executor")

    rows = module_rows(repository_root)
    if [row[0] for row in rows] != EXPECTED_IDS:
        raise RuntimeError("consolidated modules do not materialize every case exactly once")
    if [row[1] for row in rows] != EXPECTED_CONTRACTS:
        raise RuntimeError("consolidated modules changed contract lineage")
    if [row[2] for row in rows].count("component") != 16:
        raise RuntimeError("module layer distribution is not 16 component cases")
    if [row[2] for row in rows].count("integration") != 7:
        raise RuntimeError("module layer distribution is not 7 integration cases")
    if [row[3] for row in rows].count("PR") != 18:
        raise RuntimeError("module profile distribution is not 18 PR cases")
    if [row[3] for row in rows].count("NIGHTLY") != 5:
        raise RuntimeError("module profile distribution is not 5 NIGHTLY cases")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostBootstrapTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("modules do not preserve InfrastructurePending disposition")
    if "TestDisposition::KnownProductDefect" in support:
        raise RuntimeError("a product defect was asserted without an executable oracle")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-bootstrap.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Bootstrap executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Bootstrap" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.Bootstrap cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
