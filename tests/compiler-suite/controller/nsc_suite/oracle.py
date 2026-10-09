"""Exact structured-oracle comparison without recursive traversal."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any


JSON_SCALARS = (str, int, float, bool, type(None))


@dataclass(frozen=True)
class ComparisonFailure:
    path: str
    reason: str
    expected: Any
    actual: Any


def compare_structured(expected: Any, actual: Any) -> tuple[ComparisonFailure, ...]:
    """Compare JSON-compatible values exactly and report every stable mismatch path."""

    failures: list[ComparisonFailure] = []
    pending: list[tuple[str, Any, Any]] = [("$", expected, actual)]
    while pending:
        path, expected_value, actual_value = pending.pop()
        expected_type = type(expected_value)
        actual_type = type(actual_value)
        if expected_type is not actual_type:
            failures.append(
                ComparisonFailure(path, "type-mismatch", expected_type.__name__, actual_type.__name__)
            )
            continue

        if isinstance(expected_value, dict):
            expected_keys = set(expected_value)
            actual_keys = set(actual_value)
            for key in sorted(expected_keys - actual_keys):
                failures.append(
                    ComparisonFailure(_key_path(path, key), "missing-key", expected_value[key], None)
                )
            for key in sorted(actual_keys - expected_keys):
                failures.append(
                    ComparisonFailure(_key_path(path, key), "unexpected-key", None, actual_value[key])
                )
            for key in sorted(expected_keys & actual_keys, reverse=True):
                pending.append(
                    (_key_path(path, key), expected_value[key], actual_value[key])
                )
            continue

        if isinstance(expected_value, list):
            if len(expected_value) != len(actual_value):
                failures.append(
                    ComparisonFailure(
                        path,
                        "length-mismatch",
                        len(expected_value),
                        len(actual_value),
                    )
                )
            shared = min(len(expected_value), len(actual_value))
            for index in range(shared - 1, -1, -1):
                pending.append(
                    (f"{path}[{index}]", expected_value[index], actual_value[index])
                )
            continue

        if not isinstance(expected_value, JSON_SCALARS):
            failures.append(
                ComparisonFailure(
                    path,
                    "unsupported-type",
                    expected_type.__name__,
                    actual_type.__name__,
                )
            )
            continue
        if expected_value != actual_value:
            failures.append(
                ComparisonFailure(path, "value-mismatch", expected_value, actual_value)
            )

    return tuple(sorted(failures, key=lambda failure: (failure.path, failure.reason)))


def _key_path(parent: str, key: Any) -> str:
    if not isinstance(key, str):
        return f"{parent}[{key!r}]"
    if key and key.replace("_", "a").isalnum() and not key[0].isdigit():
        return f"{parent}.{key}"
    return f"{parent}[{key!r}]"
