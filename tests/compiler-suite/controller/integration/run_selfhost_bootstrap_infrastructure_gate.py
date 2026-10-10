from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path

from selfhost_bootstrap_infrastructure_support import load_contracts


EXPECTED_IDS = [f"T12B-{ordinal:05d}" for ordinal in range(1081, 1519)]
EXPECTED_RANGES = [
    (1081, 1086, "C12B-BSI-001", "selfhost/bootstrap/bootstrap-profile.json"),
    (1087, 1092, "C12B-BSI-002", "selfhost/bootstrap/bootstrap.config.example.json"),
    (1093, 1098, "C12B-BSI-003", "selfhost/bootstrap/bootstrap.lock"),
    (1099, 1104, "C12B-BSI-004", "selfhost/bootstrap/clean_selfhost.py"),
    (1105, 1110, "C12B-BSI-005", "selfhost/bootstrap/dependencies.md"),
    (1111, 1116, "C12B-BSI-006", "selfhost/bootstrap/gate.template.json"),
    (1117, 1122, "C12B-BSI-007", "selfhost/bootstrap/generations.json"),
    (1123, 1128, "C12B-BSI-008", "selfhost/bootstrap/path-configuration.md"),
    (1129, 1134, "C12B-BSI-009", "selfhost/bootstrap/profile_selection.py"),
    (1135, 1140, "C12B-BSI-010", "selfhost/bootstrap/README.md"),
    (1141, 1146, "C12B-BSI-011", "selfhost/bootstrap/recovery.md"),
    (1147, 1152, "C12B-BSI-012", "selfhost/bootstrap/report.template.md"),
    (1153, 1158, "C12B-BSI-013", "selfhost/bootstrap/run_sh44_machine.cmd"),
    (1159, 1164, "C12B-BSI-014", "selfhost/bootstrap/schemas/bootstrap-lock.schema.json"),
    (1165, 1170, "C12B-BSI-015", "selfhost/bootstrap/schemas/bootstrap-profile.schema.json"),
    (1171, 1176, "C12B-BSI-016", "selfhost/bootstrap/schemas/gate.schema.json"),
    (1177, 1182, "C12B-BSI-017", "selfhost/bootstrap/schemas/report.schema.json"),
    (1183, 1188, "C12B-BSI-018", "selfhost/bootstrap/SH44_PEER_README.txt"),
    (1189, 1194, "C12B-BSI-019", "selfhost/bootstrap/verify_advanced_toolchain.py"),
    (1195, 1200, "C12B-BSI-020", "selfhost/bootstrap/verify_arrays_memory.py"),
    (1201, 1206, "C12B-BSI-021", "selfhost/bootstrap/verify_backend_general.py"),
    (1207, 1212, "C12B-BSI-022", "selfhost/bootstrap/verify_bootstrap_dependencies.py"),
    (1213, 1218, "C12B-BSI-023", "selfhost/bootstrap/verify_bootstrap_surface.py"),
    (1219, 1224, "C12B-BSI-024", "selfhost/bootstrap/verify_callable_graph.py"),
    (1225, 1230, "C12B-BSI-025", "selfhost/bootstrap/verify_closure.py"),
    (1231, 1236, "C12B-BSI-026", "selfhost/bootstrap/verify_cross_machine_determinism.py"),
    (1237, 1242, "C12B-BSI-027", "selfhost/bootstrap/verify_data_layout.py"),
    (1243, 1248, "C12B-BSI-028", "selfhost/bootstrap/verify_developer_experience.py"),
    (1249, 1254, "C12B-BSI-029", "selfhost/bootstrap/verify_distribution_final.py"),
    (1255, 1260, "C12B-BSI-030", "selfhost/bootstrap/verify_driver_products.py"),
    (1261, 1266, "C12B-BSI-031", "selfhost/bootstrap/verify_eh_regions.py"),
    (1267, 1272, "C12B-BSI-032", "selfhost/bootstrap/verify_environment.py"),
    (1273, 1278, "C12B-BSI-033", "selfhost/bootstrap/verify_final_certification.py"),
    (1279, 1284, "C12B-BSI-034", "selfhost/bootstrap/verify_first_generation.py"),
    (1285, 1290, "C12B-BSI-035", "selfhost/bootstrap/verify_fixed_point.py"),
    (1291, 1296, "C12B-BSI-036", "selfhost/bootstrap/verify_frontend.py"),
    (1297, 1302, "C12B-BSI-037", "selfhost/bootstrap/verify_functional_equivalence.py"),
    (1303, 1308, "C12B-BSI-038", "selfhost/bootstrap/verify_generics_aot.py"),
    (1309, 1314, "C12B-BSI-039", "selfhost/bootstrap/verify_image_replay_retired.py"),
    (1315, 1320, "C12B-BSI-040", "selfhost/bootstrap/verify_interop_base.py"),
    (1321, 1326, "C12B-BSI-041", "selfhost/bootstrap/verify_interop_parity.py"),
    (1327, 1332, "C12B-BSI-042", "selfhost/bootstrap/verify_ir.py"),
    (1333, 1338, "C12B-BSI-043", "selfhost/bootstrap/verify_llvm_backend.py"),
    (1339, 1344, "C12B-BSI-044", "selfhost/bootstrap/verify_llvm_c_contract.py"),
    (1345, 1350, "C12B-BSI-045", "selfhost/bootstrap/verify_low_level_parity.py"),
    (1351, 1356, "C12B-BSI-046", "selfhost/bootstrap/verify_module_set.py"),
    (1357, 1362, "C12B-BSI-047", "selfhost/bootstrap/verify_native_text.py"),
    (1363, 1368, "C12B-BSI-048", "selfhost/bootstrap/verify_nsc_next_skeleton.py"),
    (1369, 1374, "C12B-BSI-049", "selfhost/bootstrap/verify_object_model.py"),
    (1375, 1380, "C12B-BSI-050", "selfhost/bootstrap/verify_ownership_cleanup.py"),
    (1381, 1386, "C12B-BSI-051", "selfhost/bootstrap/verify_performance.py"),
    (1387, 1392, "C12B-BSI-052", "selfhost/bootstrap/verify_profile.py"),
    (1393, 1398, "C12B-BSI-053", "selfhost/bootstrap/verify_program_ir.py"),
    (1399, 1404, "C12B-BSI-054", "selfhost/bootstrap/verify_project_driver.py"),
    (1405, 1410, "C12B-BSI-055", "selfhost/bootstrap/verify_promotion_readiness.py"),
    (1411, 1416, "C12B-BSI-056", "selfhost/bootstrap/verify_repeated_fixed_point.py"),
    (1417, 1422, "C12B-BSI-057", "selfhost/bootstrap/verify_robustness_final.py"),
    (1423, 1428, "C12B-BSI-058", "selfhost/bootstrap/verify_robustness.py"),
    (1429, 1434, "C12B-BSI-059", "selfhost/bootstrap/verify_runtime_bootstrap_contract.py"),
    (1435, 1440, "C12B-BSI-060", "selfhost/bootstrap/verify_runtime_library.py"),
    (1441, 1446, "C12B-BSI-061", "selfhost/bootstrap/verify_self_hosted_generation.py"),
    (1447, 1452, "C12B-BSI-062", "selfhost/bootstrap/verify_semantics.py"),
    (1453, 1458, "C12B-BSI-063", "selfhost/bootstrap/verify_source_bool_void.py"),
    (1459, 1464, "C12B-BSI-064", "selfhost/bootstrap/verify_source_control_flow.py"),
    (1465, 1470, "C12B-BSI-065", "selfhost/bootstrap/verify_source_derived_backend.py"),
    (1471, 1476, "C12B-BSI-066", "selfhost/bootstrap/verify_source_expressions.py"),
    (1477, 1482, "C12B-BSI-067", "selfhost/bootstrap/verify_source_floating_scalars.py"),
    (1483, 1488, "C12B-BSI-068", "selfhost/bootstrap/verify_source_functions.py"),
    (1489, 1494, "C12B-BSI-069", "selfhost/bootstrap/verify_source_graph.py"),
    (1495, 1500, "C12B-BSI-070", "selfhost/bootstrap/verify_source_integer_widths.py"),
    (1501, 1506, "C12B-BSI-071", "selfhost/bootstrap/verify_source_loop_control.py"),
    (1507, 1512, "C12B-BSI-072", "selfhost/bootstrap/verify_source_pointers.py"),
    (1513, 1518, "C12B-BSI-073", "selfhost/bootstrap/verify_specialized_code.py"),
]
RANGE_ROW = re.compile(
    r'\{\s*(\d+),\s*(\d+),\s*"(C12B-BSI-\d{3})",\s*'
    r'"[^"]+",\s*"([^"]+)"',
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()
    repository_root = options.repository_root.resolve()
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [
        row
        for row in read_csv(inventory / "test-case-design.csv")
        if row["component"] == "SelfHost.Bootstrap.Infrastructure"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Bootstrap.Infrastructure"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Bootstrap.Infrastructure"
    ]
    catalog_path = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "bootstrap-infrastructure"
        / "SelfHostBootstrapInfrastructureCatalog.jsonl"
    )
    catalog = load_contracts(repository_root, catalog_path)

    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical T12B-01081..T12B-01518")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("allocation does not contain canonical T12B-01081..T12B-01518")
    if len(contracts) != 73 or len(catalog) != 73:
        raise RuntimeError("Bootstrap.Infrastructure must contain exactly 73 contracts")
    if any(row["default_profile"] != "NIGHTLY" for row in allocation):
        raise RuntimeError("Bootstrap.Infrastructure allocation no longer assigns all cases to NIGHTLY")
    if any(row["layer"] != "self-host" for row in allocation):
        raise RuntimeError("Bootstrap.Infrastructure allocation changed its self-host layer")
    if any(row["planned_executor"] != "nsc_suite_controller+nsc" for row in allocation):
        raise RuntimeError("Bootstrap.Infrastructure allocation changed its canonical executor")
    if materialized_ranges(repository_root) != EXPECTED_RANGES:
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
    records = jsonl_rows(contradiction)
    if len(records) != 1 or records[0].get("status") != "OPEN":
        raise RuntimeError("Bootstrap.Infrastructure profile contradiction is not preserved")

    retired_fragment = (
        repository_root
        / "tests"
        / "compiler-suite"
        / "manifest"
        / "fragments"
        / "selfhost-bootstrap-infrastructure.jsonl"
    )
    if retired_fragment.exists():
        raise RuntimeError("empty Bootstrap.Infrastructure fragment must remain retired")
    central = repository_root / "tests" / "compiler-suite" / "manifest" / "cases.jsonl"
    if any(
        row.get("component") == "SelfHost.Bootstrap.Infrastructure"
        for row in jsonl_rows(central)
    ):
        raise RuntimeError("pending Bootstrap.Infrastructure cases entered the central manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

