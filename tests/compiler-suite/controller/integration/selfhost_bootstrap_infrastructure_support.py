from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from selfhost_inventory import load_component_inventory


@dataclass(frozen=True)
class BootstrapInfrastructureContract:
    contract_id: str
    file_id: str
    source_path: Path
    category: str
    language: str
    purpose: str
    oracle: str
    case_ids: tuple[str, ...]


def load_contracts(repository_root: Path, catalog: Path) -> tuple[BootstrapInfrastructureContract, ...]:
    """Load the live Wave 8 denominator without importing or executing product verifiers."""
    inventory = load_component_inventory(
        repository_root,
        "SelfHost.Bootstrap.Infrastructure",
    )
    contracts: list[BootstrapInfrastructureContract] = []
    contract_ids: set[str] = set()
    file_ids: set[str] = set()
    all_ids: set[str] = set()
    for number, raw in enumerate(catalog.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        value = json.loads(raw)
        case_ids = tuple(str(item) for item in value["case_ids"])
        contract_id = str(value["contract_id"])
        file_id = str(value["file_id"])
        if not case_ids or len(case_ids) != len(set(case_ids)):
            raise RuntimeError(f"{catalog}:{number}: contract case IDs are empty or repeated")
        if contract_id in contract_ids or file_id in file_ids:
            raise RuntimeError(f"{catalog}:{number}: duplicate contract or file identity")
        for case_id in case_ids:
            design_row = inventory.design_by_id.get(case_id)
            if design_row is None or design_row["contract_id"] != contract_id:
                raise RuntimeError(f"{catalog}:{number}: case {case_id} has invalid lineage")
            if case_id in all_ids:
                raise RuntimeError(f"{catalog}:{number}: case {case_id} is repeated")
            all_ids.add(case_id)
        contract_ids.add(contract_id)
        file_ids.add(file_id)
        relative_source = Path(str(value["source_path"])).as_posix()
        source_path = repository_root / relative_source
        contracts.append(
            BootstrapInfrastructureContract(
                contract_id=contract_id,
                file_id=file_id,
                source_path=source_path,
                category=str(value["category"]),
                language=str(value["language"]),
                purpose=str(value["purpose"]),
                oracle=str(value["oracle"]),
                case_ids=case_ids,
            )
        )

    if contract_ids != set(inventory.contracts_by_id):
        raise RuntimeError("catalog contracts differ from the canonical inventory")
    if all_ids != set(inventory.case_ids):
        raise RuntimeError("catalog cases differ from the canonical inventory")
    if any(not contract.source_path.is_file() for contract in contracts):
        raise RuntimeError("one or more catalogued bootstrap producers are absent")
    return tuple(contracts)


def case_index(contracts: tuple[BootstrapInfrastructureContract, ...]) -> dict[str, BootstrapInfrastructureContract]:
    """Build the non-recursive one-case-to-one-contract lookup used by future adapters."""
    return {
        case_id: contract
        for contract in contracts
        for case_id in contract.case_ids
    }
