from __future__ import annotations

import json
import re
import shutil
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

from component_gate_support import (
    component_manifest_rows,
    read_csv,
    required_string_array,
)


def read_jsonl(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            payload = json.loads(line, object_pairs_hook=_unique_json_object)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"invalid JSON at {path}:{line_number}: {error.msg}") from error
        except ValueError as error:
            raise RuntimeError(f"invalid JSON at {path}:{line_number}: {error}") from error
        if not isinstance(payload, dict):
            raise RuntimeError(f"expected JSON object at {path}:{line_number}")
        rows.append(payload)
    return rows


def _unique_json_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    """Build one inventory object only when every key has one definition."""
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def required_oracle_exit_code(row: dict[str, object], component: str) -> int:
    """Return an exact exit-code oracle without accepting Python coercions."""
    oracle = row.get("oracle")
    if not isinstance(oracle, dict):
        raise RuntimeError(f"{component} manifest oracle is not an object")
    if "exit_code" not in oracle:
        raise RuntimeError(f"{component} manifest oracle omits exit_code")
    exit_code = oracle["exit_code"]
    if type(exit_code) is not int:
        raise RuntimeError(f"{component} oracle exit_code must be an exact integer")
    return exit_code


def required_string_field(
    row: dict[str, object], field: str, context: str
) -> str:
    """Return one exact non-empty JSON string without implicit conversion."""
    value = row.get(field)
    if not isinstance(value, str) or not value:
        raise RuntimeError(f"{context} {field} must be a non-empty string")
    return value


def resolve_repository_file(
    repository_root: Path,
    source_text: str,
    context: str,
) -> Path:
    """Resolve one relative producer path without allowing checkout escape."""
    root = repository_root.resolve()
    relative = Path(source_text)
    if relative.is_absolute() or relative.drive:
        raise RuntimeError(f"{context} source_path must be repository-relative")
    resolved = (root / relative).resolve()
    try:
        resolved.relative_to(root)
    except ValueError as error:
        raise RuntimeError(
            f"{context} source_path resolves outside the repository"
        ) from error
    if not resolved.is_file():
        raise RuntimeError(f"{context} source_path does not exist: {source_text}")
    return resolved


def materialize_contained_project(
    repository_root: Path,
    source_project: Path,
    destination_root: Path,
) -> Path:
    """Copy one fixture project into a project-local, traversal-free workspace.

    NativeSharp project globbing deliberately rejects parent path segments in
    ``Compile`` items.  Self-host fixtures still need to consume selected
    compiler sources, so the test controller materializes those exact bytes
    below the run workspace and rewrites only the generated project.  The
    canonical project and every production source remain untouched.
    """
    root = repository_root.resolve()
    project = source_project.resolve()
    try:
        project.relative_to(root)
    except ValueError as error:
        raise RuntimeError("fixture project resolves outside the repository") from error
    if not project.is_file() or project.suffix.lower() != ".nsproj":
        raise RuntimeError(f"fixture project does not exist: {source_project}")

    destination = destination_root.resolve()
    if destination.exists():
        raise RuntimeError(f"materialized fixture destination already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.mkdir()

    try:
        tree = ET.parse(project)
    except ET.ParseError as error:
        raise RuntimeError(f"invalid fixture project XML: {project}: {error}") from error

    copied_targets: set[Path] = set()
    compile_items = list(tree.getroot().iter("Compile"))
    if not compile_items:
        raise RuntimeError(f"fixture project contains no Compile items: {project}")
    for item in compile_items:
        include = item.get("Include")
        if not include or Path(include).is_absolute() or Path(include).drive:
            raise RuntimeError("fixture Compile Include must be a non-empty relative path")
        if any(character in include for character in "*?[]"):
            raise RuntimeError("fixture Compile Include must name one explicit source file")
        source = (project.parent / include).resolve()
        try:
            relative_source = source.relative_to(root)
        except ValueError as error:
            raise RuntimeError(
                f"fixture Compile Include resolves outside the repository: {include}"
            ) from error
        if not source.is_file():
            raise RuntimeError(f"fixture Compile source does not exist: {include}")
        target = destination / "sources" / relative_source
        if target in copied_targets:
            raise RuntimeError(f"fixture Compile source is repeated: {include}")
        copied_targets.add(target)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        item.set("Include", target.relative_to(destination).as_posix())

    for item in tree.getroot().iter("ProjectReference"):
        include = item.get("Include")
        if not include:
            raise RuntimeError("fixture ProjectReference omits Include")
        reference = Path(include)
        resolved = (
            reference.resolve()
            if reference.is_absolute() or reference.drive
            else (project.parent / reference).resolve()
        )
        try:
            resolved.relative_to(root)
        except ValueError as error:
            raise RuntimeError(
                f"fixture ProjectReference resolves outside the repository: {include}"
            ) from error
        if not resolved.is_file() or resolved.suffix.lower() != ".nsproj":
            raise RuntimeError(f"fixture ProjectReference does not exist: {include}")
        item.set("Include", resolved.as_posix())

    ET.indent(tree, space="  ")
    generated = destination / project.name
    tree.write(generated, encoding="utf-8", xml_declaration=False)
    return generated


def json_values_equal_exact(expected: object, actual: object) -> bool:
    """Compare JSON-compatible values by exact type and value without recursion."""
    pending: list[tuple[object, object]] = [(expected, actual)]
    while pending:
        expected_value, actual_value = pending.pop()
        if type(expected_value) is not type(actual_value):
            return False
        if isinstance(expected_value, dict):
            if expected_value.keys() != actual_value.keys():
                return False
            pending.extend(
                (expected_value[key], actual_value[key]) for key in expected_value
            )
            continue
        if isinstance(expected_value, list):
            if len(expected_value) != len(actual_value):
                return False
            pending.extend(zip(expected_value, actual_value))
            continue
        if expected_value != actual_value:
            return False
    return True


def parse_json_value(raw: str) -> object:
    """Parse one JSON value without accepting duplicate object keys."""
    return json.loads(raw, object_pairs_hook=_unique_json_object)


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
        for row in read_csv(
            inventory_root / "test-case-design.csv",
            ("case_id", "contract_id", "component", "layer", "frequency"),
        )
        if row.get("component") == component
    )
    allocation = tuple(
        row
        for row in read_csv(
            inventory_root / "suite-case-allocation.csv",
            ("case_id", "contract_id", "component", "layer", "default_profile"),
        )
        if row.get("component") == component
    )
    contracts = tuple(
        row
        for row in read_csv(
            inventory_root / "contracts.csv",
            ("contract_id", "component"),
        )
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


def expected_nightly_ordinals(inventory: ComponentInventory) -> frozenset[int]:
    return frozenset(
        case_ordinal(case_id)
        for case_id, row in inventory.allocation_by_id.items()
        if row.get("default_profile") == "NIGHTLY"
    )


def materialized_nightly_ordinals(path: Path) -> frozenset[int]:
    text = path.read_text(encoding="utf-8")
    ordinals: set[int] = set()
    arrays = re.findall(r"nightlies\s*\{([^}]*)\}", text, flags=re.DOTALL)
    for body in arrays:
        ordinals.update(int(value) for value in re.findall(r"\d+", body))
    ordinals.update(
        int(value)
        for value in re.findall(r'ordinal\s*==\s*(\d+)\s*\?\s*"NIGHTLY"', text)
    )
    return frozenset(ordinals)


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
        case_id = row[0]
        if not isinstance(case_id, str) or not case_id:
            raise RuntimeError(
                f"{component} modules contain an invalid case identity"
            )
        if case_id in result:
            raise RuntimeError(f"{component} modules repeat case {case_id}")
        result[case_id] = row
    return result


def validate_manifest_cases(
    inventory: ComponentInventory,
    rows: list[dict[str, object]],
) -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    for row in rows:
        context = f"{inventory.component} manifest"
        case_id = required_string_field(row, "case_id", context)
        if case_id in result:
            raise RuntimeError(f"{inventory.component} manifest has a duplicate case ID")
        required_string_array(
            row, "tags", f"{inventory.component} manifest case {case_id}"
        )
        design_row = inventory.design_by_id.get(case_id)
        if design_row is None:
            raise RuntimeError(f"{inventory.component} manifest contains foreign case {case_id}")
        allocation_row = inventory.allocation_by_id[case_id]
        if row.get("component") != inventory.component:
            raise RuntimeError(f"{inventory.component} manifest changes the component of {case_id}")
        contract_id = required_string_field(row, "contract_id", context)
        layer = required_string_field(row, "layer", context)
        profile = required_string_field(row, "profile", context)
        if contract_id != design_row["contract_id"]:
            raise RuntimeError(f"{inventory.component} manifest changes the contract of {case_id}")
        if layer != design_row["layer"]:
            raise RuntimeError(f"{inventory.component} manifest changes the layer of {case_id}")
        if profile != allocation_row["default_profile"]:
            raise RuntimeError(f"{inventory.component} manifest changes the profile of {case_id}")
        result[case_id] = row
    return result


def classification_sets(
    inventory: ComponentInventory,
    path: Path,
) -> dict[str, frozenset[str]]:
    result: dict[str, set[str]] = {}
    seen: set[str] = set()
    for row in read_jsonl(path):
        context = f"{inventory.component} classification"
        case_id = required_string_field(row, "case_id", context)
        disposition = required_string_field(row, "disposition", context)
        contract_id = required_string_field(row, "contract_id", context)
        if disposition == "InfrastructurePending":
            continue
        if case_id in seen:
            raise RuntimeError(
                f"{inventory.component} classifications have a duplicate identity"
            )
        design_row = inventory.design_by_id.get(case_id)
        if design_row is None:
            raise RuntimeError(
                f"{inventory.component} classifications contain foreign case {case_id}"
            )
        if contract_id != design_row["contract_id"]:
            raise RuntimeError(
                f"{inventory.component} classification changes the contract of {case_id}"
            )
        component = row.get("component")
        if component is not None and (
            not isinstance(component, str) or not component
        ):
            raise RuntimeError(
                f"{inventory.component} classification component must be a non-empty string"
            )
        if component is not None and component != inventory.component:
            raise RuntimeError(
                f"{inventory.component} classification changes the component of {case_id}"
            )
        seen.add(case_id)
        result.setdefault(disposition, set()).add(case_id)
    return {key: frozenset(value) for key, value in result.items()}


def validate_manifest_partition(
    inventory: ComponentInventory,
    manifest_by_id: dict[str, dict[str, object]],
    non_executable: frozenset[str],
) -> None:
    executable = set(manifest_by_id)
    if executable & non_executable:
        raise RuntimeError(f"{inventory.component} non-executable cases entered the manifest")
    if executable | set(non_executable) != set(inventory.case_ids):
        raise RuntimeError(f"{inventory.component} denominator contains omissions")


def validate_classified_manifest(
    inventory: ComponentInventory,
    cases: list[dict[str, object]],
    classification_path: Path,
    non_executable_dispositions: frozenset[str],
) -> tuple[dict[str, dict[str, object]], dict[str, frozenset[str]]]:
    manifest_by_id = validate_manifest_cases(inventory, cases)
    classified = classification_sets(inventory, classification_path)
    supported = set(non_executable_dispositions) | {"KnownProductDefect"}
    unsupported = set(classified) - supported
    if unsupported:
        raise RuntimeError(
            f"{inventory.component} contains unsupported classifications: "
            + ", ".join(sorted(unsupported))
        )
    non_executable = frozenset(
        case_id
        for disposition in non_executable_dispositions
        for case_id in classified.get(disposition, frozenset())
    )
    validate_manifest_partition(inventory, manifest_by_id, non_executable)
    defects = classified.get("KnownProductDefect", frozenset())
    if not defects <= manifest_by_id.keys():
        raise RuntimeError(f"{inventory.component} defects must be executable cases")
    tagged = frozenset(
        case_id
        for case_id, row in manifest_by_id.items()
        if "known-product-defect" in row.get("tags", [])
    )
    if tagged != defects:
        raise RuntimeError(f"{inventory.component} defect tags differ from classifications")
    return manifest_by_id, classified


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
    source_root = repository_root / "tests" / "compiler-suite"
    if component_manifest_rows(
        source_root, repository_root, component
    ):
        raise RuntimeError(
            f"pending {component} cases entered the executable manifest"
        )
