from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    load_component_inventory,
    read_jsonl,
    validate_pending_manifest,
    validate_unique_materialized_ids,
)

CASE_ROW = re.compile(
    r'\{"(TSH-\d{5})", "(CSH-DOC-\d{3})", "([^"]+)",\s*\n\s*'
    r'(k[A-Za-z]+Requirement)\}'
)


def module_rows(repository_root: Path) -> list[tuple[str, str, str, str]]:
    module_root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        module_root / "SelfHostDocumentationArchitectureModule.cpp",
        module_root / "SelfHostDocumentationPolicyModule.cpp",
    ]
    rows: list[tuple[str, str, str, str]] = []
    for path in paths:
        rows.extend(CASE_ROW.findall(path.read_text(encoding="utf-8")))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = load_component_inventory(repository_root, "SelfHost.Documentation")
    if any(
        row["planned_executor"] != "nsc_suite_controller+nsc"
        for row in inventory.allocation
    ):
        raise RuntimeError("SelfHost.Documentation allocation changed its canonical executor")

    rows = module_rows(repository_root)
    materialized = validate_unique_materialized_ids("SelfHost.Documentation", rows)
    if materialized.keys() != inventory.design_by_id.keys():
        raise RuntimeError("Documentation modules differ from the canonical denominator")
    for case_id, row in materialized.items():
        if row[1] != inventory.design_by_id[case_id]["contract_id"]:
            raise RuntimeError(f"Documentation module changes the contract of {case_id}")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostDocumentationTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("modules do not preserve InfrastructurePending disposition")

    conflict_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostDocumentationPlanContradictions.jsonl"
    )
    conflicts = read_jsonl(conflict_path)
    affected: set[str] = set()
    for conflict in conflicts:
        if conflict.get("component") != "SelfHost.Documentation":
            raise RuntimeError("Documentation conflict contains a foreign component")
        case_ids = conflict.get("affected_case_ids")
        if (
            not isinstance(case_ids, list)
            or not case_ids
            or affected & {str(value) for value in case_ids}
        ):
            raise RuntimeError("Documentation conflicts repeat or omit their case list")
        affected.update(str(value) for value in case_ids)
    if affected != set(inventory.profile_mismatch_ids()):
        raise RuntimeError("Documentation conflicts differ from the live profile mismatches")

    validate_pending_manifest(
        repository_root,
        "SelfHost.Documentation",
        "selfhost-documentation.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
