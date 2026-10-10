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
OPTIONAL_FIELDS = frozenset({"expected_outcome"})
CASE_CLASSES = frozenset(
    {
        "adversarial",
        "boundary",
        "determinism",
        "invalid",
        "invalid-precondition",
        "invalid-structure",
        "nominal",
        "recovery",
        "scale",
        "transaction-state",
        "valid-minimal",
        "valid-representative",
        "valid-variant",
        "variant-matrix",
    }
)
EXECUTORS = frozenset(
    {"nsc_component_tests", "nsc_suite_controller+nsc", "historical-weekly"}
)
LAYERS = frozenset(
    {
        "unit",
        "component",
        "robustness",
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
HOST_ISOLATIONS = frozenset(
    {"in-process-reset", "temporary-directory", "filesystem-temp"}
)
ISOLATIONS = HOST_ISOLATIONS
TIMEOUT_SECONDS = {
    "tiny": 5.0,
    "short": 15.0,
    "normal": 30.0,
    "medium": 60.0,
    "extended": 180.0,
    "self-host": 900.0,
    "packaging": 900.0,
}
TIMEOUT_CLASSES = frozenset(TIMEOUT_SECONDS)
EXPECTED_OUTCOMES = frozenset({"PASS", "KNOWN_FAILURE"})


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
        for row in _read_csv_rows(contracts_path, ("contract_id", "component")):
            contract_id = row["contract_id"].strip()
            component = row["component"].strip()
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
    expected_outcome: str = "PASS"


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
                payload = json.loads(line, object_pairs_hook=_unique_json_object)
            except json.JSONDecodeError as error:
                raise ManifestError(f"invalid JSON at {path}:{line_number}: {error.msg}") from error
            except ManifestError as error:
                raise ManifestError(f"{error} at {path}:{line_number}") from error
            case = _parse_case(payload, inventory, path, line_number)
            if case.case_id in seen:
                raise ManifestError(f"duplicate case id: {case.case_id}")
            seen.add(case.case_id)
            cases.append(case)

    if require_cases and not cases:
        raise ManifestError(f"manifest contains no cases: {path}")
    _validate_dependency_graph(cases)
    return tuple(sorted(cases, key=lambda case: case.case_id))


def _unique_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    """Build one JSON object only when every key has a single definition."""
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ManifestError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


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
    unknown = sorted(fields - REQUIRED_FIELDS - OPTIONAL_FIELDS)
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
    expected_outcome = payload.get("expected_outcome", "PASS")
    if not isinstance(expected_outcome, str) or expected_outcome not in EXPECTED_OUTCOMES:
        raise ManifestError(f"invalid expected_outcome at {location}: {expected_outcome}")
    abnormal = oracle.get("abnormal_termination")
    if abnormal is not None:
        if executor != "nsc_component_tests":
            raise ManifestError(
                f"abnormal termination oracle requires a host case at {location}"
            )
        if expected_outcome != "KNOWN_FAILURE":
            raise ManifestError(
                f"abnormal termination oracle requires KNOWN_FAILURE at {location}"
            )
        if not {"known-defect", "unsafe-boundary"} <= set(tags):
            raise ManifestError(
                f"abnormal termination oracle requires known-defect and unsafe-boundary tags at {location}"
            )
        if not isinstance(abnormal, dict) or set(abnormal) != {
            "exit_code",
            "output_contains",
        }:
            raise ManifestError(
                f"invalid abnormal termination oracle at {location}"
            )
        if abnormal["exit_code"] != "nonzero":
            raise ManifestError(
                f"abnormal termination exit_code must be nonzero at {location}"
            )
        output_contains = abnormal["output_contains"]
        if (
            not isinstance(output_contains, list)
            or not output_contains
            or not all(isinstance(value, str) and value for value in output_contains)
            or len(output_contains) != len(set(output_contains))
        ):
            raise ManifestError(
                f"invalid abnormal termination output_contains at {location}"
            )

    if executor == "nsc_component_tests":
        if isolation not in HOST_ISOLATIONS:
            raise ManifestError(f"host case has invalid isolation at {location}")
        if optimizations != ("NA",):
            raise ManifestError(f"host case requires optimizations=['NA'] at {location}")
        if layer not in {"unit", "component", "robustness", "integration"}:
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
        expected_outcome=expected_outcome,
    )


def _validate_dependency_graph(cases: Iterable[CaseManifest]) -> None:
    ordered = tuple(cases)
    case_ids = {case.case_id for case in ordered}
    dependents: dict[str, list[str]] = {case_id: [] for case_id in case_ids}
    remaining: dict[str, int] = {}
    for case in ordered:
        case_dependencies = tuple(
            dependency
            for dependency in case.dependencies
            if CASE_ID.fullmatch(dependency) is not None
        )
        unknown = sorted(set(case_dependencies) - case_ids)
        if unknown:
            raise ManifestError(
                f"case {case.case_id} has unknown dependencies: {', '.join(unknown)}"
            )
        if case.case_id in case_dependencies:
            raise ManifestError(f"case {case.case_id} depends on itself")
        remaining[case.case_id] = len(case_dependencies)
        for dependency in case_dependencies:
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
    for row in _read_csv_rows(path, (field,)):
        value = row[field].strip()
        if value in values:
            raise ManifestError(f"duplicate {field} in {path}: {value}")
        values.add(value)
    return values


def _read_csv_rows(path: Path, required_fields: tuple[str, ...]) -> list[dict[str, str]]:
    """Read an extensible CSV while rejecting ambiguous or malformed structure."""
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        headers = reader.fieldnames
        if headers is None:
            raise ManifestError(f"missing CSV header: {path}")
        if any(header is None or not header.strip() for header in headers):
            raise ManifestError(f"CSV header contains an empty name: {path}")
        if len(headers) != len(set(headers)):
            raise ManifestError(f"CSV header contains duplicate names: {path}")
        missing = tuple(field for field in required_fields if field not in headers)
        if missing:
            raise ManifestError(
                f"CSV header is missing required fields at {path}: {', '.join(missing)}"
            )
        rows: list[dict[str, str]] = []
        for row_number, raw_row in enumerate(reader, start=2):
            if None in raw_row:
                raise ManifestError(f"CSV row contains extra cells at {path}:{row_number}")
            if any(value is None for value in raw_row.values()):
                raise ManifestError(f"CSV row is truncated at {path}:{row_number}")
            row = {key: value for key, value in raw_row.items() if key is not None and value is not None}
            blank = tuple(field for field in required_fields if not row[field].strip())
            if blank:
                raise ManifestError(
                    f"CSV row has blank required fields at {path}:{row_number}: "
                    + ", ".join(blank)
                )
            rows.append(row)
        return rows


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
