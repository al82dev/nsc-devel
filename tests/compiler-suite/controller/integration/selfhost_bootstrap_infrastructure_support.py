from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


EXPECTED_CONTRACTS = 73
EXPECTED_CASES = 438
EXPECTED_FIRST_CASE = "T12B-01081"
EXPECTED_LAST_CASE = "T12B-01518"


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
    """Load the frozen Wave 8 denominator without importing or executing product verifiers."""
    contracts: list[BootstrapInfrastructureContract] = []
    for number, raw in enumerate(catalog.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        value = json.loads(raw)
        case_ids = tuple(str(item) for item in value["case_ids"])
        if len(case_ids) != 6:
            raise RuntimeError(f"{catalog}:{number}: each contract must own exactly six cases")
        relative_source = Path(str(value["source_path"])).as_posix()
        source_path = repository_root / relative_source
        contracts.append(
            BootstrapInfrastructureContract(
                contract_id=str(value["contract_id"]),
                file_id=str(value["file_id"]),
                source_path=source_path,
                category=str(value["category"]),
                language=str(value["language"]),
                purpose=str(value["purpose"]),
                oracle=str(value["oracle"]),
                case_ids=case_ids,
            )
        )

    all_ids = tuple(case_id for contract in contracts for case_id in contract.case_ids)
    if len(contracts) != EXPECTED_CONTRACTS:
        raise RuntimeError(f"catalog has {len(contracts)} contracts, expected {EXPECTED_CONTRACTS}")
    if len(all_ids) != EXPECTED_CASES or len(set(all_ids)) != EXPECTED_CASES:
        raise RuntimeError("catalog must contain 438 unique case identifiers")
    if all_ids[0] != EXPECTED_FIRST_CASE or all_ids[-1] != EXPECTED_LAST_CASE:
        raise RuntimeError("catalog case range differs from T12B-01081..T12B-01518")
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
