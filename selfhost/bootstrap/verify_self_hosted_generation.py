#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path


SCHEMA = "nativesharp-selfhost-generation-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh11-report"
TH32CS_SNAPPROCESS = 0x00000002
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
        ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260),
    ]


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_files(root: Path, relative_roots: list[str]) -> list[Path]:
    files: list[Path] = []
    for relative in relative_roots:
        directory = root / relative
        require(directory.is_dir(), f"missing source root: {directory}")
        files.extend(directory.rglob("*.ns"))
    return sorted(set(files), key=lambda path: path.relative_to(root).as_posix())


def source_manifest(root: Path, contract: dict, target: Path) -> tuple[list[dict[str, object]], str]:
    paths = source_files(root, contract["source_roots"])
    validation_paths = source_files(root, contract["generation_validation_roots"])
    entries = [
        {"path": path.relative_to(root).as_posix(), "size": path.stat().st_size, "sha256": digest(path)}
        for path in paths
    ]
    target.write_text("".join(f"{path}\n" for path in validation_paths), encoding="utf-8")
    canonical = "".join(f"{item['path']}\0{item['size']}\0{item['sha256']}\n" for item in entries)
    return entries, hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def process_snapshot() -> dict[int, tuple[int, str]]:
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        raise OSError(ctypes.get_last_error(), "CreateToolhelp32Snapshot")
    result: dict[int, tuple[int, str]] = {}
    entry = PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32W)
    try:
        if kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            while True:
                result[int(entry.th32ProcessID)] = (int(entry.th32ParentProcessID), entry.szExeFile)
                if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                    break
    finally:
        kernel32.CloseHandle(snapshot)
    return result


def descendants(root_pid: int, snapshot: dict[int, tuple[int, str]]) -> list[dict[str, object]]:
    selected: set[int] = {root_pid}
    changed = True
    while changed:
        changed = False
        for pid, (parent, _) in snapshot.items():
            if parent in selected and pid not in selected:
                selected.add(pid)
                changed = True
    return [
        {"pid": pid, "parent_pid": snapshot[pid][0], "image": snapshot[pid][1]}
        for pid in sorted(selected) if pid != root_pid and pid in snapshot
    ]


def run(executable: Path, *arguments: str, env: dict[str, str] | None = None) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    completed = subprocess.run(
        [str(executable), *arguments], env=env, text=True, encoding="utf-8", errors="strict",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    return completed, time.perf_counter() - started


def audited_generation(producer: Path, generation: str, manifest: Path, output: Path,
                       trace: Path, environment: dict[str, str]) -> dict[str, object]:
    command = [str(producer), "--bootstrap-generation", generation, str(manifest),
               str(producer), str(output), str(trace)]
    started = time.perf_counter()
    process = subprocess.Popen(command, env=environment, text=True, encoding="utf-8", errors="strict",
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    observed: dict[tuple[int, int, str], dict[str, object]] = {}
    while process.poll() is None:
        for item in descendants(process.pid, process_snapshot()):
            observed[(int(item["pid"]), int(item["parent_pid"]), str(item["image"]))] = item
        time.sleep(0.001)
    stdout, stderr = process.communicate()
    elapsed = time.perf_counter() - started
    require(process.returncode == 0, f"{generation} production failed: {stdout}{stderr}")
    require(not observed, f"{generation} unexpectedly spawned child processes: {list(observed.values())}")
    require(output.is_file() and trace.is_file(), f"{generation} did not materialize its outputs")
    trace_fields = dict(line.split("=", 1) for line in trace.read_text(encoding="utf-8").splitlines() if "=" in line)
    require(trace_fields.get("generation") == generation, "generation trace changed")
    require(trace_fields.get("method") == "source-attested-image-replay", "generation method changed")
    require(trace_fields.get("source_derived_codegen") == "false", "source-derived claim is inaccurate")
    return {
        "command": command,
        "producer_pid": process.pid,
        "descendants": list(observed.values()),
        "environment": {key: environment.get(key, "") for key in ("LANG", "LC_ALL", "TZ")},
        "elapsed_seconds": round(elapsed, 6),
        "stdout": stdout,
        "stderr": stderr,
        "trace": trace_fields,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify NativeSharp nsc1 to nsc2 transition.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--clean-a-root", type=Path, required=True)
    parser.add_argument("--clean-b-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc1", type=Path, required=True)
    parser.add_argument("--adapter", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--import-library", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA and contract.get("version") == 1, "unsupported SH11 contract")
        require(len(contract.get("work_items", [])) == 11, "incomplete SH11 ledger")
        require(contract.get("source_derived_codegen") is False, "SH11 must not overclaim source-derived codegen")
        arguments.output_dir.mkdir(parents=True, exist_ok=True)

        manifest_a = arguments.output_dir / "sources-a.manifest"
        manifest_b = arguments.output_dir / "sources-b.manifest"
        entries_a, digest_a = source_manifest(arguments.clean_a_root, contract, manifest_a)
        entries_b, digest_b = source_manifest(arguments.clean_b_root, contract, manifest_b)
        relative_a = [{**item, "path": str(item["path"])} for item in entries_a]
        relative_b = [{**item, "path": str(item["path"])} for item in entries_b]
        require(relative_a == relative_b and digest_a == digest_b, "clean roots differ")

        nsc2_a = arguments.output_dir / "nsc2-a.exe"
        nsc2_b = arguments.output_dir / "nsc2-b.exe"
        env_a = os.environ.copy(); env_a.update({"LANG": "C", "LC_ALL": "C", "TZ": "UTC"})
        env_b = os.environ.copy(); env_b.update({"LANG": "tr_TR.UTF-8", "LC_ALL": "tr_TR.UTF-8", "TZ": "Pacific/Auckland"})
        audits = [
            audited_generation(arguments.nsc1, "nsc2", manifest_a, nsc2_a,
                               arguments.output_dir / "nsc2-a.trace", env_a),
            audited_generation(arguments.nsc1, "nsc2", manifest_b, nsc2_b,
                               arguments.output_dir / "nsc2-b.trace", env_b),
        ]
        hashes = {"nsc0": digest(arguments.nsc0), "nsc1": digest(arguments.nsc1),
                  "nsc2_a": digest(nsc2_a), "nsc2_b": digest(nsc2_b)}
        require(hashes["nsc1"] == hashes["nsc2_a"] == hashes["nsc2_b"], "nsc2 image is not deterministic")

        timings: dict[str, float] = {}
        version, timings["version"] = run(nsc2_a, "--version")
        require(version.returncode == 0 and version.stdout.strip() == contract["expected_identity"], "nsc2 identity changed")
        self_test, timings["self_test"] = run(nsc2_a, "--self-test-basic")
        require(self_test.returncode == 42, "nsc2 self-test failed")

        compiler_sources = sorted((arguments.source_root / "selfhost/nsc").rglob("*.ns"))
        for source in compiler_sources:
            result, elapsed = run(nsc2_a, "--verify-ssa", str(source))
            require(result.returncode == 0, f"nsc2 rejected {source}: {result.stderr}")
            timings[f"ssa:{source.name}"] = elapsed
        for project in contract["profile_projects"]:
            result, elapsed = run(nsc2_a, "--verify-project", str(arguments.source_root / project))
            require(result.returncode == 0, f"nsc2 rejected project {project}")
            timings[f"project:{project}"] = elapsed

        missing = arguments.output_dir / "missing.ns"
        diag1, _ = run(arguments.nsc1, str(missing))
        diag2, timings["diagnostic"] = run(nsc2_a, str(missing))
        require((diag1.returncode, diag1.stdout, diag1.stderr) == (diag2.returncode, diag2.stdout, diag2.stderr),
                "nsc1/nsc2 diagnostic divergence")
        nsmeta1 = arguments.output_dir / "nsc1.nsmeta"
        nsmeta2 = arguments.output_dir / "nsc2.nsmeta"
        project_path = arguments.source_root / contract["compiler_project"]
        require(run(arguments.nsc1, "--emit-nsmeta", str(project_path), str(nsmeta1))[0].returncode == 0,
                "nsc1 nsmeta emission failed")
        require(run(nsc2_a, "--emit-nsmeta", str(project_path), str(nsmeta2))[0].returncode == 0,
                "nsc2 nsmeta emission failed")
        require(nsmeta1.read_bytes() == nsmeta2.read_bytes(), "nsc1/nsc2 nsmeta divergence")

        artifacts: dict[str, str] = {}
        executions = 0
        execution_project = arguments.source_root / contract["execution_project"]
        for profile in contract["optimizations"]:
            lower = profile.lower()
            objects: list[Path] = []
            for label, compiler in (("nsc1", arguments.nsc1), ("nsc2", nsc2_a)):
                obj = arguments.output_dir / f"{label}-{lower}.obj"
                result, elapsed = run(compiler, "--emit-project-object", str(execution_project), f"-{profile}", "-o", str(obj))
                require(result.returncode == 0, f"{label}/{profile} object failed: {result.stderr}")
                timings[f"object:{label}:{profile}"] = elapsed
                artifacts[obj.name] = digest(obj)
                objects.append(obj)
            require(objects[0].read_bytes() == objects[1].read_bytes(), f"nsc1/nsc2 {profile} object divergence")
            for route, tool in (("lld", arguments.linker), ("nslink", arguments.nslink)):
                executable = arguments.output_dir / f"{route}-{lower}.exe"
                response = arguments.output_dir / f"{route}-{lower}.rsp"
                log = arguments.output_dir / f"{route}-{lower}.log"
                linked, elapsed = run(arguments.adapter, route, str(tool), str(objects[1]),
                                      str(arguments.import_library), str(response), str(executable), str(log))
                require(linked.returncode == 0, f"{route}/{profile} link failed: {linked.stdout}{linked.stderr}")
                timings[f"link:{route}:{profile}"] = elapsed
                executed, elapsed = run(executable)
                require(executed.returncode == contract["expected_exit_code"], f"{route}/{profile} exited {executed.returncode}")
                timings[f"execute:{route}:{profile}"] = elapsed
                artifacts[executable.name] = digest(executable)
                executions += 1

        report = {
            "schema": REPORT_SCHEMA, "version": 1, "status": "passed",
            "generation": "nsc2", "built_by": "nsc1",
            "generation_method": contract["generation_method"],
            "source_derived_codegen": False,
            "source_files": len(entries_a), "source_sha256": digest_a,
            "generation_sources_verified": len(source_files(arguments.clean_a_root, contract["generation_validation_roots"])),
            "clean_roots": 2, "locale_timezone_variants": 2,
            "nsc0_invocations_during_generation": 0,
            "generation_process_audits": audits, "hashes": hashes,
            "compiler_sources_verified": len(compiler_sources),
            "profile_projects_verified": len(contract["profile_projects"]),
            "profiles": len(contract["optimizations"]), "linkers": len(contract["linkers"]),
            "executions": executions, "diagnostics_equal": True, "nsmeta_equal": True,
            "objects_equal": True, "artifacts": dict(sorted(artifacts.items())),
            "timings_seconds": {key: round(value, 6) for key, value in sorted(timings.items())},
            "work_items": contract["work_items"], "problems": [],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, UnicodeError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_self_hosted_generation: error: {error}", file=sys.stderr)
        return 1
    print("SH11 nsc2 verified: nsc1-only generation, process isolation, two roots, O0-O3, two linkers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
