#!/usr/bin/env python3
"""Build NSC and run only SH41/CTest 181 with isolated evidence logs."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / "build-clang"
NSC = BUILD_DIR / "bin" / "nsc.exe"
NSLINK = Path(r"A:\nslink-devel\bin\release\nslink.exe")
LIB_ROOT = Path(r"A:\native-sharp-3.0-baseline\lib")
RUNTIME = ROOT / "build-test-nslink" / "generated" / "NativeSharp.Runtime.Win32.lib"
WRAPPER = ROOT / "cmake" / "VerifyNativeSharpSelfHostedGeneration.cmake"
TRACE_TAG = "NSC_SH41_181_DIAGNOSTIC"


class RunError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RunError(message)


def require_child(path: Path, parent: Path) -> Path:
    resolved = path.resolve()
    resolved.relative_to(parent.resolve())
    return resolved


def run_to_log(command: list[str], log_path: Path, *, environment: dict[str, str],
               show_output: bool) -> int:
    with log_path.open("w", encoding="utf-8", newline="\n") as log:
        log.write("$ " + subprocess.list2cmdline(command) + "\n")
        log.flush()
        process = subprocess.Popen(
            command, cwd=ROOT, env=environment, text=True, encoding="utf-8",
            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        require(process.stdout is not None, "subprocess output pipe was not created")
        for line in process.stdout:
            log.write(line)
            log.flush()
            if show_output:
                print(line, end="", flush=True)
        return process.wait()


def write_stream(log, stage: str, name: str, value: object) -> None:
    text = str(value or "")
    log.write(f"[{TRACE_TAG}][{stage}] {name}_begin\n")
    if text:
        log.write(text)
        if not text.endswith("\n"):
            log.write("\n")
    log.write(f"[{TRACE_TAG}][{stage}] {name}_end\n")


def append_report(log_path: Path, report_path: Path) -> None:
    if not report_path.is_file():
        return
    report = json.loads(report_path.read_text(encoding="utf-8"))
    with log_path.open("a", encoding="utf-8", newline="\n") as log:
        log.write(f"\n[{TRACE_TAG}][REPORT] path={report_path}\n")
        for profile, profile_record in report.get("profiles", {}).items():
            for generation in ("nsc2_audit", "nsc3_audit"):
                record = profile_record.get(generation, {})
                stage = f"{profile}_{generation.upper()}"
                log.write(f"[{TRACE_TAG}][{stage}] command=" +
                          subprocess.list2cmdline(record.get("command", [])) + "\n")
                log.write(f"[{TRACE_TAG}][{stage}] elapsed_seconds=" +
                          str(record.get("elapsed_seconds", "")) + "\n")
                write_stream(log, stage, "stdout", record.get("stdout", ""))
                write_stream(log, stage, "stderr", record.get("stderr", ""))
        log.write(f"[{TRACE_TAG}][REPORT] json_begin\n")
        log.write(json.dumps(report, indent=2, sort_keys=True) + "\n")
        log.write(f"[{TRACE_TAG}][REPORT] json_end\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run only NSC self-host test 181 (SH41), O0.")
    parser.add_argument("--label", required=True, choices=("current", "previous", "final"))
    parser.add_argument("--jobs", type=int, default=12)
    arguments = parser.parse_args()

    cmake = shutil.which("cmake")
    linker = shutil.which("lld-link")
    require(cmake is not None, "cmake was not found on PATH")
    require(linker is not None, "lld-link was not found on PATH")
    require(NSLINK.is_file(), f"NSLink does not exist: {NSLINK}")
    require((LIB_ROOT / "NativeSharpCore.lib").is_file(),
            f"SH41 library root is incomplete: {LIB_ROOT}")
    require(RUNTIME.is_file(), f"SH41 runtime does not exist: {RUNTIME}")
    require(WRAPPER.is_file(), f"SH41 wrapper does not exist: {WRAPPER}")

    build_log = ROOT / f"sh41-181-{arguments.label}-build.log"
    execution_log = ROOT / f"sh41-181-{arguments.label}.log"
    output_root = require_child(ROOT / "build-sh41-181", ROOT)
    output_dir = require_child(output_root / arguments.label, output_root)
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    environment = os.environ.copy()
    environment["NSC_SELFHOST_PROFILES"] = "O0"
    environment["NS_LIB_ROOT"] = str(LIB_ROOT)
    environment["NSC_LINKER"] = str(NSLINK)
    build_command = [cmake, "--build", str(BUILD_DIR), "--target", "nsc",
                     "--parallel", str(arguments.jobs), "--", "-k", "0"]
    print(f"[{TRACE_TAG}][BUILD] log={build_log}", flush=True)
    build_exit = run_to_log(build_command, build_log, environment=environment, show_output=False)
    require(build_exit == 0, f"NSC build failed ({build_exit}); see {build_log}")
    require(NSC.is_file(), f"built NSC does not exist: {NSC}")

    command = [
        cmake,
        f"-DNSC_EXECUTABLE={NSC}",
        f"-DPYTHON_EXECUTABLE={Path(sys.executable).resolve()}",
        f"-DSOURCE_ROOT={ROOT}",
        f"-DOUTPUT_DIR={output_dir}",
        f"-DRUNTIME_LIBRARY={RUNTIME}",
        f"-DLINKER_EXECUTABLE={Path(linker).resolve()}",
        f"-DNSLINK_EXECUTABLE={NSLINK}",
        "-P", str(WRAPPER),
    ]
    print(f"[{TRACE_TAG}][SH41] log={execution_log}", flush=True)
    execution_exit = run_to_log(command, execution_log, environment=environment, show_output=True)
    append_report(execution_log, output_dir / "sh41-report.json")
    print(f"[{TRACE_TAG}][SH41] exit={execution_exit} artifacts={output_dir}", flush=True)
    return execution_exit


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError, RunError) as error:
        print(f"run_sh41_181: error: {error}", file=sys.stderr)
        raise SystemExit(2)
