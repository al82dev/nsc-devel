from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path


EXPECTED_IDS = tuple(f"TSH-{value:05d}" for value in range(399, 452))
KNOWN_PRODUCT_DEFECTS = frozenset({"TSH-00442"})
PROFILE_RANK = {"PR": 0, "NIGHTLY": 1}


def read_jsonl(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for number, raw in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        value = json.loads(raw)
        if not isinstance(value, dict):
            raise RuntimeError(f"manifest row {path}:{number} is not an object")
        rows.append(value)
    return rows


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8", newline="\n")


def run_process(command: list[str], cwd: Path, environment: dict[str, str], timeout: float) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, env=environment, text=True, encoding="utf-8",
                          errors="replace", capture_output=True, timeout=timeout, check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nsc-executable", type=Path, required=True)
    parser.add_argument("--nslink-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--optimization", choices=("O0", "O1", "O2", "O3"), required=True)
    parser.add_argument("--profile", choices=("PR", "NIGHTLY"), default="NIGHTLY")
    options = parser.parse_args()

    nsc = options.nsc_executable.resolve()
    nslink = options.nslink_executable.resolve()
    if not nsc.is_file() or not nslink.is_file():
        raise RuntimeError("SelfHost.Lexer requires explicit NSC and NSLink executables")

    root = options.repository_root.resolve()
    if not (root / "docs" / "compiler-testing-reengineering" / "inventory" / "test-case-design.csv").is_file():
        raise RuntimeError("SelfHost.Lexer canonical test-case inventory is missing")
    manifest = options.source_root / "manifest" / "cases.jsonl"
    cases = [row for row in read_jsonl(manifest) if row.get("component") == "SelfHost.Lexer"]
    if tuple(str(row["case_id"]) for row in cases) != EXPECTED_IDS:
        raise RuntimeError("central manifest does not contain canonical TSH-00399..TSH-00451")
    if len({str(row["case_id"]) for row in cases}) != 53:
        raise RuntimeError("central SelfHost.Lexer rows contain duplicate identifiers")
    known = {str(row["case_id"]) for row in cases if row.get("expected_outcome") == "KNOWN_FAILURE"}
    if known != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("SelfHost.Lexer known-product-defect classification changed")

    selected = [row for row in cases if PROFILE_RANK[str(row["profile"])] <= PROFILE_RANK[options.profile]]
    work_root = options.work_parent.resolve() / f"selfhost-lexer-{options.build_mode.lower()}-{options.optimization.lower()}"
    work_root.mkdir(parents=True, exist_ok=True)
    executable = work_root / "selfhost-lexer-tests.exe"
    project = options.source_root / "selfhost" / "modules" / "SelfHostLexer.nsproj"
    environment = dict(os.environ)
    environment["NSC_LINKER"] = str(nslink)
    environment["PATH"] = str(nslink.parent) + os.pathsep + environment.get("PATH", "")
    command = [str(nsc), "--emit-project-oop-exe", str(project), f"-{options.optimization}",
               f"--profile={options.build_mode.lower()}", "-g0", "-o", str(executable)]
    write_text(work_root / "compile.command.json", json.dumps(command, indent=2) + "\n")
    compiled = run_process(command, work_root, environment, 900.0)
    write_text(work_root / "compile.stdout.log", compiled.stdout)
    write_text(work_root / "compile.stderr.log", compiled.stderr)
    if compiled.returncode != 0 or not executable.is_file():
        raise RuntimeError(f"SelfHost.Lexer fixture compilation failed with {compiled.returncode}")

    failures = 0
    results: list[dict[str, object]] = []
    for row in selected:
        case_id = str(row["case_id"])
        started = time.monotonic()
        completed = run_process([str(executable), case_id], work_root, environment, 180.0)
        duration = time.monotonic() - started
        case_root = work_root / "cases" / case_id
        write_text(case_root / "stdout.log", completed.stdout)
        write_text(case_root / "stderr.log", completed.stderr)
        passed = completed.returncode == int(row["oracle"]["exit_code"])
        if case_id in KNOWN_PRODUCT_DEFECTS:
            status = "UNEXPECTED_PASS" if passed else "KNOWN_PRODUCT_DEFECT"
            failures += 1
        else:
            status = "PASS" if passed else "FAIL"
            failures += int(not passed)
        results.append({"case_id": case_id, "contract_id": row["contract_id"], "status": status,
                        "exit_code": completed.returncode, "duration_seconds": duration})

    payload = {"component": "SelfHost.Lexer", "profile": options.profile, "selected": len(selected),
               "required": len(selected) - sum(str(row["case_id"]) in KNOWN_PRODUCT_DEFECTS for row in selected),
               "known_product_defects": sum(str(row["case_id"]) in KNOWN_PRODUCT_DEFECTS for row in selected),
               "failures": failures, "results": results}
    write_text(work_root / "results.json", json.dumps(payload, indent=2) + "\n")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"selfhost_lexer_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
