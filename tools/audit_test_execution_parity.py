"""Audit every historical CTest identifier against the consolidated graph.

The audit keeps execution coverage separate from migration completion.  A
legacy test can still be runnable through a transitional C++ adapter while its
final NativeSharp manifest remains pending; that situation is visible and does
not satisfy the final cutover gate.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable


DESTINATION_LAYERS = {
    "KEEP_CPP_UNIT": "unit",
    "MIGRATE_NS_FIXTURE": "manifest",
    "KEEP_SPECIALIZED": "specialized",
}


class ParityError(ValueError):
    """Report an invalid inventory or an execution catalog that cannot be trusted."""


@dataclass(frozen=True)
class ExecutionPath:
    """Describe one concrete Builder 2 route that owns a historical identifier."""

    layer: str
    identifier: str
    runnable: bool
    detail: str


@dataclass(frozen=True)
class ParityRow:
    """Preserve one historical test and its exact new execution correspondence."""

    ctest_id: str
    destination: str
    expected_layer: str
    primary_component: str
    owner_path: str
    expectation_contract: str
    expectation_signals: tuple[str, ...]
    execution_state: str
    migration_state: str
    paths: tuple[ExecutionPath, ...]


def sha256_file(path: Path) -> str:
    """Hash one evidence file iteratively without loading it completely into memory."""

    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def load_legacy_rows(path: Path) -> list[dict[str, Any]]:
    """Load and validate the frozen 684-row destination matrix."""

    payload = json.loads(path.read_text(encoding="utf-8"))
    rows = payload.get("tests") if isinstance(payload, dict) else None
    if not isinstance(rows, list):
        raise ParityError(f"legacy matrix has no tests array: {path}")
    identifiers: set[str] = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ParityError(f"legacy matrix contains a non-object row: {path}")
        identifier = row.get("ctest_id")
        if not isinstance(identifier, str) or not identifier:
            raise ParityError(f"legacy matrix contains a row without ctest_id: {path}")
        if identifier in identifiers:
            raise ParityError(f"legacy matrix contains duplicate ctest_id: {identifier}")
        identifiers.add(identifier)
    return rows


def parse_unit_catalog(output: str) -> dict[str, str]:
    """Parse the authoritative ``nsc_unit_tests.exe --list`` protocol."""

    catalog: dict[str, str] = {}
    declared_count: int | None = None
    for line in output.splitlines():
        if line.startswith("cases="):
            declared_count = int(line.split("=", 1)[1])
            continue
        fields = line.split("\t")
        if len(fields) != 3:
            continue
        identifier, component, _ = fields
        if identifier in catalog:
            raise ParityError(f"unit catalog contains duplicate id: {identifier}")
        catalog[identifier] = component
    if declared_count is None:
        raise ParityError("unit catalog did not publish its case count")
    if declared_count != len(catalog):
        raise ParityError(
            f"unit catalog count differs: declared={declared_count}, parsed={len(catalog)}"
        )
    return catalog


def run_unit_catalog(executable: Path) -> dict[str, str]:
    """Enumerate the built consolidated registry without executing its tests."""

    if not executable.is_file():
        raise ParityError(f"unit executable does not exist: {executable}")
    completed = subprocess.run(
        [str(executable), "--list"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if completed.returncode != 0:
        raise ParityError(
            f"unit catalog failed with {completed.returncode}: {completed.stderr.strip()}"
        )
    return parse_unit_catalog(completed.stdout)


def iter_files(root: Path, suffix: str) -> Iterable[Path]:
    """Traverse a directory with an explicit heap-backed worklist and stable order."""

    pending = [root]
    found: list[Path] = []
    while pending:
        current = pending.pop()
        entries = sorted(os.scandir(current), key=lambda entry: entry.name.casefold())
        for entry in entries:
            path = Path(entry.path)
            if entry.is_dir(follow_symlinks=False):
                pending.append(path)
            elif entry.is_file(follow_symlinks=False) and path.name.endswith(suffix):
                found.append(path)
    return tuple(sorted(found, key=lambda path: path.as_posix().casefold()))


def load_manifest_catalog(root: Path) -> dict[str, Path]:
    """Discover every external case manifest and reject duplicate identifiers."""

    catalog: dict[str, Path] = {}
    for path in iter_files(root, ".json"):
        payload = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(payload, dict) or "id" not in payload:
            continue
        identifier = payload["id"]
        if not isinstance(identifier, str) or not identifier:
            raise ParityError(f"manifest has an invalid id: {path}")
        if identifier in catalog:
            raise ParityError(
                f"manifest id {identifier} is duplicated by {catalog[identifier]} and {path}"
            )
        catalog[identifier] = path
    return catalog


def load_specialized_catalog(matrix: Path, repo_root: Path) -> dict[str, tuple[bool, str]]:
    """Use the production SuiteCatalog so the audit shares its exact blocking policy."""

    harness = repo_root / "tests" / "harness"
    sys.path.insert(0, str(harness))
    try:
        from nsc_test.suites import SuiteCatalog

        catalog = SuiteCatalog.load(matrix, repo_root)
    finally:
        sys.path.pop(0)
    return {
        suite.identifier: (
            suite.runnable,
            suite.owner.relative_to(repo_root).as_posix()
            if suite.runnable
            else str(suite.blocked_reason),
        )
        for suite in catalog.suites
    }


def classify_row(
    legacy: dict[str, Any],
    unit: dict[str, str],
    manifests: dict[str, Path],
    specialized: dict[str, tuple[bool, str]],
) -> ParityRow:
    """Map one historical ID without treating a transitional route as final migration."""

    identifier = str(legacy["ctest_id"])
    paths: list[ExecutionPath] = []
    if identifier in unit:
        paths.append(ExecutionPath("unit", identifier, True, unit[identifier]))
    if identifier in manifests:
        paths.append(ExecutionPath("manifest", identifier, True, manifests[identifier].as_posix()))
    if identifier in specialized:
        runnable, detail = specialized[identifier]
        paths.append(ExecutionPath("specialized", identifier, runnable, detail))
    runnable_paths = [path for path in paths if path.runnable]
    execution_state = _execution_state(paths, runnable_paths)
    destination = str(legacy.get("destination", ""))
    expected_layer = DESTINATION_LAYERS.get(destination, "unknown")
    migration_state = _migration_state(paths, expected_layer)
    signals = legacy.get("expectation_signals", [])
    normalized_signals = tuple(str(value) for value in signals) if isinstance(signals, list) else ()
    return ParityRow(
        ctest_id=identifier,
        destination=destination,
        expected_layer=expected_layer,
        primary_component=str(legacy.get("primary_component", "")),
        owner_path=str(legacy.get("owner_path", "")),
        expectation_contract=str(legacy.get("expectation_contract", "")),
        expectation_signals=normalized_signals,
        execution_state=execution_state,
        migration_state=migration_state,
        paths=tuple(paths),
    )


def _execution_state(paths: list[ExecutionPath], runnable: list[ExecutionPath]) -> str:
    """Classify whether exactly one concrete execution route is currently usable."""

    if not paths:
        return "UNMAPPED"
    if len(paths) > 1:
        return "AMBIGUOUS_MULTIPLE_PATHS"
    if runnable:
        return "RUNNABLE"
    return "REGISTERED_BUT_BLOCKED"


def _migration_state(paths: list[ExecutionPath], expected_layer: str) -> str:
    """Classify final-destination parity independently from current runnability."""

    if len(paths) > 1:
        return "TARGET_AMBIGUOUS_MULTIPLE_PATHS"
    matching = [path for path in paths if path.layer == expected_layer]
    if len(matching) == 1 and matching[0].runnable:
        return "TARGET_RUNNABLE"
    if len(matching) == 1:
        return "TARGET_REGISTERED_BUT_BLOCKED"
    if paths:
        return "TRANSITIONAL_WRONG_LAYER"
    return "TARGET_UNMAPPED"


def summarize(rows: list[ParityRow]) -> dict[str, int]:
    """Compute closure counts without inferring success from aggregate totals."""

    return {
        "legacy_total": len(rows),
        "runnable": sum(row.execution_state == "RUNNABLE" for row in rows),
        "blocked": sum(row.execution_state == "REGISTERED_BUT_BLOCKED" for row in rows),
        "ambiguous": sum(row.execution_state == "AMBIGUOUS_MULTIPLE_PATHS" for row in rows),
        "unmapped": sum(row.execution_state == "UNMAPPED" for row in rows),
        "target_runnable": sum(row.migration_state == "TARGET_RUNNABLE" for row in rows),
        "target_blocked": sum(
            row.migration_state == "TARGET_REGISTERED_BUT_BLOCKED" for row in rows
        ),
        "target_ambiguous": sum(
            row.migration_state == "TARGET_AMBIGUOUS_MULTIPLE_PATHS" for row in rows
        ),
        "transitional_wrong_layer": sum(
            row.migration_state == "TRANSITIONAL_WRONG_LAYER" for row in rows
        ),
        "target_unmapped": sum(row.migration_state == "TARGET_UNMAPPED" for row in rows),
    }


def report_payload(
    rows: list[ParityRow],
    matrix: Path,
    unit_executable: Path,
    unit_count: int,
    manifest_count: int,
    specialized_count: int,
) -> dict[str, Any]:
    """Build the machine-readable evidence envelope for one audit."""

    summary = summarize(rows)
    summary["gate_passed"] = int(
        summary["runnable"] == summary["legacy_total"]
        and summary["target_runnable"] == summary["legacy_total"]
        and summary["ambiguous"] == 0
    )
    return {
        "schema": 1,
        "source_matrix": matrix.as_posix(),
        "source_matrix_sha256": sha256_file(matrix),
        "unit_executable": unit_executable.as_posix(),
        "unit_executable_sha256": sha256_file(unit_executable),
        "catalog_counts": {
            "unit": unit_count,
            "manifest": manifest_count,
            "specialized": specialized_count,
        },
        "summary": summary,
        "tests": [_row_dict(row) for row in rows],
    }


def _row_dict(row: ParityRow) -> dict[str, Any]:
    """Serialize one immutable row while retaining all observed execution paths."""

    value = asdict(row)
    value["expectation_signals"] = list(row.expectation_signals)
    value["paths"] = [asdict(path) for path in row.paths]
    return value


def write_json(path: Path, payload: dict[str, Any]) -> None:
    """Write deterministic JSON evidence with one trailing newline."""

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_csv(path: Path, rows: list[ParityRow]) -> None:
    """Write a reviewable one-row-per-historical-test matrix."""

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(
            (
                "ctest_id",
                "component",
                "destination",
                "expected_layer",
                "execution_state",
                "migration_state",
                "execution_paths",
                "owner_path",
                "expectation_contract",
                "expectation_signals",
            )
        )
        for row in rows:
            paths = ";".join(
                f"{path.layer}:{path.identifier}:{'runnable' if path.runnable else 'blocked'}"
                for path in row.paths
            )
            writer.writerow(
                (
                    row.ctest_id,
                    row.primary_component,
                    row.destination,
                    row.expected_layer,
                    row.execution_state,
                    row.migration_state,
                    paths,
                    row.owner_path,
                    row.expectation_contract,
                    ";".join(row.expectation_signals),
                )
            )


def write_markdown(path: Path, payload: dict[str, Any]) -> None:
    """Write the human gate report and enumerate every non-closed historical ID."""

    summary = payload["summary"]
    lines = [
        "# Auditoría uno-a-uno de ejecución de pruebas históricas",
        "",
        "## Resultado",
        "",
        f"- Inventario histórico: **{summary['legacy_total']}**.",
        f"- Con ruta ejecutable única: **{summary['runnable']}**.",
        f"- Registradas pero bloqueadas: **{summary['blocked']}**.",
        f"- Con más de una ruta: **{summary['ambiguous']}**.",
        f"- Sin ruta ejecutable: **{summary['unmapped']}**.",
        f"- Con destino final correcto y ejecutable: **{summary['target_runnable']}**.",
        f"- En capa transitoria incorrecta: **{summary['transitional_wrong_layer']}**.",
        f"- Gate de paridad: **{'PASSED' if summary['gate_passed'] else 'FAILED'}**.",
        "",
        "## Filas no cerradas",
        "",
        "| ID histórico | Ejecución | Migración | Destino | Owner |",
        "|---|---|---|---|---|",
    ]
    for row in payload["tests"]:
        if row["execution_state"] == "RUNNABLE" and row["migration_state"] == "TARGET_RUNNABLE":
            continue
        lines.append(
            "| `{}` | {} | {} | {} | `{}` |".format(
                row["ctest_id"],
                row["execution_state"],
                row["migration_state"],
                row["expected_layer"],
                row["owner_path"],
            )
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_arguments() -> argparse.Namespace:
    """Parse explicit evidence inputs and outputs without hidden tool discovery."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument(
        "--matrix",
        type=Path,
        default=Path("docs/compiler-optimization/test-infrastructure/test-destination-matrix.json"),
    )
    parser.add_argument(
        "--unit-executable",
        type=Path,
        default=Path("build-test-v2/tests/nsc_unit_tests.exe"),
    )
    parser.add_argument(
        "--json-output",
        type=Path,
        default=Path("docs/compiler-optimization/test-infrastructure/test-execution-parity.json"),
    )
    parser.add_argument(
        "--csv-output",
        type=Path,
        default=Path("docs/compiler-optimization/test-infrastructure/test-execution-parity.csv"),
    )
    parser.add_argument(
        "--markdown-output",
        type=Path,
        default=Path("docs/compiler-optimization/test-infrastructure/test-execution-parity.md"),
    )
    parser.add_argument("--allow-incomplete", action="store_true")
    return parser.parse_args()


def resolve_inside(root: Path, value: Path) -> Path:
    """Resolve a relative CLI path against the audited repository."""

    return value.resolve() if value.is_absolute() else (root / value).resolve()


def main() -> int:
    """Generate all parity artifacts and fail unless every historical test is closed."""

    options = parse_arguments()
    repo_root = options.repo_root.resolve()
    matrix = resolve_inside(repo_root, options.matrix)
    unit_executable = resolve_inside(repo_root, options.unit_executable)
    legacy = load_legacy_rows(matrix)
    unit = run_unit_catalog(unit_executable)
    manifests = load_manifest_catalog(repo_root / "tests" / "cases")
    specialized = load_specialized_catalog(matrix, repo_root)
    rows = [classify_row(row, unit, manifests, specialized) for row in legacy]
    payload = report_payload(
        rows,
        matrix,
        unit_executable,
        len(unit),
        len(manifests),
        len(specialized),
    )
    write_json(resolve_inside(repo_root, options.json_output), payload)
    write_csv(resolve_inside(repo_root, options.csv_output), rows)
    write_markdown(resolve_inside(repo_root, options.markdown_output), payload)
    summary = payload["summary"]
    print(
        "parity: "
        f"historical={summary['legacy_total']} runnable={summary['runnable']} "
        f"target={summary['target_runnable']} blocked={summary['blocked']} "
        f"unmapped={summary['unmapped']}"
    )
    if summary["gate_passed"] or options.allow_incomplete:
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
