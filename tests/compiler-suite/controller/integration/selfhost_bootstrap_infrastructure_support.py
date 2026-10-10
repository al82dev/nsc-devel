from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from selfhost_inventory import load_component_inventory


CATALOG_FIELDS = frozenset(
    {
        "contract_id",
        "file_id",
        "source_path",
        "category",
        "language",
        "purpose",
        "oracle",
        "case_ids",
    }
)


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
        value = parse_catalog_row(raw, catalog, number)
        location = f"{catalog}:{number}"
        case_ids = _required_string_array(value, "case_ids", location)
        contract_id = _required_string(value, "contract_id", location)
        file_id = _required_string(value, "file_id", location)
        source_text = _required_string(value, "source_path", location)
        category = _required_string(value, "category", location)
        language = _required_string(value, "language", location)
        purpose = _required_string(value, "purpose", location)
        oracle = _required_string(value, "oracle", location)
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
        relative_source = Path(source_text).as_posix()
        source_path = repository_root / relative_source
        contracts.append(
            BootstrapInfrastructureContract(
                contract_id=contract_id,
                file_id=file_id,
                source_path=source_path,
                category=category,
                language=language,
                purpose=purpose,
                oracle=oracle,
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


def parse_catalog_row(raw: str, catalog: Path, line_number: int) -> dict[str, object]:
    """Parse one catalogue row without allowing duplicate object keys."""
    try:
        value = json.loads(raw, object_pairs_hook=_unique_json_object)
    except json.JSONDecodeError as error:
        raise RuntimeError(
            f"{catalog}:{line_number}: invalid JSON: {error.msg}"
        ) from error
    except ValueError as error:
        raise RuntimeError(
            f"{catalog}:{line_number}: invalid JSON: {error}"
        ) from error
    if not isinstance(value, dict):
        raise RuntimeError(f"{catalog}:{line_number}: expected JSON object")
    fields = set(value)
    missing = sorted(CATALOG_FIELDS - fields)
    unknown = sorted(fields - CATALOG_FIELDS)
    if missing:
        raise RuntimeError(
            f"{catalog}:{line_number}: missing fields: {', '.join(missing)}"
        )
    if unknown:
        raise RuntimeError(
            f"{catalog}:{line_number}: unknown fields: {', '.join(unknown)}"
        )
    location = f"{catalog}:{line_number}"
    for field in (
        "contract_id",
        "file_id",
        "source_path",
        "category",
        "language",
        "purpose",
        "oracle",
    ):
        _required_string(value, field, location)
    _required_string_array(value, "case_ids", location)
    return value


def _unique_json_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    """Build one catalogue object only from unique key definitions."""
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def _required_string(
    value: dict[str, object], field: str, location: str
) -> str:
    item = value[field]
    if not isinstance(item, str) or not item:
        raise RuntimeError(f"{location}: {field} must be a non-empty string")
    return item


def _required_string_array(
    value: dict[str, object], field: str, location: str
) -> tuple[str, ...]:
    items = value[field]
    if not isinstance(items, list) or not items:
        raise RuntimeError(f"{location}: {field} must be a non-empty array")
    if any(not isinstance(item, str) or not item for item in items):
        raise RuntimeError(
            f"{location}: {field} must contain non-empty strings"
        )
    if len(items) != len(set(items)):
        raise RuntimeError(f"{location}: {field} contains duplicates")
    return tuple(items)


def case_index(contracts: tuple[BootstrapInfrastructureContract, ...]) -> dict[str, BootstrapInfrastructureContract]:
    """Build the non-recursive one-case-to-one-contract lookup used by future adapters."""
    return {
        case_id: contract
        for contract in contracts
        for case_id in contract.case_ids
    }
