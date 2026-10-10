from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(452, 579)]
EXPECTED_RANGES = [
    (452, 458, "CSH-PAR-001", "unit"),
    (459, 465, "CSH-PAR-002", "unit"),
    (466, 472, "CSH-PAR-003", "component"),
    (473, 479, "CSH-PAR-004", "unit"),
    (480, 486, "CSH-PAR-005", "component"),
    (487, 495, "CSH-PAR-006", "component"),
    (496, 503, "CSH-PAR-007", "component"),
    (504, 512, "CSH-PAR-008", "component"),
    (513, 519, "CSH-PAR-009", "component"),
    (520, 527, "CSH-PAR-010", "component"),
    (528, 536, "CSH-PAR-011", "component"),
    (537, 545, "CSH-PAR-012", "component"),
    (546, 553, "CSH-PAR-013", "component"),
    (554, 561, "CSH-PAR-014", "component"),
    (562, 569, "CSH-PAR-015", "component"),
    (570, 578, "CSH-PAR-016", "integration"),
]
RANGE_ROW = re.compile(
    r'\{(\d+), (\d+), "(CSH-PAR-\d{3})", "(unit|component|integration)", "([^"]+)"'
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
        root / "SelfHostParserTypesLiteralsModule.cpp",
        root / "SelfHostParserExpressionsModule.cpp",
        root / "SelfHostParserStatementsModule.cpp",
        root / "SelfHostParserDeclarationsModule.cpp",
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
        if row["component"] == "SelfHost.Parser"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Parser"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Parser"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00452..TSH-00578")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00452..TSH-00578")
    if len(contracts) != 16:
        raise RuntimeError("contract inventory does not contain exactly sixteen Parser contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 107:
        raise RuntimeError("SelfHost.Parser allocation no longer contains exactly 107 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 20:
        raise RuntimeError("SelfHost.Parser allocation no longer contains exactly 20 NIGHTLY cases")
    if sum(row["layer"] == "unit" for row in allocation) != 21:
        raise RuntimeError("SelfHost.Parser allocation no longer contains exactly 21 unit cases")
    if sum(row["layer"] == "component" for row in allocation) != 97:
        raise RuntimeError("SelfHost.Parser allocation no longer contains exactly 97 component cases")
    if sum(row["layer"] == "integration" for row in allocation) != 9:
        raise RuntimeError("SelfHost.Parser allocation no longer contains exactly 9 integration cases")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Parser allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed Parser range, layer, or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostParserTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Parser modules do not preserve InfrastructurePending disposition")
    if "std::array<int, 20>" not in support:
        raise RuntimeError("Parser support does not preserve the exact NIGHTLY denominator")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-parser.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Parser executable fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Parser" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.Parser cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
