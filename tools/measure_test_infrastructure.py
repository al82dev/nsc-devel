#!/usr/bin/env python3
"""Measure NativeSharp test infrastructure without changing test semantics.

The tool has two deliberately separate operations:

* ``run`` observes one command and records wall time plus the Windows process
  tree's peak memory and process counts.
* ``snapshot`` inventories existing CTest/Ninja graphs, disk usage and harness
  reports without executing tests.

All graph and filesystem traversals are iterative.  The script never removes a
build tree and never selects a compiler or linker on behalf of the caller.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path
from typing import Iterable


SCHEMA = "nativesharp-test-infrastructure-metrics"
TH32CS_SNAPPROCESS = 0x00000002
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
PROCESS_VM_READ = 0x0010
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", wintypes.WCHAR * 260),
    ]


class PROCESS_MEMORY_COUNTERS_EX(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("PageFaultCount", wintypes.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


if os.name == "nt":
    _KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
    _PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
    _KERNEL32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    _KERNEL32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    _KERNEL32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    _KERNEL32.Process32FirstW.restype = wintypes.BOOL
    _KERNEL32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    _KERNEL32.Process32NextW.restype = wintypes.BOOL
    _KERNEL32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    _KERNEL32.OpenProcess.restype = wintypes.HANDLE
    _KERNEL32.CloseHandle.argtypes = [wintypes.HANDLE]
    _KERNEL32.CloseHandle.restype = wintypes.BOOL
    _PSAPI.GetProcessMemoryInfo.argtypes = [
        wintypes.HANDLE,
        ctypes.POINTER(PROCESS_MEMORY_COUNTERS_EX),
        wintypes.DWORD,
    ]
    _PSAPI.GetProcessMemoryInfo.restype = wintypes.BOOL
else:
    _KERNEL32 = None
    _PSAPI = None


def _windows_process_table() -> dict[int, int]:
    """Return ``pid -> parent pid`` from one ToolHelp snapshot."""

    if _KERNEL32 is None:
        return {}
    snapshot = _KERNEL32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        return {}
    table: dict[int, int] = {}
    entry = PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(entry)
    try:
        available = _KERNEL32.Process32FirstW(snapshot, ctypes.byref(entry))
        while available:
            table[int(entry.th32ProcessID)] = int(entry.th32ParentProcessID)
            available = _KERNEL32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        _KERNEL32.CloseHandle(snapshot)
    return table


def _descendants(root_pid: int, process_table: dict[int, int]) -> set[int]:
    """Expand a process tree iteratively from a single root PID."""

    selected = {root_pid}
    changed = True
    while changed:
        changed = False
        for pid, parent in process_table.items():
            if pid not in selected and parent in selected:
                selected.add(pid)
                changed = True
    return selected


def _process_memory(pid: int) -> tuple[int, int] | None:
    if _KERNEL32 is None or _PSAPI is None:
        return None
    handle = _KERNEL32.OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
        False,
        pid,
    )
    if not handle:
        return None
    counters = PROCESS_MEMORY_COUNTERS_EX()
    counters.cb = ctypes.sizeof(counters)
    try:
        ok = _PSAPI.GetProcessMemoryInfo(
            handle,
            ctypes.byref(counters),
            ctypes.sizeof(counters),
        )
        if not ok:
            return None
        return int(counters.WorkingSetSize), int(counters.PrivateUsage)
    finally:
        _KERNEL32.CloseHandle(handle)


def observe_process(
    command: list[str], cwd: Path, log_path: Path, sample_ms: int
) -> dict[str, object]:
    if os.name != "nt":
        raise RuntimeError("process-tree measurement is supported only on Windows")
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    peak_working_set = 0
    peak_private = 0
    max_concurrent = 0
    observed_pids: set[int] = set()
    with log_path.open("w", encoding="utf-8", newline="\n") as log:
        process = subprocess.Popen(
            command,
            cwd=cwd,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        while True:
            table = _windows_process_table()
            active = _descendants(process.pid, table)
            observed_pids.update(active)
            working_set = 0
            private = 0
            measured = 0
            for pid in active:
                memory = _process_memory(pid)
                if memory is None:
                    continue
                working_set += memory[0]
                private += memory[1]
                measured += 1
            peak_working_set = max(peak_working_set, working_set)
            peak_private = max(peak_private, private)
            max_concurrent = max(max_concurrent, measured)
            if process.poll() is not None:
                break
            time.sleep(sample_ms / 1000.0)
        exit_code = process.wait()
    return {
        "command": command,
        "cwd": str(cwd),
        "exit_code": exit_code,
        "wall_seconds": round(time.perf_counter() - started, 6),
        "sample_period_ms": sample_ms,
        "unique_processes": len(observed_pids),
        "max_concurrent_processes": max_concurrent,
        "peak_tree_working_set_bytes": peak_working_set,
        "peak_tree_private_bytes": peak_private,
        "log": str(log_path),
    }


def directory_size_iterative(root: Path) -> tuple[int, int]:
    """Return file count and bytes without recursive calls or symlink traversal."""

    files = 0
    size = 0
    pending = [root]
    while pending:
        current = pending.pop()
        try:
            entries = list(os.scandir(current))
        except OSError:
            continue
        for entry in entries:
            try:
                if entry.is_symlink():
                    continue
                if entry.is_dir(follow_symlinks=False):
                    pending.append(Path(entry.path))
                elif entry.is_file(follow_symlinks=False):
                    files += 1
                    size += entry.stat(follow_symlinks=False).st_size
            except OSError:
                continue
    return files, size


def parse_ctest_count(output: str) -> int:
    for line in reversed(output.splitlines()):
        if line.startswith("Total Tests:"):
            return int(line.split(":", 1)[1].strip())
    raise ValueError("ctest output does not contain Total Tests")


def logical_executable_names(output: str) -> set[str]:
    names: set[str] = set()
    for line in output.splitlines():
        target, separator, _rule = line.partition(": ")
        if separator and target.lower().endswith(".exe"):
            names.add(Path(target).name.lower())
    return names


def ninja_log_metrics(path: Path) -> dict[str, object]:
    """Summarize the latest clean Ninja edge log by output type."""

    latest: dict[str, tuple[int, int]] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 4:
            continue
        try:
            start = int(fields[0])
            end = int(fields[1])
        except ValueError:
            continue
        latest[fields[3]] = (start, end)

    def summarize(suffix: str) -> dict[str, int]:
        durations = [end - start for output, (start, end) in latest.items() if output.lower().endswith(suffix)]
        return {
            "edges": len(durations),
            "aggregate_milliseconds": sum(durations),
            "longest_milliseconds": max(durations, default=0),
        }

    return {
        "recorded_outputs": len(latest),
        "build_span_milliseconds": max((end for _start, end in latest.values()), default=0),
        "compile_objects": summarize(".obj"),
        "link_executables": summarize(".exe"),
    }


def _capture(command: list[str], cwd: Path) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(command)}\n{result.stdout}")
    return result.stdout


def build_snapshot(build_dir: Path) -> dict[str, object]:
    resolved = build_dir.resolve()
    if not (resolved / "CMakeCache.txt").is_file():
        raise ValueError(f"not a configured CMake build tree: {resolved}")
    ctest_output = _capture(["ctest", "--test-dir", str(resolved), "-N"], resolved)
    ninja_output = _capture(["ninja", "-C", str(resolved), "-t", "targets", "all"], resolved)
    files, size = directory_size_iterative(resolved)
    executables = logical_executable_names(ninja_output)
    return {
        "build_dir": str(resolved),
        "ctest_entries": parse_ctest_count(ctest_output),
        "logical_executables": len(executables),
        "logical_executable_names": sorted(executables),
        "disk_files": files,
        "disk_bytes": size,
        "ninja_log": ninja_log_metrics(resolved / ".ninja_log"),
    }


def harness_summary(path: Path) -> dict[str, object]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    rows = payload.get("results", [])
    if not isinstance(rows, list):
        raise ValueError(f"invalid harness results: {path}")
    # Negative compiler cases intentionally return non-zero.  The harness's
    # verified `passed` verdict, not the raw child exit code, is authoritative.
    failed = []
    for row in rows:
        if "passed" in row:
            row_passed = row.get("passed") is True
        else:
            # Specialized owner adapters predate the explicit verifier field;
            # their owner exit code and timeout are the preserved contract.
            row_passed = row.get("exit_code") == 0 and not row.get("timed_out")
        if not row_passed:
            failed.append(row)
    return {
        "path": str(path.resolve()),
        "selected": len(rows),
        "passed": len(rows) - len(failed),
        "failed": len(failed),
        "summed_case_seconds": round(
            sum(float(row.get("duration_seconds", 0.0)) for row in rows), 6
        ),
    }


def process_measurement(path: Path) -> dict[str, object]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("schema") != SCHEMA or payload.get("kind") != "process-observation":
        raise ValueError(f"invalid process measurement: {path}")
    return payload


def write_json(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Measure NSC test infrastructure")
    subparsers = parser.add_subparsers(dest="operation", required=True)

    run = subparsers.add_parser("run", help="measure one command and its process tree")
    run.add_argument("--name", required=True)
    run.add_argument("--cwd", required=True, type=Path)
    run.add_argument("--output", required=True, type=Path)
    run.add_argument("--log", required=True, type=Path)
    run.add_argument("--sample-ms", type=int, default=100)
    run.add_argument("command", nargs=argparse.REMAINDER)

    snapshot = subparsers.add_parser("snapshot", help="inventory existing build graphs")
    snapshot.add_argument("--legacy-build", required=True, type=Path)
    snapshot.add_argument("--v2-build", required=True, type=Path)
    snapshot.add_argument("--harness-summary", action="append", default=[], type=Path)
    snapshot.add_argument("--process-measurement", action="append", default=[], type=Path)
    snapshot.add_argument("--unit-wall-seconds", required=True, type=float)
    snapshot.add_argument("--manifest-wall-seconds", required=True, type=float)
    snapshot.add_argument("--specialized-wall-seconds", required=True, type=float)
    snapshot.add_argument("--output", required=True, type=Path)
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    if arguments.operation == "run":
        command = list(arguments.command)
        if command and command[0] == "--":
            command.pop(0)
        if not command:
            raise SystemExit("measure_test_infrastructure: error: missing command after --")
        if arguments.sample_ms < 20:
            raise SystemExit("measure_test_infrastructure: error: --sample-ms must be at least 20")
        payload = {
            "schema": SCHEMA,
            "version": 1,
            "kind": "process-observation",
            "name": arguments.name,
            **observe_process(command, arguments.cwd.resolve(), arguments.log.resolve(), arguments.sample_ms),
        }
        write_json(arguments.output.resolve(), payload)
        print(f"measurement={arguments.output.resolve()}")
        print(f"exit_code={payload['exit_code']}")
        print(f"wall_seconds={payload['wall_seconds']}")
        print(f"peak_tree_working_set_bytes={payload['peak_tree_working_set_bytes']}")
        return int(payload["exit_code"])

    payload = {
        "schema": SCHEMA,
        "version": 1,
        "kind": "graph-snapshot",
        "legacy": build_snapshot(arguments.legacy_build),
        "v2": build_snapshot(arguments.v2_build),
        "harness": [harness_summary(path.resolve()) for path in arguments.harness_summary],
        "process_measurements": [
            process_measurement(path.resolve()) for path in arguments.process_measurement
        ],
        "o0_wall_seconds": {
            "unit_consolidated": arguments.unit_wall_seconds,
            "manifest_cases": arguments.manifest_wall_seconds,
            "specialized_suites": arguments.specialized_wall_seconds,
            "total": round(
                arguments.unit_wall_seconds
                + arguments.manifest_wall_seconds
                + arguments.specialized_wall_seconds,
                6,
            ),
        },
    }
    write_json(arguments.output.resolve(), payload)
    print(f"snapshot={arguments.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
