from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    load_component_inventory,
    validate_pending_manifest,
    validate_unique_materialized_ids,
)

DESCRIPTOR = re.compile(
    r'\{(\d+)U, (\d+)U, (\d+)U, (\d+)U, "(unit|component|integration)",\s*'
    r'"([^"]+)"\}'
)


def descriptors(repository_root: Path) -> list[tuple[int, int, int, int, str, str]]:
    module_root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        module_root / "SelfHostCodeGenFoundationModule.cpp",
        module_root / "SelfHostCodeGenLoweringEmissionModule.cpp",
    ]
    found: list[tuple[int, int, int, int, str, str]] = []
    for path in paths:
        for match in DESCRIPTOR.findall(path.read_text(encoding="utf-8")):
            ordinal, first, count, nightly, layer, oracle = match
            found.append((int(ordinal), int(first), int(count), int(nightly), layer, oracle))
    return found


def main() -> int:
    """Validate CodeGen materialization and classification without executing the product."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    root = options.repository_root.resolve()
    inventory = load_component_inventory(root, "SelfHost.CodeGen")
    if any(
        row["planned_executor"] != "nsc_suite_controller+nsc"
        for row in inventory.allocation
    ):
        raise RuntimeError("SelfHost.CodeGen allocation changed its canonical executor")

    materialized: list[tuple[str, str, str, str, str]] = []
    for ordinal, first, count, nightly_count, layer, oracle in descriptors(root):
        contract_id = f"CSH-CG-{ordinal:03d}"
        for offset in range(count):
            profile = "NIGHTLY" if offset >= count - nightly_count else "PR"
            materialized.append(
                (f"TSH-{first + offset:05d}", contract_id, layer, profile, oracle)
            )

    materialized_by_id = validate_unique_materialized_ids(
        "SelfHost.CodeGen", materialized
    )
    if materialized_by_id.keys() != inventory.design_by_id.keys():
        raise RuntimeError("CodeGen modules differ from the canonical denominator")
    for case_id, row in materialized_by_id.items():
        design_row = inventory.design_by_id[case_id]
        allocation_row = inventory.allocation_by_id[case_id]
        contract = inventory.contracts_by_id[design_row["contract_id"]]
        expected = (
            case_id,
            design_row["contract_id"],
            design_row["layer"],
            allocation_row["default_profile"],
            contract["observable_oracle"],
        )
        if row != expected:
            raise RuntimeError(f"CodeGen module metadata differs for {case_id}")

    support = (
        root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostCodeGenTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("CodeGen cases lack InfrastructurePending disposition")
    if "TestDisposition::KnownProductDefect" in support:
        raise RuntimeError("CodeGen asserts a product defect without an executable oracle")

    validate_pending_manifest(root, "SelfHost.CodeGen", "selfhost-codegen.jsonl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
