"""Strict, inventory-backed JSONL manifest loading for the new compiler suite."""

from __future__ import annotations

import csv
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


CASE_ID = re.compile(r"^[A-Z][A-Z0-9-]*-[0-9]{3,}$")
CONTRACT_ID = re.compile(r"^[A-Z][A-Z0-9-]*-[0-9]{3,}$")
REQUIRED_FIELDS = frozenset(
    {
        "case_id",
        "contract_id",
        "component",
        "class",
        "executor",
        "layer",
        "inputs",
        "oracle",
        "dependencies",
        "profile",
        "build_modes",
        "optimizations",
        "isolation",
        "timeout_class",
        "tags",
    }
)
CASE_CLASSES = frozenset(
    {"nominal", "valid-variant", "invalid", "boundary", "recovery", "determinism"}
)
EXECUTORS = frozenset(
    {"nsc_component_tests", "nsc_suite_controller+nsc", "historical-weekly"}
)
LAYERS = frozenset(
    {
        "unit",
        "component",
        "integration",
        "process",
        "self-host",
        "build-contract",
        "build",
        "packaging",
        "historical",
    }
)
PROFILES = frozenset({"PR", "NIGHTLY", "WEEKLY"})
BUILD_MODES = frozenset({"Debug", "Release"})
OPTIMIZATIONS = frozenset({"NA", "O0", "O1", "O2", "O3"})
ISOLATIONS = frozenset({"in-process-reset", "temporary-directory"})
TIMEOUT_CLASSES = frozenset({"tiny", "normal", "extended", "self-host", "packaging"})


class ManifestError(ValueError):
    """Raised when the suite manifest violates a stable infrastructure contract."""


@dataclass(frozen=True)
class InventoryIndex:
    components: frozenset[str]
    contract_components: dict[str, str]

    @staticmethod
    def from_csv(components_path: Path, contracts_path: Path) -> "InventoryIndex":
        components = _read_column(components_path, "component")
        contract_components: dict[str, str] = {}
        with contracts_path.open(encoding="utf-8-sig", newline="") as stream:
            for row_number, row in enumerate(csv.DictReader(stream), start=2):
                contract_id = row.get("contract_id", "").strip()
                component = row.get("component", "").strip()
                if not contract_id or not component:
                    raise ManifestError(
                        f"invalid contract inventory row {contracts_path}:{row_number}"
                    )
                if contract_id in contract_components:
                    raise ManifestError(f"duplicate contract inventory id: {contract_id}")
                contract_components[contract_id] = component
        unknown_owners = sorted(set(contract_components.values()) - components)
        if unknown_owners:
            raise ManifestError(
                "contract inventory references unknown components: " + ", ".join(unknown_owners)
            )
        return InventoryIndex(frozenset(components), contract_components)


@dataclass(frozen=True)
class CaseManifest:
    case_id: str
    contract_id: str
    component: str
    case_class: str
    executor: str
    layer: str
    inputs: dict[str, Any]
    oracle: dict[str, Any]
    dependencies: tuple[str, ...]
    profile: str
    build_modes: tuple[str, ...]
    optimizations: tuple[str, ...]
    isolation: str
    timeout_class: str
    tags: tuple[str, ...]


def load_manifest(
    path: Path,
    inventory: InventoryIndex,
    *,
    require_cases: bool = True,
) -> tuple[CaseManifest, ...]:
    """Load, validate, order, and dependency-check every JSONL case iteratively."""

    cases: list[CaseManifest] = []
    seen: set[str] = set()
    with path.open(encoding="utf-8-sig") as stream:
        for line_number, raw_line in enumerate(stream, start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                payload = json.loads(line)
            except json.JSONDecodeError as error:
                raise ManifestError(f"invalid JSON at {path}:{line_number}: {error.msg}") from error
            case = _parse_case(payload, inventory, path, line_number)
            if case.case_id in seen:
                raise ManifestError(f"duplicate case id: {case.case_id}")
            seen.add(case.case_id)
            cases.append(case)

    if require_cases and not cases:
        raise ManifestError(f"manifest contains no cases: {path}")
    _validate_dependency_graph(cases)
    return tuple(sorted(cases, key=lambda case: case.case_id))


def _parse_case(
    payload: Any,
    inventory: InventoryIndex,
    path: Path,
    line_number: int,
) -> CaseManifest:
    location = f"{path}:{line_number}"
    if not isinstance(payload, dict):
        raise ManifestError(f"case must be a JSON object at {location}")
    fields = set(payload)
    missing = sorted(REQUIRED_FIELDS - fields)
    unknown = sorted(fields - REQUIRED_FIELDS)
    if missing:
        raise ManifestError(f"missing fields at {location}: {', '.join(missing)}")
    if unknown:
        raise ManifestError(f"unknown fields at {location}: {', '.join(unknown)}")

    case_id = _string(payload, "case_id", location)
    contract_id = _string(payload, "contract_id", location)
    component = _string(payload, "component", location)
    if CASE_ID.fullmatch(case_id) is None:
        raise ManifestError(f"invalid case_id at {location}: {case_id}")
    if CONTRACT_ID.fullmatch(contract_id) is None:
        raise ManifestError(f"invalid contract_id at {location}: {contract_id}")
    if component not in inventory.components:
        raise ManifestError(f"unknown component at {location}: {component}")
    owner = inventory.contract_components.get(contract_id)
    if owner is None:
        raise ManifestError(f"unknown contract at {location}: {contract_id}")
    if owner != component:
        raise ManifestError(
            f"contract owner mismatch at {location}: {contract_id} belongs to {owner}"
        )

    case_class = _enum(payload, "class", CASE_CLASSES, location)
    executor = _enum(payload, "executor", EXECUTORS, location)
    layer = _enum(payload, "layer", LAYERS, location)
    profile = _enum(payload, "profile", PROFILES, location)
    isolation = _enum(payload, "isolation", ISOLATIONS, location)
    timeout_class = _enum(payload, "timeout_class", TIMEOUT_CLASSES, location)
    inputs = _object(payload, "inputs", location, allow_empty=True)
    oracle = _object(payload, "oracle", location, allow_empty=False)
    dependencies = _string_array(payload, "dependencies", location, allow_empty=True)
    build_modes = _enum_array(payload, "build_modes", BUILD_MODES, location)
    optimizations = _enum_array(payload, "optimizations", OPTIMIZATIONS, location)
    tags = _string_array(payload, "tags", location, allow_empty=True)

    if executor == "nsc_component_tests":
        if isolation != "in-process-reset":
            raise ManifestError(f"host case requires in-process-reset at {location}")
        if optimizations != ("NA",):
            raise ManifestError(f"host case requires optimizations=['NA'] at {location}")
        if layer not in {"unit", "component"}:
            raise ManifestError(f"host case has invalid layer at {location}: {layer}")
    elif executor == "nsc_suite_controller+nsc":
        if isolation != "temporary-directory":
            raise ManifestError(f"compiler-driven case requires temporary-directory at {location}")
    elif profile != "WEEKLY" or layer != "historical":
        raise ManifestError(f"historical case requires WEEKLY/historical at {location}")

    return CaseManifest(
        case_id=case_id,
        contract_id=contract_id,
        component=component,
        case_class=case_class,
        executor=executor,
        layer=layer,
        inputs=inputs,
        oracle=oracle,
        dependencies=dependencies,
        profile=profile,
        build_modes=build_modes,
        optimizations=optimizations,
        isolation=isolation,
        timeout_class=timeout_class,
        tags=tags,
    )


def _validate_dependency_graph(cases: Iterable[CaseManifest]) -> None:
    ordered = tuple(cases)
    case_ids = {case.case_id for case in ordered}
    dependents: dict[str, list[str]] = {case_id: [] for case_id in case_ids}
    remaining: dict[str, int] = {}
    for case in ordered:
        unknown = sorted(set(case.dependencies) - case_ids)
        if unknown:
            raise ManifestError(
                f"case {case.case_id} has unknown dependencies: {', '.join(unknown)}"
            )
        if case.case_id in case.dependencies:
            raise ManifestError(f"case {case.case_id} depends on itself")
        remaining[case.case_id] = len(case.dependencies)
        for dependency in case.dependencies:
            dependents[dependency].append(case.case_id)

    ready = sorted(case_id for case_id, count in remaining.items() if count == 0)
    processed = 0
    while ready:
        current = ready.pop(0)
        processed += 1
        for dependent in sorted(dependents[current]):
            remaining[dependent] -= 1
            if remaining[dependent] == 0:
                ready.append(dependent)
                ready.sort()
    if processed != len(ordered):
        cyclic = sorted(case_id for case_id, count in remaining.items() if count > 0)
        raise ManifestError("dependency cycle among cases: " + ", ".join(cyclic))


def _read_column(path: Path, field: str) -> set[str]:
    values: set[str] = set()
    with path.open(encoding="utf-8-sig", newline="") as stream:
        for row_number, row in enumerate(csv.DictReader(stream), start=2):
            value = row.get(field, "").strip()
            if not value:
                raise ManifestError(f"missing {field} at {path}:{row_number}")
            if value in values:
                raise ManifestError(f"duplicate {field} in {path}: {value}")
            values.add(value)
    return values


def _string(payload: dict[str, Any], field: str, location: str) -> str:
    value = payload[field]
    if not isinstance(value, str) or not value:
        raise ManifestError(f"{field} must be a non-empty string at {location}")
    return value


def _enum(payload: dict[str, Any], field: str, allowed: frozenset[str], location: str) -> str:
    value = _string(payload, field, location)
    if value not in allowed:
        raise ManifestError(f"invalid {field} at {location}: {value}")
    return value


def _object(
    payload: dict[str, Any], field: str, location: str, *, allow_empty: bool
) -> dict[str, Any]:
    value = payload[field]
    if not isinstance(value, dict) or (not allow_empty and not value):
        qualifier = "an object" if allow_empty else "a non-empty object"
        raise ManifestError(f"{field} must be {qualifier} at {location}")
    return value


def _string_array(
    payload: dict[str, Any], field: str, location: str, *, allow_empty: bool
) -> tuple[str, ...]:
    value = payload[field]
    if not isinstance(value, list) or (not allow_empty and not value):
        raise ManifestError(f"{field} must be an array at {location}")
    if any(not isinstance(item, str) or not item for item in value):
        raise ManifestError(f"{field} must contain non-empty strings at {location}")
    if len(value) != len(set(value)):
        raise ManifestError(f"{field} contains duplicates at {location}")
    return tuple(value)


def _enum_array(
    payload: dict[str, Any], field: str, allowed: frozenset[str], location: str
) -> tuple[str, ...]:
    values = _string_array(payload, field, location, allow_empty=False)
    invalid = sorted(set(values) - allowed)
    if invalid:
        raise ManifestError(f"invalid {field} at {location}: {', '.join(invalid)}")
    return values
