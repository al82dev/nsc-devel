from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    expected_nightly_ordinals,
    expected_ranges,
    load_component_inventory,
    materialized_nightly_ordinals,
    validate_pending_manifest,
)

RANGE_ROW = re.compile(
    r'\{(\d+), (\d+), "(CSH-IR-\d{3})", "(unit|component|integration)", "([^"]+)"'
)


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostIrHirFlowModule.cpp",
        root / "SelfHostIrMirVerificationModule.cpp",
        root / "SelfHostIrDominanceLifecycleModule.cpp",
    ]
    rows: list[tuple[int, int, str, str]] = []
    for path in paths:
        for first, last, contract, layer, _oracle in RANGE_ROW.findall(
            path.read_text(encoding="utf-8")
        ):
            rows.append((int(first), int(last), contract, layer))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = load_component_inventory(repository_root, "SelfHost.IR")
    if any(
        row["planned_executor"] != "nsc_suite_controller+nsc"
        for row in inventory.allocation
    ):
        raise RuntimeError("SelfHost.IR allocation changed its canonical executor")
    materialized = tuple(materialized_ranges(repository_root))
    expected = expected_ranges(inventory, include_layer=True)
    if len(set(materialized)) != len(materialized) or set(materialized) != set(expected):
        raise RuntimeError("consolidated modules changed IR range, layer, or contract lineage")

    support_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostIrTestSupport.h"
    )
    support = support_path.read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("IR modules do not preserve InfrastructurePending disposition")
    if materialized_nightly_ordinals(support_path) != expected_nightly_ordinals(inventory):
        raise RuntimeError("IR support profiles differ from the live allocation")

    validate_pending_manifest(repository_root, "SelfHost.IR", "selfhost-ir.jsonl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
