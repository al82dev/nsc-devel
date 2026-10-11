from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    validate_empty_component_universe,
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
    materialized = tuple(materialized_ranges(repository_root))
    if not materialized or len(set(materialized)) != len(materialized):
        raise RuntimeError("IR historical ranges are empty or duplicated")

    support_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostIrTestSupport.h"
    )
    support = support_path.read_text(encoding="utf-8")
    if "TestDisposition::Executable" in support:
        raise RuntimeError("retired IR cases expose an executable disposition")

    validate_empty_component_universe(repository_root, "SelfHost.IR", "selfhost-ir.jsonl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
