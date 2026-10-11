from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite import InventoryIndex, load_manifest
from component_gate_support import read_csv, read_jsonl


RegistryRow = tuple[str, str, str]
EXTERNAL_EXECUTOR = "nsc_suite_controller+nsc"
FUTURE_INVENTORY_FIELDS = {
    "case_id",
    "component",
    "contract_id",
    "disposition",
    "required_profile",
}


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


def future_inventory_rows(
    source_root: Path, expected: tuple[RegistryRow, ...]
) -> tuple[RegistryRow, ...]:
    """Load the versioned future-only surface without making it executable."""
    path = source_root / "manifest" / "future-infrastructure.jsonl"
    rows = read_jsonl(path)
    if not rows:
        raise RuntimeError("future infrastructure inventory is empty")

    expected_by_id = {row[0]: row for row in expected}
    manifest_ids = {
        row.get("case_id")
        for row in read_jsonl(source_root / "manifest" / "cases.jsonl")
    }
    future: list[RegistryRow] = []
    seen: set[str] = set()
    for line_number, row in enumerate(rows, start=1):
        if set(row) != FUTURE_INVENTORY_FIELDS:
            raise RuntimeError(
                f"future infrastructure row {line_number} has invalid fields"
            )
        if any(not isinstance(row[field], str) or not row[field] for field in row):
            raise RuntimeError(
                f"future infrastructure row {line_number} has an empty field"
            )
        if row["disposition"] != "InfrastructurePending":
            raise RuntimeError(
                f"future infrastructure row {line_number} has invalid disposition"
            )
        case_id = row["case_id"]
        if case_id in seen:
            raise RuntimeError(f"duplicate future infrastructure case ID: {case_id}")
        seen.add(case_id)
        canonical = expected_by_id.get(case_id)
        candidate = (case_id, row["contract_id"], row["component"])
        if canonical is None:
            raise RuntimeError(f"future infrastructure case is not canonical: {case_id}")
        if candidate != canonical:
            raise RuntimeError(
                f"future infrastructure metadata differs from design: {case_id}"
            )
        if case_id in manifest_ids:
            raise RuntimeError(
                f"future infrastructure case is present in executable manifest: {case_id}"
            )
        future.append(candidate)
    return tuple(sorted(future))


def external_inventory_rows(
    repository_root: Path, source_root: Path, expected: tuple[RegistryRow, ...]
) -> tuple[RegistryRow, ...]:
    inventory_root = (
        repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    )
    inventory = InventoryIndex.from_csv(
        inventory_root / "components.csv",
        inventory_root / "contracts.csv",
    )
    manifests = load_manifest(source_root / "manifest" / "cases.jsonl", inventory)
    expected_by_id = {row[0]: row for row in expected}
    external_manifests = tuple(
        case
        for case in manifests
        if case.executor == EXTERNAL_EXECUTOR and case.case_id in expected_by_id
    )
    unsupported = sorted(
        {case.executor for case in manifests}
        - {
            "nsc_component_tests",
            "nsc_component_tests_destructive",
            "nsc_component_tests_source_extended",
            EXTERNAL_EXECUTOR,
        }
    )
    if unsupported:
        raise RuntimeError(
            "canonical manifest contains unsupported executors: "
            + ", ".join(unsupported)
        )

    external_components = {case.component for case in external_manifests}
    external_ids = {case.case_id for case in external_manifests}
    classification_root = source_root / "selfhost" / "modules"
    for path in sorted(classification_root.glob("*.jsonl")):
        for row in read_jsonl(path):
            case_id = row.get("case_id")
            if not isinstance(case_id, str):
                continue
            canonical = expected_by_id.get(case_id)
            if canonical is None or canonical[2] not in external_components:
                continue
            contract_id = row.get("contract_id")
            if contract_id is not None and contract_id != canonical[1]:
                raise RuntimeError(
                    f"external classification changes the contract of {case_id}"
                )
            external_ids.add(case_id)

    rows = tuple(sorted(expected_by_id[case_id] for case_id in external_ids))
    if len(rows) != len(external_ids):
        raise RuntimeError("external inventory contains a non-canonical case ID")
    return rows


def validate_inventory_partition(
    expected: tuple[RegistryRow, ...],
    future: tuple[RegistryRow, ...],
    ordinary: tuple[RegistryRow, ...],
    destructive: tuple[RegistryRow, ...],
    source_extended: tuple[RegistryRow, ...],
    pending: tuple[RegistryRow, ...],
    external: tuple[RegistryRow, ...],
) -> None:
    expected_by_id = {row[0]: row[1:] for row in expected}
    future_by_id = {row[0]: row[1:] for row in future}
    if len(future_by_id) != len(future):
        raise RuntimeError("future infrastructure inventory contains duplicate case IDs")
    foreign_future = sorted(set(future_by_id) - set(expected_by_id))
    changed_future = sorted(
        case_id
        for case_id in set(future_by_id) & set(expected_by_id)
        if future_by_id[case_id] != expected_by_id[case_id]
    )
    if foreign_future or changed_future:
        raise RuntimeError(
            "future infrastructure inventory differs from canonical design; "
            f"foreign={foreign_future}; changed={changed_future}"
        )

    surfaces = {
        "ordinary": ordinary,
        "destructive": destructive,
        "source-extended": source_extended,
        "pending": pending,
        "external": external,
    }
    owner_by_id = {case_id: "future" for case_id in future_by_id}
    combined: list[RegistryRow] = []
    overlaps: list[str] = []
    for owner, rows in surfaces.items():
        for row in rows:
            previous = owner_by_id.get(row[0])
            if previous is not None:
                overlaps.append(f"{row[0]}:{previous}/{owner}")
            else:
                owner_by_id[row[0]] = owner
                combined.append(row)

    runtime_expected_by_id = {
        case_id: metadata
        for case_id, metadata in expected_by_id.items()
        if case_id not in future_by_id
    }
    actual_by_id = {row[0]: row[1:] for row in combined}
    missing = sorted(set(runtime_expected_by_id) - set(actual_by_id))
    unexpected = sorted(set(actual_by_id) - set(runtime_expected_by_id))
    changed = sorted(
        case_id
        for case_id in set(runtime_expected_by_id) & set(actual_by_id)
        if runtime_expected_by_id[case_id] != actual_by_id[case_id]
    )
    if overlaps or missing or unexpected or changed:
        raise RuntimeError(
            "compiler inventory partition differs from canonical inventory; "
            f"overlaps={sorted(overlaps)}; missing={missing}; "
            f"unexpected={unexpected}; changed={changed}"
        )


def listed_registry_rows(
    host_executable: Path, selector: str
) -> tuple[RegistryRow, ...]:
    completed = subprocess.run(
        [str(host_executable), selector],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"host inventory listing {selector} returned {completed.returncode}: "
            f"{completed.stderr.strip()}"
        )
    return parse_registry_output(completed.stdout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--destructive-host-executable", type=Path, required=True)
    parser.add_argument("--source-extended-host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()

    expected = expected_registry_rows(options.repository_root, options.source_root)
    future = future_inventory_rows(options.source_root, expected)
    ordinary = listed_registry_rows(options.host_executable, "--list")
    destructive = listed_registry_rows(options.destructive_host_executable, "--list")
    source_extended = listed_registry_rows(options.source_extended_host_executable, "--list")
    pending = listed_registry_rows(options.host_executable, "--list-pending")
    external = external_inventory_rows(
        options.repository_root, options.source_root, expected
    )
    validate_inventory_partition(
        expected, future, ordinary, destructive, source_extended, pending, external
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
