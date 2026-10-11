from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    validate_empty_component_universe,
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
    """Validate that retired CodeGen materialization stays outside active execution."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    root = options.repository_root.resolve()
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
    if not materialized_by_id:
        raise RuntimeError("CodeGen historical materialization unexpectedly disappeared")

    support = (
        root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostCodeGenTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::Executable" in support:
        raise RuntimeError("retired CodeGen cases expose an executable disposition")
    if "TestDisposition::KnownProductDefect" in support:
        raise RuntimeError("CodeGen asserts a product defect without an executable oracle")

    validate_empty_component_universe(root, "SelfHost.CodeGen", "selfhost-codegen.jsonl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
