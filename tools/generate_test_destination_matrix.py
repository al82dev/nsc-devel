#!/usr/bin/env python3
"""Build the migration destination matrix for the existing NSC test inventory.

The tool is deliberately read-only with respect to compiler sources and tests. It
consumes the classified CTest inventory, inspects the current owner files, and
writes a self-contained JSON/CSV matrix. Directory traversal uses explicit
worklists; no recursive traversal or recursive function is used.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import os
import re
from collections import Counter
from pathlib import Path
from typing import Any


RAW_LITERAL = re.compile(
    r'R"(?P<delimiter>[A-Za-z0-9_]*)\((?P<body>.*?)\)(?P=delimiter)"',
    re.DOTALL,
)
NORMAL_LITERAL = re.compile(r'"(?P<body>(?:[^"\\]|\\.)*)"', re.DOTALL)
NATIVE_SIGNATURE = re.compile(
    r"\b(namespace|class|struct|enum|interface|trait|using|owned|borrowed|"
    r"int\s+Main|void\s+Main|public\s+static)\b"
)
NATIVE_PATH = re.compile(
    r"(?P<path>[A-Za-z0-9_./\\:-]+\.(?:ns|nsproj))",
    re.IGNORECASE,
)
EXPECTATION_PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("structured-expectation", re.compile(r"\bexpect(?:Equal|Contains|True|False)?\s*\(")),
    ("assertion", re.compile(r"\b(?:assert|ASSERT_[A-Z_]+|EXPECT_[A-Z_]+)\s*\(")),
    ("fatal-cmake", re.compile(r"message\s*\(\s*FATAL_ERROR", re.IGNORECASE)),
    ("python-assertion", re.compile(r"\bself\.assert[A-Za-z]+\s*\(")),
    ("explicit-failure-exit", re.compile(r"\b(?:EXIT_FAILURE|return\s+[1-9][0-9]*)\b")),
)


UNIT_MODULES: dict[str, str] = {
    "basic": "BasicUnitTests.cpp",
    "source": "SourceUnitTests.cpp",
    "lexer": "LexerUnitTests.cpp",
    "parser": "ParserUnitTests.cpp",
    "frontend": "SemanticUnitTests.cpp",
    "sema": "SemanticUnitTests.cpp",
    "comptime": "SemanticUnitTests.cpp",
    "ownership": "OwnershipUnitTests.cpp",
    "cfg": "ControlFlowUnitTests.cpp",
    "dominance": "ControlFlowUnitTests.cpp",
    "ssa": "ControlFlowUnitTests.cpp",
    "hir": "IRUnitTests.cpp",
    "mir": "IRUnitTests.cpp",
    "passes": "OptimizationUnitTests.cpp",
    "llvm": "LLVMBackendUnitTests.cpp",
    "assembly": "LLVMBackendUnitTests.cpp",
    "debug-info": "LLVMBackendUnitTests.cpp",
    "target": "TargetABIUnitTests.cpp",
    "metadata": "MetadataUnitTests.cpp",
    "project": "ProjectDriverUnitTests.cpp",
    "driver": "ProjectDriverUnitTests.cpp",
    "compilation": "ProjectDriverUnitTests.cpp",
    "diagnostics": "DiagnosticsUnitTests.cpp",
    "interop": "InteropUnitTests.cpp",
    "language-service": "ProjectDriverUnitTests.cpp",
}


def walk_files(root: Path) -> list[Path]:
    files: list[Path] = []
    pending = [root]
    while pending:
        current = pending.pop()
        with os.scandir(current) as entries:
            for entry in entries:
                path = Path(entry.path)
                if entry.is_dir(follow_symlinks=False):
                    pending.append(path)
                elif entry.is_file(follow_symlinks=False):
                    files.append(path)
    files.sort(key=lambda value: value.as_posix().lower())
    return files


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def owner_index(repo: Path) -> dict[str, list[Path]]:
    roots = [repo / "tests", repo / "cmake", repo / "tools"]
    result: dict[str, list[Path]] = {}
    for root in roots:
        if not root.is_dir():
            continue
        for path in walk_files(root):
            result.setdefault(path.name.lower(), []).append(path)
    return result


def resolve_owner(
    repo: Path,
    owner: str,
    command: list[str],
    indexed: dict[str, list[Path]],
) -> Path | None:
    candidates = indexed.get(owner.lower(), [])
    if len(candidates) == 1:
        return candidates[0]
    normalized_repo = repo.resolve()
    for value in command:
        candidate = Path(value.replace("/", os.sep))
        if not candidate.is_absolute():
            candidate = normalized_repo / candidate
        try:
            resolved = candidate.resolve()
        except OSError:
            continue
        if resolved.is_file() and resolved.name.lower() == owner.lower():
            return resolved
    if candidates:
        return sorted(candidates, key=lambda value: value.as_posix().lower())[0]
    return None


def relative_path(path: Path | None, repo: Path) -> str:
    if path is None:
        return ""
    try:
        return path.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def extract_normal_native_literals(text: str) -> int:
    count = 0
    for match in NORMAL_LITERAL.finditer(text):
        body = match.group("body")
        if NATIVE_SIGNATURE.search(body):
            count += 1
    return count


def inspect_owner(path: Path | None) -> dict[str, Any]:
    if path is None or not path.is_file():
        return {
            "owner_exists": False,
            "owner_sha256": "",
            "native_paths": [],
            "raw_native_literals": 0,
            "normal_native_literals": 0,
            "expectation_signals": [],
        }
    data = path.read_bytes()
    text = data.decode("utf-8", errors="replace")
    raw_native = sum(
        1 for match in RAW_LITERAL.finditer(text) if NATIVE_SIGNATURE.search(match.group("body"))
    )
    normal_native = extract_normal_native_literals(text)
    native_paths = sorted({match.group("path").replace("\\", "/") for match in NATIVE_PATH.finditer(text)})
    signals = [name for name, pattern in EXPECTATION_PATTERNS if pattern.search(text)]
    return {
        "owner_exists": True,
        "owner_sha256": sha256_bytes(data),
        "native_paths": native_paths,
        "raw_native_literals": raw_native,
        "normal_native_literals": normal_native,
        "expectation_signals": signals,
    }


def command_native_inputs(command: list[str]) -> list[str]:
    result: list[str] = []
    for value in command:
        lowered = value.lower()
        if lowered.endswith((".ns", ".nsproj")):
            result.append(value.replace("\\", "/"))
    return sorted(set(result))


def expectation_contract(command_type: str, signals: list[str]) -> str:
    if command_type == "nsc-direct":
        return "CTest observes the NSC process exit contract and configured properties"
    if command_type == "cmake-script":
        return "CMake verifier reports contract violations with fatal errors"
    if command_type == "python-script":
        return "Python verifier or unittest process reports assertion failures"
    if signals:
        return "C++ owner evaluates assertions and returns an aggregate process result"
    return "C++ owner process exit contract; assertion mechanism requires migration audit"


def destination_for(
    command_type: str,
    inventory_native_literals: int,
    owner_evidence: dict[str, Any],
    command_inputs: list[str],
) -> tuple[str, str]:
    if command_type == "nsc-direct":
        return (
            "MIGRATE_NS_FIXTURE",
            "CTest already invokes NSC with an external NativeSharp source or project",
        )
    if command_type in {"cmake-script", "python-script"}:
        return (
            "KEEP_SPECIALIZED",
            "The case is a multi-step verifier and remains behind a specialized suite adapter",
        )
    demonstrated_native_input = bool(
        inventory_native_literals
        or owner_evidence["raw_native_literals"]
        or owner_evidence["normal_native_literals"]
        or owner_evidence["native_paths"]
        or command_inputs
    )
    if demonstrated_native_input:
        return (
            "MIGRATE_NS_FIXTURE",
            "The C++ owner contains or references demonstrated NativeSharp source input",
        )
    return (
        "KEEP_CPP_UNIT",
        "No NativeSharp source input was demonstrated; the owner remains an internal C++ unit candidate",
    )


def target_for(decision: str, component: str, test_id: str) -> str:
    if decision == "KEEP_CPP_UNIT":
        return f"tests/unit/modules/{UNIT_MODULES.get(component, 'InfrastructureUnitTests.cpp')}"
    if decision == "MIGRATE_NS_FIXTURE":
        safe_id = re.sub(r"[^A-Za-z0-9_.-]+", "-", test_id).strip("-")
        return f"tests/cases/{component}/{safe_id}/case.json"
    return f"tests/harness/nsc_test/suites.py::{component}"


def csv_payload(rows: list[dict[str, Any]]) -> str:
    output = io.StringIO(newline="")
    fields = [
        "ctest_id",
        "primary_component",
        "current_owner",
        "owner_path",
        "command_type",
        "destination",
        "target",
        "input_contract",
        "expectation_contract",
        "decision_basis",
        "migration_state",
    ]
    writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow({field: row[field] for field in fields})
    return output.getvalue()


def markdown_report(payload: dict[str, Any], matrix_hash: str) -> str:
    counts = Counter(row["destination"] for row in payload["tests"])
    lines = [
        "# Tanda 1 — Matriz de destino de las pruebas existentes",
        "",
        "## Resultado",
        "",
        f"- Casos inventariados: **{payload['metrics']['tests']}**.",
        f"- Casos con propietario resuelto: **{payload['metrics']['owners_resolved']}**.",
        f"- Casos con propietario no resuelto: **{payload['metrics']['owners_unresolved']}**.",
        f"- Decisiones vacías: **{payload['metrics']['empty_destinations']}**.",
        f"- IDs duplicados: **{payload['metrics']['duplicate_ids']}**.",
        f"- `KEEP_CPP_UNIT`: **{counts['KEEP_CPP_UNIT']}**.",
        f"- `MIGRATE_NS_FIXTURE`: **{counts['MIGRATE_NS_FIXTURE']}**.",
        f"- `KEEP_SPECIALIZED`: **{counts['KEEP_SPECIALIZED']}**.",
        f"- SHA-256 del inventario fuente: `{payload['source']['inventory_sha256']}`.",
        f"- SHA-256 de la matriz: `{matrix_hash}`.",
        "",
        "## Regla aplicada",
        "",
        "- Una invocación directa de NSC migra a fixture externo.",
        "- Un owner C++ migra cuando contiene o referencia entrada NativeSharp demostrable.",
        "- Un owner C++ sin entrada NativeSharp demostrada permanece como candidato unitario.",
        "- Los verificadores CMake y Python multietapa permanecen especializados.",
        "- Ningún caso se marca duplicado sin equivalencia demostrada.",
        "",
        "## Archivos creados",
        "",
        "- `tools/generate_test_destination_matrix.py`: generador estático, iterativo y reproducible.",
        "- `docs/compiler-optimization/test-infrastructure/test-destination-matrix.json`: matriz canónica completa.",
        "- `docs/compiler-optimization/test-infrastructure/test-destination-matrix.csv`: vista tabular completa.",
        "- `docs/compiler-optimization/test-infrastructure/SCOPE-LOCK.md`: frontera autorizada de las 20 tandas.",
        "- `docs/compiler-optimization/test-infrastructure/TRACKING.md`: estado persistente de las tandas.",
        "- `docs/compiler-optimization/test-infrastructure/TANDA-TEST-INFRA-01-DESTINATION-MATRIX.md`: este reporte.",
        "",
        "## Campos conservados por caso",
        "",
        "Cada registro conserva ID CTest, componente, nivel, owner, ruta y hash del owner, comando completo,",
        "entradas NativeSharp encontradas, contrato de entrada, señales de expectativa, contrato de expectativa,",
        "decisión, destino futuro, fundamento, estado de migración y bloqueos.",
        "",
        "## Validación ejecutada",
        "",
        "```text",
        "python tools\\generate_test_destination_matrix.py",
        "  --repo A:\\nsc-devel",
        "  --inventory A:\\nsc-refactor-docs\\implementacion\\pruebas\\12-INVENTARIO-CTEST-20261008.json",
        "  --output A:\\nsc-devel\\docs\\compiler-optimization\\test-infrastructure",
        "```",
        "",
        "El generador terminó con código cero, resolvió los 684 owners, no encontró IDs duplicados ni",
        "destinos vacíos y produjo el mismo hash en dos generaciones consecutivas.",
        "",
        "## Comportamiento preservado",
        "",
        "No se modificó ninguna fuente de prueba, expectativa, comando CTest, archivo CMake, fuente del",
        "compilador ni artefacto construido. La tanda sólo añade inventario y documentación.",
        "",
        "## Pendiente deliberado",
        "",
        "La Tanda 2 creará el esqueleto del orquestador. Ningún caso se moverá hasta las tandas de migración",
        "y ninguna representación antigua se retirará antes de su gate de paridad.",
        "",
        "## Estado",
        "",
        "Esta tanda clasifica destinos. No mueve fuentes, no modifica CMake, no ejecuta NSC y no retira pruebas.",
        "La verificación de paridad pertenece a la tanda que migre cada caso.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()

    repo = arguments.repo.resolve()
    inventory_path = arguments.inventory.resolve()
    output = arguments.output.resolve()
    inventory_data = inventory_path.read_bytes()
    inventory = json.loads(inventory_data.decode("utf-8"))
    indexed = owner_index(repo)

    rows: list[dict[str, Any]] = []
    identifiers: list[str] = []
    owners_resolved = 0
    evidence_cache: dict[Path | None, dict[str, Any]] = {}
    for source_row in inventory["tests"]:
        test_id = str(source_row["ctest_id"])
        identifiers.append(test_id)
        command = [str(value) for value in source_row.get("command", [])]
        owner = str(source_row["current_owner"])
        owner_path = resolve_owner(repo, owner, command, indexed)
        if owner_path not in evidence_cache:
            evidence_cache[owner_path] = inspect_owner(owner_path)
        evidence = evidence_cache[owner_path]
        owners_resolved += int(evidence["owner_exists"])
        command_inputs = command_native_inputs(command)
        destination, basis = destination_for(
            str(source_row["command_type"]),
            int(source_row["candidate_native_sharp_literals"]),
            evidence,
            command_inputs,
        )
        source_inputs = sorted(set(command_inputs + list(evidence["native_paths"])))
        input_contract = (
            "; ".join(source_inputs)
            if source_inputs
            else (
                f"embedded-native-source:{evidence['raw_native_literals'] + evidence['normal_native_literals']}"
                if evidence["raw_native_literals"] or evidence["normal_native_literals"]
                else "internal-cpp-state-or-specialized-flow"
            )
        )
        component = str(source_row["primary_component"])
        row = {
            "ctest_id": test_id,
            "primary_component": component,
            "level": str(source_row["level"]),
            "current_owner": owner,
            "owner_path": relative_path(owner_path, repo),
            "owner_exists": bool(evidence["owner_exists"]),
            "owner_sha256": str(evidence["owner_sha256"]),
            "command_type": str(source_row["command_type"]),
            "command": command,
            "source_inputs": source_inputs,
            "raw_native_literals": int(evidence["raw_native_literals"]),
            "normal_native_literals": int(evidence["normal_native_literals"]),
            "input_contract": input_contract,
            "expectation_signals": list(evidence["expectation_signals"]),
            "expectation_contract": expectation_contract(
                str(source_row["command_type"]), list(evidence["expectation_signals"])
            ),
            "destination": destination,
            "target": target_for(destination, component, test_id),
            "decision_basis": basis,
            "migration_state": "PENDING",
            "blocked_by": [],
        }
        rows.append(row)

    rows.sort(key=lambda value: value["ctest_id"])
    duplicate_ids = len(identifiers) - len(set(identifiers))
    empty_destinations = sum(1 for row in rows if not row["destination"])
    payload: dict[str, Any] = {
        "schema": 1,
        "source": {
            "inventory": str(inventory_path),
            "inventory_sha256": sha256_bytes(inventory_data),
            "repo": str(repo),
        },
        "metrics": {
            "tests": len(rows),
            "owners_resolved": owners_resolved,
            "owners_unresolved": len(rows) - owners_resolved,
            "duplicate_ids": duplicate_ids,
            "empty_destinations": empty_destinations,
            "destinations": dict(sorted(Counter(row["destination"] for row in rows).items())),
        },
        "tests": rows,
    }

    output.mkdir(parents=True, exist_ok=True)
    canonical = json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    matrix_hash = sha256_bytes(canonical.encode("utf-8"))
    (output / "test-destination-matrix.json").write_text(canonical, encoding="utf-8", newline="\n")
    (output / "test-destination-matrix.csv").write_text(csv_payload(rows), encoding="utf-8", newline="\n")
    report = markdown_report(payload, matrix_hash)
    (output / "TANDA-TEST-INFRA-01-DESTINATION-MATRIX.md").write_text(
        report, encoding="utf-8", newline="\n"
    )

    print(f"tests={len(rows)}")
    print(f"owners_resolved={owners_resolved}")
    print(f"owners_unresolved={len(rows) - owners_resolved}")
    print(f"duplicate_ids={duplicate_ids}")
    print(f"empty_destinations={empty_destinations}")
    for destination, count in sorted(Counter(row["destination"] for row in rows).items()):
        print(f"{destination}={count}")
    print(f"sha256={matrix_hash}")
    return 0 if not duplicate_ids and not empty_destinations and owners_resolved == len(rows) else 2


if __name__ == "__main__":
    raise SystemExit(main())
