from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    validate_empty_component_universe,
)

RANGE_ROW = re.compile(r'\{(\d+), (\d+), "(CSH-DRV-\d{3})", "([^"]+)"')


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostDriverCliFrontendModule.cpp",
        root / "SelfHostDriverEmissionModule.cpp",
        root / "SelfHostDriverAuditDispatchModule.cpp",
    ]
    rows: list[tuple[int, int, str]] = []
    for path in paths:
        for first, last, contract, _oracle in RANGE_ROW.findall(path.read_text(encoding="utf-8")):
            rows.append((int(first), int(last), contract))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    materialized = tuple(materialized_ranges(repository_root))
    if not materialized or len(set(materialized)) != len(materialized):
        raise RuntimeError("Driver historical ranges are empty or duplicated")

    support_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostDriverTestSupport.h"
    )
    support = support_path.read_text(encoding="utf-8")
    if "TestDisposition::Executable" in support:
        raise RuntimeError("retired Driver cases expose an executable disposition")

    validate_empty_component_universe(repository_root, "SelfHost.Driver", "selfhost-driver.jsonl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
