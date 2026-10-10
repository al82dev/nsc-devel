from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


EXPECTED_IDS = [f"T12B-{ordinal:05d}" for ordinal in range(1519, 1807)]
EXPECTED_RANGES = [
    (1519, 1524, "C12B-SHC-001", "selfhost/contracts/advanced-toolchain.contract.json"),
    (1525, 1530, "C12B-SHC-002", "selfhost/contracts/arrays-memory.contract.json"),
    (1531, 1536, "C12B-SHC-003", "selfhost/contracts/backend-general.contract.json"),
    (1537, 1542, "C12B-SHC-004", "selfhost/contracts/closure.contract.json"),
    (1543, 1548, "C12B-SHC-005", "selfhost/contracts/cross-machine-determinism.contract.json"),
    (1549, 1554, "C12B-SHC-006", "selfhost/contracts/data-layout.contract.json"),
    (1555, 1560, "C12B-SHC-007", "selfhost/contracts/developer-experience.contract.json"),
    (1561, 1566, "C12B-SHC-008", "selfhost/contracts/diagnostics.map.json"),
    (1567, 1572, "C12B-SHC-009", "selfhost/contracts/distribution-final.contract.json"),
    (1573, 1578, "C12B-SHC-010", "selfhost/contracts/final-certification.contract.json"),
    (1579, 1584, "C12B-SHC-011", "selfhost/contracts/first-generation.contract.json"),
    (1585, 1590, "C12B-SHC-012", "selfhost/contracts/fixed-point.contract.json"),
    (1591, 1596, "C12B-SHC-013", "selfhost/contracts/frontend.contract.json"),
    (1597, 1602, "C12B-SHC-014", "selfhost/contracts/functional-equivalence.contract.json"),
    (1603, 1608, "C12B-SHC-015", "selfhost/contracts/generics-aot.contract.json"),
    (1609, 1614, "C12B-SHC-016", "selfhost/contracts/image-replay-retirement.contract.json"),
    (1615, 1620, "C12B-SHC-017", "selfhost/contracts/interop-base.contract.json"),
    (1621, 1626, "C12B-SHC-018", "selfhost/contracts/interop-parity.contract.json"),
    (1627, 1632, "C12B-SHC-019", "selfhost/contracts/ir.contract.json"),
    (1633, 1638, "C12B-SHC-020", "selfhost/contracts/llvm-backend.contract.json"),
    (1639, 1644, "C12B-SHC-021", "selfhost/contracts/llvm-c-api.contract.json"),
    (1645, 1650, "C12B-SHC-022", "selfhost/contracts/low-level-parity.contract.json"),
    (1651, 1656, "C12B-SHC-023", "selfhost/contracts/module-set.contract.json"),
    (1657, 1662, "C12B-SHC-024", "selfhost/contracts/native-text.contract.json"),
    (1663, 1668, "C12B-SHC-025", "selfhost/contracts/object-model.contract.json"),
    (1669, 1674, "C12B-SHC-026", "selfhost/contracts/performance.contract.json"),
    (1675, 1680, "C12B-SHC-027", "selfhost/contracts/program-ir.contract.json"),
    (1681, 1686, "C12B-SHC-028", "selfhost/contracts/project-driver.contract.json"),
    (1687, 1692, "C12B-SHC-029", "selfhost/contracts/promotion-readiness.contract.json"),
    (1693, 1698, "C12B-SHC-030", "selfhost/contracts/README.md"),
    (1699, 1704, "C12B-SHC-031", "selfhost/contracts/repeated-fixed-point.contract.json"),
    (1705, 1710, "C12B-SHC-032", "selfhost/contracts/robustness-final.contract.json"),
    (1711, 1716, "C12B-SHC-033", "selfhost/contracts/robustness.contract.json"),
    (1717, 1722, "C12B-SHC-034", "selfhost/contracts/runtime-bootstrap.contract.json"),
    (1723, 1728, "C12B-SHC-035", "selfhost/contracts/runtime-library.contract.json"),
    (1729, 1734, "C12B-SHC-036", "selfhost/contracts/self-hosted-generation.contract.json"),
    (1735, 1740, "C12B-SHC-037", "selfhost/contracts/semantic.contract.json"),
    (1741, 1746, "C12B-SHC-038", "selfhost/contracts/source-bool-void.contract.json"),
    (1747, 1752, "C12B-SHC-039", "selfhost/contracts/source-control-flow.contract.json"),
    (1753, 1758, "C12B-SHC-040", "selfhost/contracts/source-derived-backend.contract.json"),
    (1759, 1764, "C12B-SHC-041", "selfhost/contracts/source-expressions.contract.json"),
    (1765, 1770, "C12B-SHC-042", "selfhost/contracts/source-floating-scalars.contract.json"),
    (1771, 1776, "C12B-SHC-043", "selfhost/contracts/source-functions.contract.json"),
    (1777, 1782, "C12B-SHC-044", "selfhost/contracts/source-graph.contract.json"),
    (1783, 1788, "C12B-SHC-045", "selfhost/contracts/source-integer-widths.contract.json"),
    (1789, 1794, "C12B-SHC-046", "selfhost/contracts/source-loop-control.contract.json"),
    (1795, 1800, "C12B-SHC-047", "selfhost/contracts/source-pointers.contract.json"),
    (1801, 1806, "C12B-SHC-048", "selfhost/contracts/specialized-code.contract.json"),
]
RANGE_ROW = re.compile(
    r'\{\s*(\d+),\s*(\d+),\s*"(C12B-SHC-\d{3})",\s*'
    r'"([^"]+)"',
    re.MULTILINE,
)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def jsonl_rows(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        payload = json.loads(line)
        if not isinstance(payload, dict):
            raise RuntimeError(f"{path}:{line_number} is not a JSON object")
        rows.append(payload)
    return rows


def materialized_ranges(repository_root: Path) -> list[tuple[int, int, str, str]]:
    root = repository_root / "tests" / "compiler-suite" / "selfhost" / "modules"
    paths = [
        root / "SelfHostContractsToolchainModule.cpp",
        root / "SelfHostContractsIrRuntimeModule.cpp",
        root / "SelfHostContractsDistributionModule.cpp",
        root / "SelfHostContractsSourceModule.cpp",
    ]
    rows: list[tuple[int, int, str, str]] = []
    for path in paths:
        for first, last, contract, source_path in RANGE_ROW.findall(
            path.read_text(encoding="utf-8")
        ):
            rows.append((int(first), int(last), contract, source_path))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [
        row
        for row in read_csv(inventory / "test-case-design.csv")
        if row["component"] == "SelfHost.Contracts"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Contracts"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Contracts"
    ]

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical T12B-01519..T12B-01806")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical T12B-01519..T12B-01806")
    if len(contracts) != 48:
        raise RuntimeError("contract inventory does not contain exactly 48 Contracts contracts")
    if any(row["default_profile"] != "NIGHTLY" for row in allocation):
        raise RuntimeError("SelfHost.Contracts allocation no longer assigns all cases to NIGHTLY")
    if any(row["layer"] != "self-host" for row in allocation):
        raise RuntimeError("SelfHost.Contracts allocation changed its self-host layer")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("SelfHost.Contracts allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
        raise RuntimeError("consolidated modules changed range, contract, or source lineage")

    support = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostContractsTestSupport.h"
    ).read_text(encoding="utf-8")
    if "TestDisposition::InfrastructurePending" not in support:
        raise RuntimeError("Contracts modules do not preserve InfrastructurePending disposition")

    catalog = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "contracts"
        / "SelfHostContractsCatalog.jsonl"
    )
    catalog_rows = jsonl_rows(catalog)
    if len(catalog_rows) != 48:
        raise RuntimeError("Contracts catalog does not contain exactly 48 contract rows")
    catalog_ids = [case_id for row in catalog_rows for case_id in row["case_ids"]]
    if catalog_ids != EXPECTED_IDS:
        raise RuntimeError("Contracts catalog case IDs differ from the canonical sequence")

    contradiction = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostContractsPlanContradictions.jsonl"
    )
    records = jsonl_rows(contradiction)
    if len(records) != 1 or records[0].get("status") != "OPEN":
        raise RuntimeError("Contracts profile contradiction is not preserved")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-contracts.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty SelfHost.Contracts executable fragment must remain retired")

    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(row.get("component") == "SelfHost.Contracts" for row in jsonl_rows(central)):
        raise RuntimeError("pending SelfHost.Contracts cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

