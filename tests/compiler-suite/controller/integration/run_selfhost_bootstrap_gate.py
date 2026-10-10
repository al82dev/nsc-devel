from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    load_component_inventory,
    validate_pending_manifest,
    validate_unique_materialized_ids,
)

CASE_ROW = re.compile(
    r'\{"(TSH-\d{5})", "(CSH-BOO-\d{3})", "(component|integration)", '
    r'"(PR|NIGHTLY)",\s*\n\s*"([^"]+)", (k[A-Za-z]+Requirement)\}'
)


def module_rows(repository_root: Path) -> list[tuple[str, str, str, str, str, str]]:
    module_root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        module_root / "SelfHostBootstrapProfileModule.cpp",
        module_root / "SelfHostBootstrapBuildModule.cpp",
    ]
    rows: list[tuple[str, str, str, str, str, str]] = []
    for path in paths:
        rows.extend(CASE_ROW.findall(path.read_text(encoding="utf-8")))
    return rows


def main() -> int:
    """Validate denominator, lineage, pending disposition, and empty executable fragment."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = load_component_inventory(repository_root, "SelfHost.Bootstrap")
    if any(
        row["planned_executor"] != "nsc_suite_controller+nsc"
        for row in inventory.allocation
    ):
        raise RuntimeError("SelfHost.Bootstrap allocation changed its canonical executor")

    rows = module_rows(repository_root)
    materialized = validate_unique_materialized_ids("SelfHost.Bootstrap", rows)
    if materialized.keys() != inventory.design_by_id.keys():
        raise RuntimeError("Bootstrap modules differ from the canonical denominator")
    for case_id, row in materialized.items():
        design_row = inventory.design_by_id[case_id]
        allocation_row = inventory.allocation_by_id[case_id]
        if row[1] != design_row["contract_id"]:
            raise RuntimeError(f"Bootstrap module changes the contract of {case_id}")
        if row[2] != design_row["layer"]:
            raise RuntimeError(f"Bootstrap module changes the layer of {case_id}")
        if row[3] != allocation_row["default_profile"]:
            raise RuntimeError(f"Bootstrap module changes the profile of {case_id}")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostBootstrapTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("modules do not preserve InfrastructurePending disposition")
    if "TestDisposition::KnownProductDefect" in support:
        raise RuntimeError("a product defect was asserted without an executable oracle")

    validate_pending_manifest(
        repository_root,
        "SelfHost.Bootstrap",
        "selfhost-bootstrap.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
