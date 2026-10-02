#!/usr/bin/env python3
"""SH52: hostile inputs, cancellation, atomic failure and reproducibility."""

from __future__ import annotations

import argparse
import hashlib
import json
import random
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], cwd: Path, timeout: int = 1200) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout, check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--selfhost-compiler", type=Path, required=True)
    parser.add_argument("--ctest", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()

    root = args.source_root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    failures: list[str] = []
    if (contract.get("schema") != "nativesharp-selfhost-robustness-final-contract" or
            contract.get("gate") != "SH52"):
        print("NSH5201: unsupported SH52 contract", file=sys.stderr)
        return 1

    rng = random.Random(0x53483532)
    alphabet = "{}[]();,+-*/<>!= abcdefghijklmnopqrstuvwxyz0123456789\n\t\"'@#$%^&|"
    fuzz_dir = output / "fuzz"
    fuzz_dir.mkdir(exist_ok=True)
    fuzz_results: list[dict[str, object]] = []
    for index in range(contract["fuzz_cases"]):
        payload = "".join(rng.choice(alphabet) for _ in range(128 + index * 31))
        source = fuzz_dir / f"hostile-{index:02d}.ns"
        source.write_text(payload, encoding="utf-8", newline="\n")
        for label, compiler in (("nsc0", args.compiler), ("nsc2", args.selfhost_compiler)):
            if label == "nsc2" and index >= contract["selfhost_fuzz_cases"]:
                continue
            result = run([str(compiler), "--dump-tokens", str(source)], root, timeout=20)
            if result.returncode not in (0, 1):
                failures.append(f"{label} hostile case {index} returned {result.returncode}")
        fuzz_results.append({"case": index, "sha256": digest(source)})

    corrupt = output / "corrupt.nsmeta"
    corrupt.write_bytes(b"NSMETA\x00\xffhostile\r\nModule=truncated")
    metadata = run([str(args.compiler), "--verify-library-metadata", str(corrupt)], root)
    if metadata.returncode == 0:
        failures.append("corrupt metadata was accepted")

    runtime_project = root / "selfhost/tests/runtime-2.0/RuntimeLibraries.nsproj"
    timed = output / "cancelled.obj"
    cancelled = run([str(args.compiler), "--emit-project-oop-obj", str(runtime_project), "-O2",
                     f"--timeout-ms={contract['timeout_ms']}", "-o", str(timed)], root)
    if cancelled.returncode == 0 or "cancel" not in cancelled.stderr.lower() or timed.exists():
        failures.append("cooperative timeout did not cancel atomically")

    occupied = output / "occupied.obj"
    occupied.mkdir(exist_ok=True)
    atomic = run([str(args.compiler), "--emit-project-oop-obj", str(runtime_project), "-O0",
                  "-o", str(occupied)], root)
    if atomic.returncode == 0 or not occupied.is_dir() or any(occupied.iterdir()):
        failures.append("failed output publication was not atomic")
    if occupied.is_dir():
        shutil.rmtree(occupied)
    else:
        occupied.unlink(missing_ok=True)

    first = output / "repeat-a.obj"
    second = output / "repeat-b.obj"
    for destination in (first, second):
        compiled = run([str(args.compiler), "--emit-project-oop-obj", str(runtime_project), "-O0",
                        "-o", str(destination)], root)
        if compiled.returncode != 0:
            failures.append(f"reproducibility compilation failed for {destination.name}")
            break
    reproducible = first.is_file() and second.is_file() and digest(first) == digest(second)
    if not reproducible:
        failures.append("repeated clean O0 objects differ")

    interrupted = False
    interrupt_source = output / "interrupt.ns"
    for size_mib in contract["interrupt_payload_mib"]:
        interrupt_source.write_text(" " * (size_mib * 1024 * 1024) + "class End {}\n",
                                    encoding="ascii", newline="\n")
        process = subprocess.Popen([str(args.compiler), "--dump-tokens", str(interrupt_source)],
                                   cwd=root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.02)
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
            interrupted = True
            break
        process.wait()
    interrupt_source.unlink(missing_ok=True)
    if not interrupted:
        failures.append("could not exercise external process interruption")

    messagebox_sources: list[str] = []
    for base in (root / "src", root / "include"):
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in {".cpp", ".c", ".h", ".hpp"}:
                if "messagebox" in path.read_text(encoding="utf-8", errors="replace").lower():
                    messagebox_sources.append(path.relative_to(root).as_posix())
    binary_text = args.compiler.read_bytes().lower()
    if messagebox_sources or b"messageboxa" in binary_text or b"messageboxw" in binary_text:
        failures.append(f"MessageBox dependency detected: {messagebox_sources}")

    names = contract["ctests"]
    expression = "^(" + "|".join(re.escape(name) for name in names) + ")$"
    ctest = run([str(args.ctest), "--test-dir", str(args.build_dir), "-C", "Release",
                 "-R", expression, "--output-on-failure", "-j1"], root)
    if ctest.returncode != 0:
        failures.append(f"robustness CTest matrix: {ctest.stdout}{ctest.stderr}")

    temporary_leaks = sorted(path.relative_to(output).as_posix() for path in output.rglob("*")
                             if path.is_file() and path.suffix.lower() in {".tmp", ".temp"})
    if temporary_leaks:
        failures.append(f"temporary files leaked: {temporary_leaks}")

    report = {
        "schema": "nativesharp-selfhost-robustness-final-report",
        "version": 1,
        "gate": "SH52",
        "tanda": 54,
        "status": "passed" if not failures else "failed",
        "compiler_sha256": digest(args.compiler),
        "selfhost_compiler_sha256": digest(args.selfhost_compiler),
        "contract_sha256": digest(args.contract),
        "fuzz": {"host": len(fuzz_results), "selfhost": contract["selfhost_fuzz_cases"]},
        "corrupt_metadata_rejected": metadata.returncode != 0,
        "cooperative_timeout": cancelled.returncode != 0 and not timed.exists(),
        "atomic_output_failure": atomic.returncode != 0,
        "external_interruption": interrupted,
        "messagebox_dependencies": messagebox_sources,
        "reproducible_o0": reproducible,
        "object_sha256": digest(first) if first.is_file() else None,
        "temporary_leaks": temporary_leaks,
        "ctests": {"expected": len(names), "passed": len(names) if ctest.returncode == 0 else 0,
                    "names": names},
        "failures": failures,
    }
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8", newline="\n")
    if failures:
        for failure in failures:
            print(f"NSH5202: {failure}", file=sys.stderr)
        return 1
    print(f"SH52 robustness passed: {len(fuzz_results)} host fuzz, "
          f"{contract['selfhost_fuzz_cases']} selfhost fuzz, {len(names)}/{len(names)} CTests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
