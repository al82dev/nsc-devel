from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_inventory import (
    validate_empty_component_universe,
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
    """Validate that retired Bootstrap materialization stays outside active execution."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    rows = module_rows(repository_root)
    materialized = validate_unique_materialized_ids("SelfHost.Bootstrap", rows)
    if not materialized:
        raise RuntimeError("Bootstrap historical materialization unexpectedly disappeared")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostBootstrapTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::Executable" in support:
        raise RuntimeError("retired Bootstrap cases expose an executable disposition")
    if "TestDisposition::KnownProductDefect" in support:
        raise RuntimeError("a product defect was asserted without an executable oracle")

    validate_empty_component_universe(
        repository_root,
        "SelfHost.Bootstrap",
        "selfhost-bootstrap.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
