from __future__ import annotations

import argparse
import re
from pathlib import Path

from component_gate_support import required_string_array
from selfhost_inventory import (
    ComponentInventory,
    case_ordinal,
    read_jsonl,
    required_string_field,
    resolve_repository_file,
    validate_empty_component_universe,
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
            or row.get("disposition") == "Executable"
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
    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostContractsTestSupport.h"
    ).read_text(encoding="utf-8")
    materialized = tuple(materialized_ranges(repository_root))
    if not materialized or len(set(materialized)) != len(materialized):
        raise RuntimeError("Contracts historical ranges are empty or duplicated")
    if "TestDisposition::Executable" in support:
        raise RuntimeError("retired Contracts cases expose an executable disposition")

    validate_empty_component_universe(
        repository_root,
        "SelfHost.Contracts",
        "selfhost-contracts.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

