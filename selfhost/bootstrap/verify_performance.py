#!/usr/bin/env python3
"""SH50: measured optimization and build-performance contract."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], cwd: Path, timeout: int = 1200) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=timeout, check=False)
    return result, time.perf_counter() - started


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--ctest", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    root = args.source_root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    failures: list[str] = []
    if contract.get("schema") != "nativesharp-selfhost-performance-contract" or contract.get("gate") != "SH50":
        print("NSH5001: unsupported SH50 contract", file=sys.stderr)
        return 1

    project = root / contract["project"]
    profiles: dict[str, dict[str, object]] = {}
    for profile in contract["profiles"]:
        elapsed_values: list[float] = []
        hashes: list[str] = []
        sizes: list[int] = []
        for repetition in range(int(contract["repetitions"])):
            artifact = output / f"runtime-{profile.lower()}-r{repetition + 1}.obj"
            result, elapsed = run([str(args.compiler), "--emit-project-library-object", str(project),
                                   f"-{profile}", "-o", str(artifact)], root)
            if result.returncode != 0:
                failures.append(f"{profile} repetition {repetition + 1}: {result.stdout}{result.stderr}")
                continue
            elapsed_values.append(elapsed)
            hashes.append(digest(artifact))
            sizes.append(artifact.stat().st_size)
        if len(set(hashes)) > 1:
            failures.append(f"{profile} is not byte deterministic across repetitions")
        profiles[profile] = {
            "seconds": [round(value, 6) for value in elapsed_values],
            "median_seconds": round(statistics.median(elapsed_values), 6) if elapsed_values else None,
            "sha256": hashes[0] if hashes else None,
            "bytes": sizes[0] if sizes else None,
            "deterministic": bool(hashes) and len(set(hashes)) == 1,
        }

    o0 = profiles["O0"]["median_seconds"]
    o2 = profiles["O2"]["median_seconds"]
    if not isinstance(o0, float) or o0 > float(contract["o0_max_median_seconds"]):
        failures.append(f"O0 median {o0} exceeds {contract['o0_max_median_seconds']} seconds")
    if isinstance(o0, float) and isinstance(o2, float) and o0 > o2 * float(contract["o0_max_ratio_to_o2"]):
        failures.append(f"O0 median {o0} is not the required fast path relative to O2 {o2}")
    profile_hashes = [profiles[name]["sha256"] for name in contract["profiles"]]
    if contract["require_distinct_profile_objects"] and len(set(profile_hashes)) != len(profile_hashes):
        failures.append("O0/O1/O2/O3 did not produce four distinct object contracts")

    names = contract["ctests"]
    expression = "^(" + "|".join(re.escape(name) for name in names) + ")$"
    result, ctest_seconds = run([str(args.ctest), "--test-dir", str(args.build_dir), "-R", expression,
                                 "--output-on-failure", "-j1"], root)
    if result.returncode != 0:
        failures.append(f"performance CTest matrix: {result.stdout}{result.stderr}")

    report = {
        "schema": "nativesharp-selfhost-performance-report",
        "version": 1,
        "gate": "SH50",
        "tanda": 52,
        "status": "passed" if not failures else "failed",
        "compiler_sha256": digest(args.compiler),
        "contract_sha256": digest(args.contract),
        "profiles": profiles,
        "o0_fast_path": not failures and isinstance(o0, float) and isinstance(o2, float),
        "ctests": {"expected": len(names), "passed": len(names) if result.returncode == 0 else 0,
                    "seconds": round(ctest_seconds, 6), "names": names},
        "failures": failures,
    }
    report_path = args.report or output / "sh50-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    if failures:
        for failure in failures:
            print(f"NSH5002: {failure}", file=sys.stderr)
        return 1
    print(f"SH50 performance passed: O0 median={o0:.3f}s, O2 median={o2:.3f}s; {len(names)}/{len(names)} build tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
