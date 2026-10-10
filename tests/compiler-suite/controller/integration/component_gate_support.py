from __future__ import annotations

from collections import Counter
import csv
import json
from pathlib import Path

from nsc_suite import (
    CaseManifest,
    InventoryIndex,
    Selection,
    load_manifest,
    select_cases,
)


def read_csv(path: Path, required_fields: tuple[str, ...]) -> list[dict[str, str]]:
    """Read an extensible integration CSV without ambiguous row structure."""
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        headers = reader.fieldnames
        if headers is None:
            raise RuntimeError(f"{path}: missing CSV header")
        if any(header is None or not header.strip() for header in headers):
            raise RuntimeError(f"{path}: CSV header contains an empty name")
        if len(headers) != len(set(headers)):
            raise RuntimeError(f"{path}: CSV header contains duplicate names")
        missing = tuple(field for field in required_fields if field not in headers)
        if missing:
            raise RuntimeError(
                f"{path}: CSV header is missing required fields: {', '.join(missing)}"
            )
        rows: list[dict[str, str]] = []
        for row_number, raw_row in enumerate(reader, start=2):
            if None in raw_row:
                raise RuntimeError(f"{path}: row {row_number} contains extra cells")
            if any(value is None for value in raw_row.values()):
                raise RuntimeError(f"{path}: row {row_number} is truncated")
            row = {
                key: value
                for key, value in raw_row.items()
                if key is not None and value is not None
            }
            blank = tuple(field for field in required_fields if not row[field].strip())
            if blank:
                raise RuntimeError(
                    f"{path}: row {row_number} has blank required fields: "
                    + ", ".join(blank)
                )
            rows.append(row)
        return rows


def read_jsonl(path: Path) -> list[dict[str, object]]:
    """Read infrastructure JSONL without accepting duplicate object keys."""
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
            raise RuntimeError(
                f"invalid JSON at {path}:{line_number}: {error.msg}"
            ) from error
        except ValueError as error:
            raise RuntimeError(
                f"invalid JSON at {path}:{line_number}: {error}"
            ) from error
        if not isinstance(payload, dict):
            raise RuntimeError(f"expected JSON object at {path}:{line_number}")
        rows.append(payload)
    return rows


def read_json_object(path: Path) -> dict[str, object]:
    """Read one infrastructure JSON document with unique object keys."""
    try:
        payload = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_unique_json_object,
        )
    except json.JSONDecodeError as error:
        raise RuntimeError(f"invalid JSON at {path}: {error.msg}") from error
    except ValueError as error:
        raise RuntimeError(f"invalid JSON at {path}: {error}") from error
    if not isinstance(payload, dict):
        raise RuntimeError(f"expected JSON object at {path}")
    return payload


def _unique_json_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    """Build one infrastructure object only from unique key definitions."""
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


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


def unique_string_index(
    rows: list[dict[str, object]] | list[dict[str, str]],
    key: str,
    source: str,
) -> dict[str, dict[str, object] | dict[str, str]]:
    """Index rows by one exact non-empty string identity without coercion."""
    result: dict[str, dict[str, object] | dict[str, str]] = {}
    for row in rows:
        value = row.get(key)
        if not isinstance(value, str) or not value:
            raise RuntimeError(f"{source} contains a row without a string {key}")
        if value in result:
            raise RuntimeError(f"{source} contains duplicate {key} {value}")
        result[value] = row
    return result


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
