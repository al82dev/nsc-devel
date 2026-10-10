from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite import InventoryIndex, load_manifest
from component_gate_support import read_csv


RegistryRow = tuple[str, str, str]


def expected_registry_rows(
    repository_root: Path, source_root: Path
) -> tuple[RegistryRow, ...]:
    design_path = (
        repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
        / "test-case-design.csv"
    )
    expected = [
        (row["case_id"], row["contract_id"], row["component"])
        for row in read_csv(
            design_path,
            ("case_id", "contract_id", "component"),
        )
        if row["component"].startswith(("Host.", "SelfHost."))
    ]

    fixture = source_root / "fixtures" / "infrastructure"
    inventory = InventoryIndex.from_csv(
        fixture / "components.csv",
        fixture / "contracts.csv",
    )
    expected.extend(
        (case.case_id, case.contract_id, case.component)
        for case in load_manifest(fixture / "cases.jsonl", inventory)
    )
    ordered = tuple(sorted(expected))
    identities = tuple(row[0] for row in ordered)
    if len(identities) != len(set(identities)):
        raise RuntimeError("canonical host registry inventory contains duplicate case IDs")
    return ordered


def parse_registry_output(output: str) -> tuple[RegistryRow, ...]:
    rows: list[RegistryRow] = []
    seen: set[str] = set()
    for line_number, line in enumerate(output.splitlines(), start=1):
        fields = line.split("\t")
        if len(fields) != 3 or any(not field for field in fields):
            raise RuntimeError(f"malformed host registry row at line {line_number}")
        case_id, contract_id, component = fields
        if case_id in seen:
            raise RuntimeError(f"duplicate host registry case ID: {case_id}")
        seen.add(case_id)
        rows.append((case_id, contract_id, component))
    if not rows:
        raise RuntimeError("host registry is empty")
    if rows != sorted(rows):
        raise RuntimeError("host registry is not ordered by case identity")
    return tuple(rows)


def listed_registry_rows(host_executable: Path) -> tuple[RegistryRow, ...]:
    completed = subprocess.run(
        [str(host_executable), "--list"],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=60,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"host registry listing returned {completed.returncode}: "
            f"{completed.stderr.strip()}"
        )
    return parse_registry_output(completed.stdout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()

    expected = expected_registry_rows(options.repository_root, options.source_root)
    actual = listed_registry_rows(options.host_executable)
    if actual != expected:
        expected_by_id = {row[0]: row[1:] for row in expected}
        actual_by_id = {row[0]: row[1:] for row in actual}
        missing = sorted(set(expected_by_id) - set(actual_by_id))
        unexpected = sorted(set(actual_by_id) - set(expected_by_id))
        changed = sorted(
            case_id
            for case_id in set(expected_by_id) & set(actual_by_id)
            if expected_by_id[case_id] != actual_by_id[case_id]
        )
        raise RuntimeError(
            "host registry differs from canonical inventory; "
            f"missing={missing}; unexpected={unexpected}; changed={changed}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
