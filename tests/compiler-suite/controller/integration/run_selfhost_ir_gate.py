from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(307, 399)]
EXPECTED_RANGES = [
    (307, 314, "CSH-IR-001", "unit"),
    (315, 323, "CSH-IR-002", "component"),
    (324, 330, "CSH-IR-003", "unit"),
    (331, 337, "CSH-IR-004", "unit"),
    (338, 346, "CSH-IR-005", "component"),
    (347, 353, "CSH-IR-006", "component"),
    (354, 361, "CSH-IR-007", "component"),
    (362, 368, "CSH-IR-008", "unit"),
    (369, 376, "CSH-IR-009", "component"),
    (377, 383, "CSH-IR-010", "unit"),
    (384, 391, "CSH-IR-011", "unit"),
    (392, 398, "CSH-IR-012", "integration"),
]
RANGE_ROW = re.compile(
    r'\{(\d+), (\d+), "(CSH-IR-\d{3})", "(unit|component|integration)", "([^"]+)"'
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
        root / "SelfHostIrHirFlowModule.cpp",
        root / "SelfHostIrMirVerificationModule.cpp",
        root / "SelfHostIrDominanceLifecycleModule.cpp",
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
        if row["component"] == "SelfHost.IR"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.IR"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.IR"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00307..TSH-00398")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00307..TSH-00398")
    if len(contracts) != 12:
        raise RuntimeError("contract inventory does not contain exactly twelve IR contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 79:
        raise RuntimeError("SelfHost.IR allocation no longer contains exactly 79 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 13:
        raise RuntimeError("SelfHost.IR allocation no longer contains exactly 13 NIGHTLY cases")
    if sum(row["layer"] == "unit" for row in allocation) != 44:
        raise RuntimeError("SelfHost.IR allocation no longer contains exactly 44 unit cases")
    if sum(row["layer"] == "component" for row in allocation) != 41:
        raise RuntimeError("SelfHost.IR allocation no longer contains exactly 41 component cases")
    if sum(row["layer"] == "integration" for row in allocation) != 7:
        raise RuntimeError("SelfHost.IR allocation no longer contains exactly 7 integration cases")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.IR allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed IR range, layer, or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostIrTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("IR modules do not preserve InfrastructurePending disposition")
    if "std::array<int, 13>" not in support:
        raise RuntimeError("IR support does not preserve the exact NIGHTLY denominator")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-ir.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.IR executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.IR" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.IR cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
