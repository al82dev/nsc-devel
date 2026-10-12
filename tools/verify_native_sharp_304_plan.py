#!/usr/bin/env python3
"""Verify the NativeSharp 3.0.4 plan from structural, repository-owned data."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


PLAN_ITEM = re.compile(r"^- `(?P<id>NS304-T\d{2}-F\d{2})` — ")
TRACKING_ITEM = re.compile(r"^- \[(?P<mark>[ xX])\] `(?P<id>NS304-T\d{2}-F\d{2})` — ")


@dataclass(frozen=True)
class Issue:
    code: str
    path: str
    item_id: str | None = None

    def as_dict(self) -> dict[str, str]:
        result = {"code": self.code, "path": self.path}
        if self.item_id is not None:
            result["item_id"] = self.item_id
        return result


def _read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8-sig").splitlines()


def _duplicates(values: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    repeated: set[str] = set()
    for value in values:
        if value in seen:
            repeated.add(value)
        seen.add(value)
    return sorted(repeated)


def load_plan(path: Path) -> tuple[list[str], list[Issue]]:
    items = [match.group("id") for line in _read_lines(path) if (match := PLAN_ITEM.match(line))]
    issues = [Issue("PLAN_DUPLICATE_ID", str(path), item) for item in _duplicates(items)]
    if not items:
        issues.append(Issue("PLAN_HAS_NO_ITEMS", str(path)))
    return items, issues


def load_tracking(path: Path) -> tuple[list[str], set[str], list[Issue]]:
    items: list[str] = []
    closed: set[str] = set()
    for line in _read_lines(path):
        match = TRACKING_ITEM.match(line)
        if match is None:
            continue
        item = match.group("id")
        items.append(item)
        if match.group("mark").lower() == "x":
            closed.add(item)
    issues = [Issue("TRACKING_DUPLICATE_ID", str(path), item) for item in _duplicates(items)]
    if not items:
        issues.append(Issue("TRACKING_HAS_NO_ITEMS", str(path)))
    return items, closed, issues


def load_contracts(directory: Path) -> tuple[list[dict[str, object]], list[Issue]]:
    contracts: list[dict[str, object]] = []
    issues: list[Issue] = []
    for path in sorted(directory.glob("*.json")):
        try:
            payload = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            issues.append(Issue("CONTRACT_JSON_INVALID", str(path)))
            continue
        if not isinstance(payload, dict):
            issues.append(Issue("CONTRACT_ROOT_NOT_OBJECT", str(path)))
            continue
        if not isinstance(payload.get("schema"), str) or not payload["schema"].startswith("NS304_"):
            issues.append(Issue("CONTRACT_SCHEMA_INVALID", str(path)))
        if payload.get("schema_version") != 1:
            issues.append(Issue("CONTRACT_SCHEMA_VERSION_INVALID", str(path)))
        if payload.get("release") != "3.0.4":
            issues.append(Issue("CONTRACT_RELEASE_INVALID", str(path)))
        contracts.append(payload)
    if not contracts:
        issues.append(Issue("CONTRACT_DIRECTORY_EMPTY", str(directory)))
    return contracts, issues


def verify(plan_path: Path, tracking_path: Path, contracts_dir: Path) -> dict[str, object]:
    plan, issues = load_plan(plan_path)
    tracking, closed, tracking_issues = load_tracking(tracking_path)
    contracts, contract_issues = load_contracts(contracts_dir)
    issues.extend(tracking_issues)
    issues.extend(contract_issues)

    plan_set = set(plan)
    tracking_set = set(tracking)
    issues.extend(Issue("TRACKING_MISSING_ITEM", str(tracking_path), item)
                  for item in sorted(plan_set - tracking_set))
    issues.extend(Issue("TRACKING_EXTRA_ITEM", str(tracking_path), item)
                  for item in sorted(tracking_set - plan_set))
    if plan_set == tracking_set and plan != tracking:
        issues.append(Issue("TRACKING_ORDER_MISMATCH", str(tracking_path)))

    return {
        "status": "PASS" if not issues else "FAIL",
        "discovered": {
            "plan_items": len(plan),
            "tracking_items": len(tracking),
            "closed_items": len(closed),
            "contract_documents": len(contracts),
        },
        "issues": [issue.as_dict() for issue in issues],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", required=True, type=Path)
    parser.add_argument("--tracking", required=True, type=Path)
    parser.add_argument("--contracts-dir", required=True, type=Path)
    arguments = parser.parse_args()
    result = verify(arguments.plan, arguments.tracking, arguments.contracts_dir)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
