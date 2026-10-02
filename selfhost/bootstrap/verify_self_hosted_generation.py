#!/usr/bin/env python3
"""Verify the first real NativeSharp source-derived self-compilation (SH41)."""
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
REPORT_SCHEMA = "nativesharp-selfhost-sh41-report"
TH32CS_SNAPPROCESS = 0x00000002
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
# A compiler invocation lasts long enough that a 100 Hz audit still observes any real seed
# compiler process, while avoiding the former 1,000 full Windows process-table snapshots/second.
# One final snapshot is taken after exit so children alive at the boundary are not lost.
PROCESS_AUDIT_INTERVAL_SECONDS = 0.010


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


def compiler_sources(root: Path, relative_roots: list[str]) -> list[Path]:
    files: list[Path] = []
    for relative in relative_roots:
        directory = root / relative
        require(directory.is_dir(), f"missing source root: {directory}")
        files.extend(directory.rglob("*.ns"))
        files.extend(directory.rglob("*.nsproj"))
    return sorted(set(files), key=lambda path: path.relative_to(root).as_posix())


def source_manifest(root: Path, relative_roots: list[str], target: Path) -> tuple[int, str]:
    paths = compiler_sources(root, relative_roots)
    rows = []
    canonical = bytearray()
    for path in paths:
        relative = path.relative_to(root).as_posix()
        sha256 = digest(path)
        size = path.stat().st_size
        rows.append(f"{sha256}  {size}  {relative}\n")
        canonical.extend(relative.encode("utf-8"))
        canonical.extend(b"\0")
        canonical.extend(str(size).encode("ascii"))
        canonical.extend(b"\0")
        canonical.extend(sha256.encode("ascii"))
        canonical.extend(b"\n")
    target.write_text("".join(rows), encoding="utf-8", newline="\n")
    return len(paths), hashlib.sha256(canonical).hexdigest()


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
                result[int(entry.th32ProcessID)] = (
                    int(entry.th32ParentProcessID), entry.szExeFile)
                if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                    break
    finally:
        kernel32.CloseHandle(snapshot)
    return result


def descendants(root_pid: int, snapshot: dict[int, tuple[int, str]]) -> list[dict[str, object]]:
    selected = {root_pid}
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


def run(command: list[str], environment: dict[str, str] | None = None) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    completed = subprocess.run(
        command, env=environment, text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    return completed, time.perf_counter() - started


def audited_emit(producer: Path, project: Path, profile: str, output: Path,
                 forbidden_image: str, environment: dict[str, str]) -> tuple[dict[str, object], float]:
    command = [str(producer), "--emit-bootstrap-object", str(project),
               f"-{profile}", "-o", str(output)]
    started = time.perf_counter()
    process = subprocess.Popen(
        command, env=environment, text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    observed: dict[tuple[int, int, str], dict[str, object]] = {}
    while process.poll() is None:
        for item in descendants(process.pid, process_snapshot()):
            key = (int(item["pid"]), int(item["parent_pid"]), str(item["image"]))
            observed[key] = item
        time.sleep(PROCESS_AUDIT_INTERVAL_SECONDS)
    for item in descendants(process.pid, process_snapshot()):
        key = (int(item["pid"]), int(item["parent_pid"]), str(item["image"]))
        observed[key] = item
    stdout, stderr = process.communicate()
    elapsed = time.perf_counter() - started
    require(process.returncode == 0,
            f"{producer.name}/{profile} emission failed ({process.returncode}): {stdout}{stderr}")
    require(output.is_file() and output.stat().st_size > 0,
            f"{producer.name}/{profile} did not create {output}")
    forbidden = forbidden_image.casefold()
    accidental = [item for item in observed.values()
                  if str(item["image"]).casefold() == forbidden]
    require(not accidental, f"source compiler spawned forbidden seed compiler: {accidental}")
    return {
        "command": command,
        "producer_pid": process.pid,
        "descendants": list(observed.values()),
        "forbidden_seed_image": forbidden_image,
        "forbidden_seed_invocations": 0,
        "stdout": stdout,
        "stderr": stderr,
        "elapsed_seconds": round(elapsed, 6),
    }, elapsed


def link_image(tool: Path, object_path: Path, libraries: list[Path],
               output: Path) -> tuple[dict[str, object], float]:
    command = [str(tool), "--link", str(object_path)]
    for library in libraries:
        command.extend(["--lib", str(library)])
    command.extend(["--entry", "mainCRTStartup", "-o", str(output)])
    completed, elapsed = run(command)
    require(completed.returncode == 0,
            f"NSLink failed ({completed.returncode}): {completed.stdout}{completed.stderr}")
    require(output.is_file() and output.stat().st_size > 0, f"NSLink did not create {output}")
    return {
        "command": command, "stdout": completed.stdout, "stderr": completed.stderr,
        "elapsed_seconds": round(elapsed, 6), "sha256": digest(output),
    }, elapsed


def verify_image(image: Path, identity: str, project: Path) -> dict[str, object]:
    version, version_time = run([str(image), "--version"])
    require(version.returncode == 0 and version.stdout.strip() == identity,
            f"identity mismatch for {image}: {version.stdout}{version.stderr}")
    self_test, self_test_time = run([str(image), "--self-test-basic"])
    require(self_test.returncode == 42, f"self-test failed for {image}: {self_test.returncode}")
    project_check, check_time = run([str(image), "--check-project", str(project)])
    require(project_check.returncode == 0,
            f"project check failed for {image}: {project_check.stdout}{project_check.stderr}")
    return {
        "identity": version.stdout.strip(), "self_test_exit_code": self_test.returncode,
        "project_check_exit_code": project_check.returncode,
        "timings_seconds": {
            "version": round(version_time, 6), "self_test": round(self_test_time, 6),
            "check_project": round(check_time, 6),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify NativeSharp SH41 real self-compilation.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc1", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--runtime-library", type=Path, required=True)
    parser.add_argument("--llvm-library", type=Path, required=True)
    parser.add_argument("--import-library", action="append", type=Path, default=[])
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA and contract.get("version") == 2,
                "unsupported SH41 contract")
        require(contract.get("generation_method") == "source-derived-llvm-object",
                "SH41 requires source-derived object generation")
        require(contract.get("source_derived_codegen") is True,
                "SH41 contract must require source-derived code generation")
        require(arguments.nsc0.is_file() and arguments.nsc1.is_file(), "missing seed or nsc1")
        require(arguments.nslink.is_file(), "missing NSLink")
        libraries = [arguments.runtime_library, arguments.llvm_library, *arguments.import_library]
        require(all(path.is_file() for path in libraries), "one or more link libraries are missing")
        arguments.output_dir.mkdir(parents=True, exist_ok=True)
        project = arguments.source_root / contract["compiler_project"]
        source_count, source_sha256 = source_manifest(
            arguments.source_root, contract["source_roots"],
            arguments.output_dir / "compiler-sources.manifest")
        environment = os.environ.copy()
        environment.update({"LANG": "C", "LC_ALL": "C", "TZ": "UTC"})

        profiles: dict[str, object] = {}
        hashes: dict[str, str] = {"nsc0": digest(arguments.nsc0), "nsc1": digest(arguments.nsc1)}
        total_executions = 0
        for profile in contract["optimizations"]:
            lower = profile.lower()
            nsc2_object = arguments.output_dir / f"nsc2-{lower}.obj"
            nsc2_audit, _ = audited_emit(
                arguments.nsc1, project, profile, nsc2_object,
                arguments.nsc0.name, environment)
            hashes[f"nsc2_{lower}_object"] = digest(nsc2_object)

            nsc2_links: dict[str, object] = {}
            nsc2_image = arguments.output_dir / f"nsc2-{lower}-nslink.exe"
            link_record, _ = link_image(
                arguments.nslink, nsc2_object, libraries, nsc2_image)
            link_record["verification"] = verify_image(
                nsc2_image, contract["expected_identity"], project)
            nsc2_links["nslink"] = link_record
            hashes[f"nsc2_{lower}_nslink_image"] = digest(nsc2_image)
            total_executions += 1

            nsc3_object = arguments.output_dir / f"nsc3-{lower}.obj"
            nsc3_audit, _ = audited_emit(
                nsc2_image, project, profile, nsc3_object,
                arguments.nsc0.name, environment)
            hashes[f"nsc3_{lower}_object"] = digest(nsc3_object)
            require(nsc2_object.read_bytes() == nsc3_object.read_bytes(),
                    f"{profile} nsc2/nsc3 objects are not byte-identical")

            nsc3_links: dict[str, object] = {}
            nsc3_image = arguments.output_dir / f"nsc3-{lower}-nslink.exe"
            link_record, _ = link_image(
                arguments.nslink, nsc3_object, libraries, nsc3_image)
            link_record["verification"] = verify_image(
                nsc3_image, contract["expected_identity"], project)
            nsc3_links["nslink"] = link_record
            hashes[f"nsc3_{lower}_nslink_image"] = digest(nsc3_image)
            total_executions += 1

            profiles[profile] = {
                "nsc2_audit": nsc2_audit, "nsc3_audit": nsc3_audit,
                "object_byte_identical": True,
                "object_sha256": hashes[f"nsc2_{lower}_object"],
                "nsc2_links": nsc2_links, "nsc3_links": nsc3_links,
            }

        report = {
            "schema": REPORT_SCHEMA, "version": 1, "status": "passed",
            "gate": "SH41", "tanda": 43,
            "generation": "nsc1->nsc2->nsc3",
            "generation_method": contract["generation_method"],
            "source_derived_codegen": True, "image_replay": False,
            "nsc0_invocations_during_generation": 0,
            "source_files": source_count, "source_sha256": source_sha256,
            "profiles": profiles, "profile_count": len(profiles),
            "linkers": ["NSLink"], "executions": total_executions,
            "object_fixed_point_per_profile": True,
            "hashes": dict(sorted(hashes.items())),
            "work_items": contract["work_items"], "problems": [],
        }
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    except (OSError, UnicodeError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_self_hosted_generation: error: {error}", file=sys.stderr)
        return 1
    print("SH41 passed: real nsc1->nsc2->nsc3 source compilation, O0-O3, NSLink")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
