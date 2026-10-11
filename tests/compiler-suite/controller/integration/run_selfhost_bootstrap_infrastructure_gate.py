from __future__ import annotations

import argparse
import re
from pathlib import Path

from selfhost_bootstrap_infrastructure_support import (
    BootstrapInfrastructureContract,
    load_contracts,
)
from selfhost_inventory import (
    case_ordinal,
    validate_empty_component_universe,
)
RANGE_ROW = re.compile(
    r'\{\s*(\d+),\s*(\d+),\s*"(C12B-BSI-\d{3})",\s*'
    r'"[^"]+",\s*"([^"]+)"',
    re.MULTILINE,
)


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostBootstrapInfrastructureCatalog001To012Module.cpp",
        root / "SelfHostBootstrapInfrastructureCatalog013To024Module.cpp",
        root / "SelfHostBootstrapInfrastructureCatalog025To036Module.cpp",
        root / "SelfHostBootstrapInfrastructureCatalog037To048Module.cpp",
        root / "SelfHostBootstrapInfrastructureCatalog049To060Module.cpp",
        root / "SelfHostBootstrapInfrastructureCatalog061To073Module.cpp",
    ]
    rows: list[tuple[int, int, str, str]] = []
    for path in paths:
        for first, last, contract, source_path in RANGE_ROW.findall(
            path.read_text(encoding="utf-8")
        ):
            rows.append((int(first), int(last), contract, source_path))
    return rows


def catalog_ranges(
    repository_root: Path,
    contracts: tuple[BootstrapInfrastructureContract, ...],
) -> set[tuple[int, int, str, str]]:
    result: set[tuple[int, int, str, str]] = set()
    for contract in contracts:
        ordinals = tuple(case_ordinal(case_id) for case_id in contract.case_ids)
        if ordinals != tuple(range(ordinals[0], ordinals[-1] + 1)):
            raise RuntimeError(f"catalog contract {contract.contract_id} is not contiguous")
        source_path = contract.source_path.relative_to(repository_root).as_posix()
        row = (ordinals[0], ordinals[-1], contract.contract_id, source_path)
        if row in result:
            raise RuntimeError(f"catalog repeats materialized range {row!r}")
        result.add(row)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    catalog_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "bootstrap-infrastructure"
        / "SelfHostBootstrapInfrastructureCatalog.jsonl"
    )
    catalog = load_contracts(repository_root, catalog_path)
    materialized = tuple(materialized_ranges(repository_root))
    expected = catalog_ranges(repository_root, catalog)
    if len(set(materialized)) != len(materialized) or set(materialized) != expected:
        raise RuntimeError("materialized modules changed range, contract, or source lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostBootstrapInfrastructureTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::Executable" in support:
        raise RuntimeError("retired Bootstrap.Infrastructure cases expose an executable disposition")

    validate_empty_component_universe(
        repository_root,
        "SelfHost.Bootstrap.Infrastructure",
        "selfhost-bootstrap-infrastructure.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

