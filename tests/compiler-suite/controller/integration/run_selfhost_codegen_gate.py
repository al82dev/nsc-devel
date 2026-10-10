from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"TSH-{ordinal:05d}" for ordinal in range(52, 188)]
DESCRIPTOR = re.compile(
    r'\{(\d+)U, (\d+)U, (\d+)U, (\d+)U, "(unit|component|integration)",\s*'
    r'"([^"]+)"\}'
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


def descriptors(repository_root: Path) -> list[tuple[int, int, int, int, str, str]]:
    module_root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        module_root / "SelfHostCodeGenFoundationModule.cpp",
        module_root / "SelfHostCodeGenLoweringEmissionModule.cpp",
    ]
    found: list[tuple[int, int, int, int, str, str]] = []
    for path in paths:
        for match in DESCRIPTOR.findall(path.read_text(encoding="utf-8")):
            ordinal, first, count, nightly, layer, oracle = match
            found.append((int(ordinal), int(first), int(count), int(nightly), layer, oracle))
    return found


def main() -> int:
    """Validate CodeGen materialization and classification without executing the product."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    root = options.repository_root.resolve()
    inventory = root / "docs" / "compiler-testing-reengineering" / "inventory"

    design = [
        row
        for row in read_csv(inventory / "test-case-design.csv")
        if row["component"] == "SelfHost.CodeGen"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.CodeGen"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.CodeGen"
    ]
    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00052..TSH-00187")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00052..TSH-00187")
    if len(contracts) != 19:
        raise RuntimeError("contract inventory does not contain exactly 19 CodeGen contracts")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.CodeGen allocation changed its canonical executor")

    materialized: list[tuple[str, str, str, str, str]] = []
    for ordinal, first, count, nightly_count, layer, oracle in descriptors(root):
        contract_id = f"CSH-CG-{ordinal:03d}"
        for offset in range(count):
            profile = "NIGHTLY" if offset >= count - nightly_count else "PR"
            materialized.append(
                (f"TSH-{first + offset:05d}", contract_id, layer, profile, oracle)
            )

    expected = [
        (
            row["case_id"],
            row["contract_id"],
            row["layer"],
            row["frequency"],
            next(
                contract["observable_oracle"]
                for contract in contracts
                if contract["contract_id"] == row["contract_id"]
            ),
        )
        for row in design
    ]
    if materialized != expected:
        raise RuntimeError("module descriptors do not preserve canonical case lineage")
    if len(materialized) != 136 or len({row[0] for row in materialized}) != 136:
        raise RuntimeError("modules do not materialize 136 unique CodeGen cases")

    layers = [row[2] for row in materialized]
    profiles = [row[3] for row in materialized]
    if (layers.count("unit"), layers.count("component"), layers.count("integration")) != (
        37,
        75,
        24,
    ):
        raise RuntimeError("CodeGen layer distribution is not canonical 37/75/24")
    if (profiles.count("PR"), profiles.count("NIGHTLY")) != (120, 16):
        raise RuntimeError("CodeGen profile distribution is not canonical 120/16")

    support = (
        root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostCodeGenTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("CodeGen cases lack InfrastructurePending disposition")
    if "TestDisposition::KnownProductDefect" in support:
        raise RuntimeError("CodeGen asserts a product defect without an executable oracle")

    fragments = root / "tests" / "compiler-suite" / "manifest" / "fragments"
    if fragments.joinpath("selfhost-codegen.jsonl").exists():
        raise RuntimeError("zero-executable CodeGen must not retain an empty fragment")
    central = root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.CodeGen" for row in executable_rows(central)):
        raise RuntimeError("pending SelfHost.CodeGen cases entered the executable manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
