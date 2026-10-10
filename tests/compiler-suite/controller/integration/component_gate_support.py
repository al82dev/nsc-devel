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
) -> None:
    """Require one terminal result per live manifest case and its declared outcome."""
    raw_results = payload.get("results")
    raw_counts = payload.get("counts")
    if not isinstance(raw_results, list) or not isinstance(raw_counts, dict):
        raise RuntimeError(f"{component} controller result schema is invalid")
    if any(not isinstance(result, dict) for result in raw_results):
        raise RuntimeError(f"{component} controller contains a non-object result")
    expected_ids = tuple(case.case_id for case in cases)
    actual_ids = tuple(str(result.get("case_id", "")) for result in raw_results)
    if actual_ids != expected_ids:
        raise RuntimeError(
            f"{component} controller results differ from the live manifest selection"
        )

    expected_counts = Counter(case.expected_outcome for case in cases)
    observed_counts = {
        str(status): int(count)
        for status, count in raw_counts.items()
        if int(count) != 0
    }
    if observed_counts != dict(expected_counts):
        raise RuntimeError(
            f"{component} result counts differ from declared outcomes: {raw_counts}"
        )
