#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import subprocess
import sys
import time
from pathlib import Path


SCHEMA = "nativesharp-selfhost-first-generation-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh10-report"


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_files(root: Path, contract: dict) -> list[Path]:
    result: list[Path] = []
    for relative in contract["source_roots"]:
        directory = root / relative
        require(directory.is_dir(), f"missing source root: {relative}")
        result.extend(path for path in directory.rglob("*") if path.suffix in {".ns", ".nsproj"})
    return sorted(set(result), key=lambda path: path.relative_to(root).as_posix())


def manifest(root: Path, contract: dict) -> tuple[list[dict[str, object]], str]:
    entries = [
        {
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": digest(path),
        }
        for path in source_files(root, contract)
    ]
    canonical = "".join(f"{entry['path']}\0{entry['size']}\0{entry['sha256']}\n" for entry in entries)
    return entries, hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def run(executable: Path, *arguments: str) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    completed = subprocess.run(
        [str(executable), *arguments],
        text=True,
        encoding="utf-8",
        errors="strict",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    return completed, time.perf_counter() - started


def pe_contract(path: Path, required_imports: list[str]) -> dict[str, object]:
    data = path.read_bytes()
    require(len(data) >= 0x100 and data[:2] == b"MZ", f"{path.name} is not a PE image")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    require(data[pe:pe + 4] == b"PE\0\0", f"{path.name} misses PE signature")
    machine, sections, timestamp = struct.unpack_from("<HHI", data, pe + 4)
    require(machine == 0x8664 and sections >= 4, f"{path.name} is not AMD64 PE")
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    require(optional_size >= 0xF0 and struct.unpack_from("<H", data, optional)[0] == 0x20B,
            f"{path.name} is not PE32+")
    imports = [name for name in required_imports if name.encode("ascii") in data]
    require(imports == required_imports, f"{path.name} import surface changed: {imports}")
    return {"machine": "AMD64", "sections": sections, "timestamp": timestamp, "imports": imports}


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify NativeSharp first generation nsc1.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--clean-a-root", type=Path, required=True)
    parser.add_argument("--clean-b-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc1-a", type=Path, required=True)
    parser.add_argument("--nsc1-b", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA and contract.get("version") == 1,
                "unsupported SH10 contract")
        require(len(contract.get("work_items", [])) == 12, "incomplete SH10 ledger")
        arguments.output_dir.mkdir(parents=True, exist_ok=True)

        source_entries, source_digest = manifest(arguments.source_root, contract)
        clean_a_entries, clean_a_digest = manifest(arguments.clean_a_root, contract)
        clean_b_entries, clean_b_digest = manifest(arguments.clean_b_root, contract)
        require(source_entries == clean_a_entries == clean_b_entries,
                "clean-tree source manifests differ from the frozen source set")
        require(source_digest == clean_a_digest == clean_b_digest,
                "clean-tree source digests differ")

        first_hash = digest(arguments.nsc1_a)
        second_hash = digest(arguments.nsc1_b)
        require(first_hash == second_hash, "nsc1 differs between independent absolute paths")
        pe = pe_contract(arguments.nsc1_a, contract["required_imports"])
        require(pe_contract(arguments.nsc1_b, contract["required_imports"]) == pe,
                "nsc1 PE contracts differ")
        require((arguments.linker.parent / "LLVM-C.dll").is_file(), "LLVM-C.dll is not physical")
        require(arguments.nslink.is_file(), "NSLink is not physical")

        timings: dict[str, float] = {}
        version, timings["version"] = run(arguments.nsc1_a, "--version")
        require(version.returncode == 0 and version.stdout.strip() == contract["expected_identity"],
                "nsc1 identity changed")
        help_result, timings["help"] = run(arguments.nsc1_a, "--help")
        require(help_result.returncode == 0 and "Usage:" in help_result.stdout, "nsc1 help failed")
        self_test, timings["self_test"] = run(arguments.nsc1_a, "--self-test-basic")
        require(self_test.returncode == 42, "nsc1 basic self-test failed")

        missing = arguments.output_dir / "missing source.ns"
        nsc0_missing, _ = run(arguments.nsc0, str(missing))
        nsc1_missing, timings["diagnostic"] = run(arguments.nsc1_a, str(missing))
        require((nsc0_missing.returncode, nsc0_missing.stdout, nsc0_missing.stderr) ==
                (nsc1_missing.returncode, nsc1_missing.stdout, nsc1_missing.stderr),
                "nsc1 missing-source diagnostic differs from nsc0")

        compiler_sources = sorted((arguments.source_root / "selfhost/nsc").rglob("*.ns"))
        require(len(compiler_sources) >= 20, "compiler source set unexpectedly small")
        subsystem_checks = 0
        for source in compiler_sources:
            result, elapsed = run(arguments.nsc1_a, "--verify-ssa", str(source))
            require(result.returncode == 0, f"nsc1 rejected compiler source {source.name}: {result.stderr}")
            timings[f"ssa:{source.relative_to(arguments.source_root).as_posix()}"] = elapsed
            subsystem_checks += 1

        for project in contract["profile_projects"]:
            result, elapsed = run(arguments.nsc1_a, "--verify-project", str(arguments.source_root / project))
            require(result.returncode == 0, f"nsc1 rejected bootstrap project {project}")
            timings[f"project:{project}"] = elapsed
            subsystem_checks += 1

        artifact_hashes: dict[str, str] = {}
        executions = 0
        for profile in contract["optimizations"]:
            lower = profile.lower()
            obj = arguments.output_dir / f"corpus-{lower}.obj"
            require(obj.is_file() and obj.stat().st_size > 0, f"missing corpus object {profile}")
            artifact_hashes[obj.name] = digest(obj)
            for route in ("lld", "nslink"):
                executable = arguments.output_dir / f"{route}-{lower}.exe"
                require(executable.is_file(), f"missing {route}/{profile} executable")
                artifact_hashes[executable.name] = digest(executable)
                completed, elapsed = run(executable)
                require(completed.returncode == contract["expected_exit_code"],
                        f"{route}/{profile} exited {completed.returncode}")
                timings[f"execute:{route}:{profile}"] = elapsed
                executions += 1

        report = {
            "schema": REPORT_SCHEMA,
            "version": 1,
            "status": "passed",
            "generation": "nsc1",
            "built_by": "nsc0",
            "source_files": len(source_entries),
            "source_sha256": source_digest,
            "source_manifest": source_entries,
            "nsc0_sha256": digest(arguments.nsc0),
            "nsc1_sha256": first_hash,
            "clean_roots": 2,
            "byte_identical_clean_builds": True,
            "pe": pe,
            "compiler_sources_verified": len(compiler_sources),
            "subsystem_checks": subsystem_checks,
            "profiles": len(contract["optimizations"]),
            "linkers": len(contract["linkers"]),
            "executions": executions,
            "binary_size": arguments.nsc1_a.stat().st_size,
            "timings_seconds": {key: round(value, 6) for key, value in sorted(timings.items())},
            "artifacts": dict(sorted(artifact_hashes.items())),
            "work_items": contract["work_items"],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, UnicodeError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_first_generation: error: {error}", file=sys.stderr)
        return 1
    print("SH10 nsc1 verified: two clean roots, frozen sources, compiler corpus, O0-O3, two linkers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
