from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    expected_ranges,
    load_component_inventory,
    read_jsonl,
    required_string_field,
    validate_pending_manifest,
)

RANGE_ROW = re.compile(r"\{\s*(\d+),\s*(\d+),\s*\"(CSH-PRO-\d{3})\"")


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostProjectIdentityModule.cpp",
        root / "SelfHostProjectDependenciesModule.cpp",
        root / "SelfHostProjectInventoryModule.cpp",
    ]
    rows: list[tuple[int, int, str]] = []
    for path in paths:
        for first, last, contract in RANGE_ROW.findall(path.read_text(encoding="utf-8")):
            rows.append((int(first), int(last), contract))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = load_component_inventory(repository_root, "SelfHost.Project")
    if any(
        row["planned_executor"] != "nsc_suite_controller+nsc"
        for row in inventory.allocation
    ):
        raise RuntimeError("SelfHost.Project allocation changed its canonical executor")
    materialized = tuple(materialized_ranges(repository_root))
    expected = expected_ranges(inventory, include_layer=False)
    if len(set(materialized)) != len(materialized) or set(materialized) != set(expected):
        raise RuntimeError("consolidated modules changed Project range or contract lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostProjectTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Project modules do not preserve InfrastructurePending disposition")
    contradiction = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostProjectPlanContradictions.jsonl"
    )
    records = read_jsonl(contradiction)
    record_ids: set[str] = set()
    for record in records:
        record_id = required_string_field(
            record,
            "id",
            "SelfHost.Project contradiction",
        )
        if (
            not record_id
            or record_id in record_ids
            or record.get("component") != "SelfHost.Project"
            or record.get("status") != "OPEN"
        ):
            raise RuntimeError("Project contradiction contains an invalid record")
        record_ids.add(record_id)
    if bool(inventory.profile_mismatch_ids()) != bool(records):
        raise RuntimeError("Project contradiction does not match the live profile mismatch")

    validate_pending_manifest(
        repository_root,
        "SelfHost.Project",
        "selfhost-project.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
