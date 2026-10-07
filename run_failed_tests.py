#!/usr/bin/env python3
"""Run the exact CTest names listed in test.run and preserve complete evidence."""

from __future__ import annotations

import argparse
import json
import locale
import os
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import TextIO


ROOT = Path(__file__).resolve().parent
DEFAULT_MANIFEST = ROOT / "test.run"
DEFAULT_BUILD = ROOT / "build-test-nslink"
DEFAULT_HISTORY = ROOT / "_nscbuilder" / "failed-tests"
DEFAULT_NSC = Path(r"C:\native-sharp\bin\nsc.exe")
DEFAULT_NSLINK = Path(r"C:\native-sharp\bin\nslink.exe")
CTEST_FILE = "CTestTestfile.cmake"
PATH_TOOL_PATTERN = re.compile(
    r"(?i)[A-Za-z]:[/\\][^\"\r\n]*[/\\]"
    r"(cmake|ctest|ninja|python|clang-cl|clang\+\+|clang|lld-link|llvm-[A-Za-z0-9_-]+)\.exe"
)
NSC_PATH_PATTERN = re.compile(r"(?i)[A-Za-z]:[/\\][^\"\r\n]*[/\\]nsc\.exe")
NSLINK_PATH_PATTERN = re.compile(r"(?i)[A-Za-z]:[/\\][^\"\r\n]*[/\\]nslink\.exe")
SOURCE_ERROR = re.compile(
    r"^(?:[A-Za-z]:)?[^\r\n]+?(?:\(\d+(?:,\d+)?\)|:\d+:\d+):\s*"
    r"(?:fatal\s+)?error(?:\[[^]]+\])?:\s*.+$",
    re.IGNORECASE,
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Ejecuta únicamente las CTests listadas en test.run.",
    )
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD)
    parser.add_argument("--tests-file", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_HISTORY)
    parser.add_argument("--nsc", type=Path, default=DEFAULT_NSC)
    parser.add_argument("--nslink", type=Path, default=DEFAULT_NSLINK)
    parser.add_argument("--list", action="store_true", help="Muestra la selección y no ejecuta.")
    return parser.parse_args()


def read_test_manifest(path: Path) -> list[str]:
    if not path.is_file():
        raise ValueError(f"No existe el archivo de pruebas: {path}")
    names: list[str] = []
    seen: set[str] = set()
    for number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if any(character.isspace() for character in line):
            raise ValueError(f"{path}:{number}: el nombre CTest contiene espacios: {line!r}")
        if line not in seen:
            names.append(line)
            seen.add(line)
    return names


def safe_name(value: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("._")
    return normalized or "test"


def write_both(master: TextIO, per_test: TextIO, text: str) -> None:
    print(text, end="", flush=True)
    master.write(text)
    master.flush()
    per_test.write(text)
    per_test.flush()


def run_one_test(
    build_dir: Path,
    name: str,
    master: TextIO,
    log_path: Path,
    environment: dict[str, str],
) -> dict:
    command = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "--interactive-debug-mode",
        "0",
        "--output-on-failure",
        "--no-tests=error",
        "--verbose",
        "-R",
        f"^{re.escape(name)}$",
    ]
    started = time.monotonic()
    output: list[str] = []
    encoding = locale.getpreferredencoding(False) or "utf-8"
    with log_path.open("w", encoding="utf-8", errors="replace") as per_test:
        header = f"\n{'=' * 88}\nPRUEBA: {name}\nCOMANDO: {subprocess.list2cmdline(command)}\n"
        write_both(master, per_test, header)
        process = subprocess.Popen(
            command,
            cwd=str(ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding=encoding,
            errors="replace",
            bufsize=1,
            env=environment,
        )
        assert process.stdout is not None
        try:
            for line in process.stdout:
                output.append(line)
                write_both(master, per_test, line)
            exit_code = process.wait()
        except KeyboardInterrupt:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            raise
        seconds = time.monotonic() - started
        footer = f"RESULTADO: exit={exit_code}; tiempo={seconds:.3f} s\n"
        write_both(master, per_test, footer)
    return {
        "name": name,
        "status": "passed" if exit_code == 0 else "failed",
        "exit_code": exit_code,
        "seconds": round(seconds, 3),
        "log": str(log_path.resolve()),
        "output": "".join(output),
    }


def extract_diagnostics(output: str) -> list[str]:
    diagnostics: list[str] = []
    capture_cmake_detail = False
    for raw_line in output.splitlines():
        line = re.sub(r"^\d+:\s*", "", raw_line.strip())
        if not line:
            continue
        actionable = (
            SOURCE_ERROR.match(line) is not None
            or line.startswith(("FAIL:", "AssertionError:", "CMake Error at "))
            or "nsc: error:" in line
            or " error[" in line
            or line.startswith("error:")
        )
        if actionable and line not in diagnostics:
            diagnostics.append(line)
            capture_cmake_detail = line.startswith("CMake Error at ")
        elif capture_cmake_detail and not line.startswith("Call Stack"):
            diagnostics.append(line)
            capture_cmake_detail = False
        if len(diagnostics) == 12:
            break
    return diagnostics or ["El proceso falló sin un diagnóstico accionable en su salida."]


def markdown_cell(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\r", " ").replace("\n", "<br>")


def public_result(result: dict) -> dict:
    value = {key: item for key, item in result.items() if key != "output"}
    value["diagnostics"] = (
        ["Sin errores."]
        if result.get("status") == "passed"
        else extract_diagnostics(str(result.get("output", "")))
    )
    return value


def write_reports(
    run_dir: Path,
    tests_file: Path,
    nsc: Path,
    nslink: Path,
    requested: int,
    results: list[dict],
    interrupted: bool,
) -> None:
    public = [public_result(result) for result in results]
    failed = [result for result in public if result["status"] != "passed"]
    payload = {
        "schema": "nativesharp-failed-ctest-run",
        "version": 1,
        "tests_file": str(tests_file.resolve()),
        "nsc": str(nsc),
        "nslink": str(nslink),
        "requested": requested,
        "executed": len(results),
        "passed": len(public) - len(failed),
        "failed": len(failed),
        "interrupted": interrupted,
        "results": public,
    }
    (run_dir / "summary.json").write_text(
        json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    lines = [
        "# Ejecución dirigida de CTests fallidas",
        "",
        f"- NSC: `{nsc}`",
        f"- NSLink: `{nslink}`",
        "- CTest y herramientas auxiliares: resueltos mediante PATH.",
        f"- Solicitadas: {requested}",
        f"- Ejecutadas: {len(results)}",
        f"- Aprobadas: {len(public) - len(failed)}",
        f"- Fallidas: {len(failed)}",
        f"- Interrumpida: {'sí' if interrupted else 'no'}",
        "",
        "| Prueba | Estado | Exit | Tiempo | Diagnóstico principal | Log |",
        "|---|---:|---:|---:|---|---|",
    ]
    for result in public:
        diagnostic = result["diagnostics"][0]
        lines.append(
            f"| {markdown_cell(result['name'])} | {result['status']} | "
            f"{result['exit_code']} | {result['seconds']:.3f} s | "
            f"{markdown_cell(diagnostic)} | `{result['log']}` |"
        )
    lines.extend(("", "## Diagnósticos completos por prueba", ""))
    for result in failed:
        lines.append(f"### {result['name']}")
        lines.append("")
        lines.extend(f"- {diagnostic}" for diagnostic in result["diagnostics"])
        lines.extend(("", f"Log: `{result['log']}`", ""))
    (run_dir / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def path_tool_name(match: re.Match[str]) -> str:
    """Return only the executable name so Windows resolves the auxiliary tool through PATH."""
    return f"{match.group(1)}.exe"


def prepare_ctest_overlay(
    build_dir: Path,
    run_dir: Path,
    nsc: Path,
    nslink: Path,
) -> Path:
    """Copy only CTest metadata and redirect tool references without touching the built tree."""
    source = build_dir / CTEST_FILE
    if not source.is_file():
        raise ValueError(f"El árbol CTest ya construido no existe: {build_dir}")
    text = source.read_text(encoding="utf-8", errors="replace")
    text = NSC_PATH_PATTERN.sub(nsc.as_posix(), text)
    text = NSLINK_PATH_PATTERN.sub(nslink.as_posix(), text)
    text = PATH_TOOL_PATTERN.sub(path_tool_name, text)
    overlay = run_dir / "ctest"
    overlay.mkdir()
    (overlay / CTEST_FILE).write_text(text, encoding="utf-8")
    return overlay


def test_environment(nsc: Path, nslink: Path) -> dict[str, str]:
    """Expose only the two explicit NativeSharp tool paths; every other tool remains PATH-owned."""
    environment = dict(os.environ)
    environment["NSC_EXE"] = str(nsc)
    environment["NSC_LINKER"] = str(nslink)
    return environment


def execute(args: argparse.Namespace, names: list[str]) -> int:
    build_dir = args.build_dir.resolve()
    nsc = args.nsc.resolve()
    nslink = args.nslink.resolve()
    if not nsc.is_file():
        raise ValueError(f"No existe el compilador oficial: {nsc}")
    if not nslink.is_file():
        raise ValueError(f"No existe el linker oficial: {nslink}")
    run_id = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    run_dir = args.output_root.resolve() / run_id
    run_dir.mkdir(parents=True, exist_ok=False)
    logs_dir = run_dir / "tests"
    logs_dir.mkdir()
    test_dir = prepare_ctest_overlay(build_dir, run_dir, nsc, nslink)
    environment = test_environment(nsc, nslink)
    results: list[dict] = []
    interrupted = False
    master_path = run_dir / "full.log"
    with master_path.open("w", encoding="utf-8", errors="replace") as master:
        for index, name in enumerate(names, 1):
            print(f"\n[{index}/{len(names)}] {name}")
            try:
                result = run_one_test(
                    test_dir,
                    name,
                    master,
                    logs_dir / f"{index:03d}-{safe_name(name)}.log",
                    environment,
                )
            except KeyboardInterrupt:
                interrupted = True
                break
            results.append(result)
    write_reports(run_dir, args.tests_file, nsc, nslink, len(names), results, interrupted)
    print(f"\nLog completo : {master_path}")
    print(f"Resumen       : {run_dir / 'summary.md'}")
    print(f"Datos         : {run_dir / 'summary.json'}")
    failed = sum(result["status"] != "passed" for result in results)
    return 130 if interrupted else (1 if failed else 0)


def main() -> int:
    args = parse_arguments()
    try:
        names = read_test_manifest(args.tests_file.resolve())
        if args.list:
            print("\n".join(names))
            return 0
        if not names:
            print(f"{args.tests_file.resolve()} no contiene CTests fallidas por ejecutar.")
            return 0
        return execute(args, names)
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f"run_failed_tests: error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
