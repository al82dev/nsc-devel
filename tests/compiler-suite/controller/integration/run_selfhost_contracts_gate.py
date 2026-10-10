from __future__ import annotations

import argparse
import re
from pathlib import Path

from component_gate_support import required_string_array
from selfhost_inventory import (
    ComponentInventory,
    case_ordinal,
    load_component_inventory,
    read_jsonl,
    required_string_field,
    resolve_repository_file,
    validate_pending_manifest,
)

RANGE_ROW = re.compile(
    r'\{\s*(\d+),\s*(\d+),\s*"(C12B-SHC-\d{3})",\s*'
    r'"([^"]+)"',
    re.MULTILINE,
)


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostContractsToolchainModule.cpp",
        root / "SelfHostContractsIrRuntimeModule.cpp",
        root / "SelfHostContractsDistributionModule.cpp",
        root / "SelfHostContractsSourceModule.cpp",
    ]
    rows: list[tuple[int, int, str, str]] = []
    for path in paths:
        for first, last, contract, source_path in RANGE_ROW.findall(
            path.read_text(encoding="utf-8")
        ):
            rows.append((int(first), int(last), contract, source_path))
    return rows


def catalog_ranges(
    repository_root: Path,
    inventory: ComponentInventory,
    catalog_rows: list[dict[str, object]],
) -> set[tuple[int, int, str, str]]:
    contract_ids: set[str] = set()
    all_case_ids: set[str] = set()
    result: set[tuple[int, int, str, str]] = set()
    for row in catalog_rows:
        contract_id = required_string_field(
            row,
            "contract_id",
            "SelfHost.Contracts catalog",
        )
        source_path = required_string_field(
            row,
            "source_path",
            f"SelfHost.Contracts catalog {contract_id}",
        )
        case_ids = required_string_array(
            row,
            "case_ids",
            f"SelfHost.Contracts catalog {contract_id}",
        )
        if (
            contract_id in contract_ids
            or contract_id not in inventory.contracts_by_id
            or row.get("disposition") != "InfrastructurePending"
        ):
            raise RuntimeError("Contracts catalog contains an invalid contract row")

        if not case_ids:
            raise RuntimeError(f"catalog contract {contract_id} has invalid case IDs")
        ordinals = tuple(case_ordinal(case_id) for case_id in case_ids)
        if ordinals != tuple(range(ordinals[0], ordinals[-1] + 1)):
            raise RuntimeError(f"catalog contract {contract_id} is not contiguous")
        for case_id in case_ids:
            design_row = inventory.design_by_id.get(case_id)
            if design_row is None or design_row["contract_id"] != contract_id:
                raise RuntimeError(f"catalog case {case_id} has invalid lineage")
            if case_id in all_case_ids:
                raise RuntimeError(f"catalog repeats case {case_id}")
            all_case_ids.add(case_id)

        resolve_repository_file(
            repository_root,
            source_path,
            f"SelfHost.Contracts catalog {contract_id}",
        )
        materialized = (ordinals[0], ordinals[-1], contract_id, source_path)
        if materialized in result:
            raise RuntimeError(f"catalog repeats materialized range {materialized!r}")
        result.add(materialized)
        contract_ids.add(contract_id)

    if contract_ids != set(inventory.contracts_by_id):
        raise RuntimeError("Contracts catalog differs from the canonical contracts")
    if all_case_ids != set(inventory.case_ids):
        raise RuntimeError("Contracts catalog differs from the canonical cases")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = load_component_inventory(repository_root, "SelfHost.Contracts")
    if any(row["default_profile"] != "NIGHTLY" for row in inventory.allocation):
        raise RuntimeError("SelfHost.Contracts allocation no longer assigns all cases to NIGHTLY")
    if any(row["layer"] != "self-host" for row in inventory.allocation):
        raise RuntimeError("SelfHost.Contracts allocation changed its self-host layer")
    if any(
        row["planned_executor"] != "nsc_suite_controller+nsc"
        for row in inventory.allocation
    ):
        raise RuntimeError("SelfHost.Contracts allocation changed its canonical executor")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostContractsTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Contracts modules do not preserve InfrastructurePending disposition")

    catalog = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "contracts"
        / "SelfHostContractsCatalog.jsonl"
    )
    expected = catalog_ranges(repository_root, inventory, read_jsonl(catalog))
    materialized = tuple(materialized_ranges(repository_root))
    if len(set(materialized)) != len(materialized) or set(materialized) != expected:
        raise RuntimeError("consolidated modules changed range, contract, or source lineage")

    contradiction = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostContractsPlanContradictions.jsonl"
    )
    records = read_jsonl(contradiction)
    record_ids: set[str] = set()
    for record in records:
        record_id = required_string_field(
            record,
            "id",
            "SelfHost.Contracts contradiction",
        )
        if (
            not record_id
            or record_id in record_ids
            or record.get("component") != "SelfHost.Contracts"
            or record.get("status") != "OPEN"
        ):
            raise RuntimeError("Contracts profile contradiction is invalid")
        record_ids.add(record_id)
    if bool(inventory.profile_mismatch_ids()) != bool(records):
        raise RuntimeError("Contracts contradiction differs from live profiles")

    validate_pending_manifest(
        repository_root,
        "SelfHost.Contracts",
        "selfhost-contracts.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

