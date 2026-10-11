from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    validate_empty_component_universe,
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
    materialized = tuple(materialized_ranges(repository_root))
    if not materialized or len(set(materialized)) != len(materialized):
        raise RuntimeError("Project historical ranges are empty or duplicated")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostProjectTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::Executable" in support:
        raise RuntimeError("retired Project cases expose an executable disposition")

    validate_empty_component_universe(
        repository_root,
        "SelfHost.Project",
        "selfhost-project.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
