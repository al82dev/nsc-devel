from __future__ import annotations

from collections import Counter
from pathlib import Path

from nsc_suite import (
    CaseManifest,
    InventoryIndex,
    Selection,
    load_manifest,
    select_cases,
)


def required_manifest_integer(
    inputs: dict[str, object], field: str, component: str
) -> int:
    """Read an exact JSON integer without accepting Python coercions or bools."""
    if field not in inputs:
        raise RuntimeError(f"{component} manifest inputs omit {field}")
    value = inputs[field]
    if type(value) is not int:
        raise RuntimeError(
            f"{component} manifest input {field} must be an exact integer"
        )
    return value


def required_nonnegative_integer(
    payload: dict[str, object], field: str, source: str
) -> int:
    """Read an exact nonnegative integer from an infrastructure document."""
    if field not in payload:
        raise RuntimeError(f"{source} omits {field}")
    value = payload[field]
    if type(value) is not int or value < 0:
        raise RuntimeError(f"{source} field {field} must be an exact nonnegative integer")
    return value


def component_manifest_cases(
    source_root: Path,
    repository_root: Path,
    component: str,
    profile: str,
    build_mode: str,
) -> tuple[CaseManifest, ...]:
    """Reuse the production selector instead of duplicating its denominator."""
    manifest = source_root / "manifest" / "cases.jsonl"
    inventory_root = (
        repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    inventory = InventoryIndex.from_csv(
        inventory_root / "components.csv",
        inventory_root / "contracts.csv",
    )
    cases = load_manifest(manifest, inventory)
    selected = select_cases(
        cases,
        Selection(
            components=frozenset({component}),
            profile=profile,
            build_mode=build_mode,
        ),
    )
    if not selected:
        raise RuntimeError(f"central manifest contains no executable {component} cases")
    case_ids = tuple(case.case_id for case in selected)
    if any(not case_id for case_id in case_ids) or len(case_ids) != len(set(case_ids)):
        raise RuntimeError(f"central manifest contains invalid {component} case identities")
    return selected


def assert_component_results(
    payload: dict[str, object],
    cases: tuple[CaseManifest, ...],
    component: str,
    *,
    selection_name: str = "live manifest selection",
) -> None:
    """Require one terminal result per live manifest case and its declared outcome."""
    raw_results = payload.get("results")
    raw_counts = payload.get("counts")
    if not isinstance(raw_results, list) or not isinstance(raw_counts, dict):
        raise RuntimeError(f"{component} controller result schema is invalid")
    if any(not isinstance(result, dict) for result in raw_results):
        raise RuntimeError(f"{component} controller contains a non-object result")
    expected_ids = tuple(case.case_id for case in cases)
    raw_ids = tuple(result.get("case_id") for result in raw_results)
    if any(not isinstance(case_id, str) or not case_id for case_id in raw_ids):
        raise RuntimeError(f"{component} controller contains an invalid case identity")
    actual_ids = raw_ids
    if actual_ids != expected_ids:
        raise RuntimeError(
            f"{component} controller results differ from the {selection_name}"
        )

    expected_statuses = tuple(case.expected_outcome for case in cases)
    actual_statuses = tuple(result.get("status") for result in raw_results)
    if any(not isinstance(status, str) or not status for status in actual_statuses):
        raise RuntimeError(f"{component} controller contains an invalid row status")
    if actual_statuses != expected_statuses:
        raise RuntimeError(
            f"{component} controller row statuses differ from declared outcomes"
        )

    expected_counts = Counter(case.expected_outcome for case in cases)
    row_counts = Counter(actual_statuses)
    observed_counts: dict[str, int] = {}
    for status, count in raw_counts.items():
        if not isinstance(status, str) or not status:
            raise RuntimeError(f"{component} controller contains an invalid result state")
        if type(count) is not int or count < 0:
            raise RuntimeError(f"{component} controller contains an invalid result count")
        if count != 0:
            observed_counts[status] = count
    if observed_counts != dict(row_counts) or row_counts != expected_counts:
        raise RuntimeError(
            f"{component} result counts differ from declared outcomes: {raw_counts}"
        )
