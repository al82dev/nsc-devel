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
    load_component_inventory,
    read_jsonl,
    required_string_field,
    validate_pending_manifest,
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
    inventory = load_component_inventory(
        repository_root,
        "SelfHost.Bootstrap.Infrastructure",
    )
    catalog_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "bootstrap-infrastructure"
        / "SelfHostBootstrapInfrastructureCatalog.jsonl"
    )
    catalog = load_contracts(repository_root, catalog_path)
    if any(
        row["planned_executor"] != "nsc_suite_controller+nsc"
        for row in inventory.allocation
    ):
        raise RuntimeError("Bootstrap.Infrastructure allocation changed its canonical executor")
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
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Bootstrap.Infrastructure cases lost their pending disposition")

    contradiction = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostBootstrapInfrastructurePlanContradictions.jsonl"
    )
    records = read_jsonl(contradiction)
    record_ids: set[str] = set()
    for record in records:
        record_id = required_string_field(
            record,
            "id",
            "SelfHost.Bootstrap.Infrastructure contradiction",
        )
        if (
            not record_id
            or record_id in record_ids
            or record.get("component") != "SelfHost.Bootstrap.Infrastructure"
            or record.get("status") != "OPEN"
        ):
            raise RuntimeError("Bootstrap.Infrastructure contradiction is invalid")
        record_ids.add(record_id)
    if bool(inventory.profile_mismatch_ids()) != bool(records):
        raise RuntimeError("Bootstrap.Infrastructure contradiction differs from live profiles")

    validate_pending_manifest(
        repository_root,
        "SelfHost.Bootstrap.Infrastructure",
        "selfhost-bootstrap-infrastructure.jsonl",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

