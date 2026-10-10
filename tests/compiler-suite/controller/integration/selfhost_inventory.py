from __future__ import annotations

import csv
import json
from dataclasses import dataclass
from pathlib import Path


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def read_jsonl(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            payload = json.loads(line)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"invalid JSON at {path}:{line_number}: {error.msg}") from error
        if not isinstance(payload, dict):
            raise RuntimeError(f"expected JSON object at {path}:{line_number}")
        rows.append(payload)
    return rows


def unique_index(
    rows: list[dict[str, str]], key: str, source: str
) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    for row in rows:
        value = row.get(key, "")
        if not value:
            raise RuntimeError(f"{source} contains a row without {key}")
        if value in result:
            raise RuntimeError(f"{source} contains duplicate {key} {value}")
        result[value] = row
    return result


@dataclass(frozen=True)
class ComponentInventory:
    component: str
    design: tuple[dict[str, str], ...]
    allocation: tuple[dict[str, str], ...]
    contracts: tuple[dict[str, str], ...]
    design_by_id: dict[str, dict[str, str]]
    allocation_by_id: dict[str, dict[str, str]]
    contracts_by_id: dict[str, dict[str, str]]

    @property
    def case_ids(self) -> frozenset[str]:
        return frozenset(self.design_by_id)

    def profile_mismatch_ids(self) -> frozenset[str]:
        return frozenset(
            case_id
            for case_id, design_row in self.design_by_id.items()
            if design_row.get("frequency", "")
            != self.allocation_by_id[case_id].get("default_profile", "")
        )


def load_component_inventory(repository_root: Path, component: str) -> ComponentInventory:
    inventory_root = (
        repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    )
    design = tuple(
        row
        for row in read_csv(inventory_root / "test-case-design.csv")
        if row.get("component") == component
    )
    allocation = tuple(
        row
        for row in read_csv(inventory_root / "suite-case-allocation.csv")
        if row.get("component") == component
    )
    contracts = tuple(
        row
        for row in read_csv(inventory_root / "contracts.csv")
        if row.get("component") == component
    )
    if not design or not allocation or not contracts:
        raise RuntimeError(f"{component} canonical inventory is empty")
    design_by_id = unique_index(list(design), "case_id", f"{component} design")
    allocation_by_id = unique_index(
        list(allocation), "case_id", f"{component} allocation"
    )
    contracts_by_id = unique_index(
        list(contracts), "contract_id", f"{component} contracts"
    )
    if design_by_id.keys() != allocation_by_id.keys():
        raise RuntimeError(f"{component} design and allocation case IDs differ")
    referenced_contracts: set[str] = set()
    for case_id, design_row in design_by_id.items():
        allocation_row = allocation_by_id[case_id]
        contract_id = design_row.get("contract_id", "")
        if contract_id not in contracts_by_id:
            raise RuntimeError(f"{component} case {case_id} has unknown contract {contract_id}")
        referenced_contracts.add(contract_id)
        if allocation_row.get("contract_id", "") != contract_id:
            raise RuntimeError(f"{component} allocation changes the contract of {case_id}")
        if allocation_row.get("layer", "") != design_row.get("layer", ""):
            raise RuntimeError(f"{component} allocation changes the layer of {case_id}")
    if referenced_contracts != set(contracts_by_id):
        raise RuntimeError(f"{component} contract inventory contains an unreferenced contract")
    return ComponentInventory(
        component=component,
        design=design,
        allocation=allocation,
        contracts=contracts,
        design_by_id=design_by_id,
        allocation_by_id=allocation_by_id,
        contracts_by_id=contracts_by_id,
    )


def case_ordinal(case_id: str) -> int:
    _prefix, separator, ordinal = case_id.rpartition("-")
    if not separator or not ordinal.isdigit():
        raise RuntimeError(f"case ID has no numeric ordinal: {case_id}")
    return int(ordinal)


def expected_ranges(
    inventory: ComponentInventory, *, include_layer: bool
) -> tuple[tuple[object, ...], ...]:
    ranges: list[tuple[object, ...]] = []
    current: list[object] | None = None
    previous_ordinal: int | None = None
    for row in inventory.design:
        ordinal = case_ordinal(row["case_id"])
        contract_id = row["contract_id"]
        layer = row["layer"]
        identity = (contract_id, layer) if include_layer else (contract_id,)
        if current is None or previous_ordinal is None:
            current = [ordinal, ordinal, *identity]
            ranges.append(tuple(current))
        elif ordinal != previous_ordinal + 1 or tuple(current[2:]) != identity:
            current = [ordinal, ordinal, *identity]
            ranges.append(tuple(current))
        else:
            current[1] = ordinal
            ranges[-1] = tuple(current)
        previous_ordinal = ordinal
    return tuple(ranges)


def validate_unique_materialized_ids(
    component: str,
    rows: list[tuple[object, ...]],
) -> dict[str, tuple[object, ...]]:
    result: dict[str, tuple[object, ...]] = {}
    for row in rows:
        case_id = str(row[0])
        if case_id in result:
            raise RuntimeError(f"{component} modules repeat case {case_id}")
        result[case_id] = row
    return result


def validate_pending_manifest(
    repository_root: Path,
    component: str,
    fragment_name: str,
) -> None:
    fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / fragment_name
    )
    if fragment.exists():
        raise RuntimeError(f"zero-executable {component} must not retain a fragment")
    central = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "cases.jsonl"
    )
    if any(row.get("component") == component for row in read_jsonl(central)):
        raise RuntimeError(f"pending {component} cases entered the executable manifest")
