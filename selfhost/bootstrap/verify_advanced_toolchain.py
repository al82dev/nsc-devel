#!/usr/bin/env python3
"""SH49: certify advanced toolchain contracts around the self-hosted compiler."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
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
    timings: dict[str, float] = {}
    if contract.get("schema") != "nativesharp-selfhost-advanced-toolchain-contract" or contract.get("gate") != "SH49":
        print("NSH4901: unsupported SH49 contract", file=sys.stderr)
        return 1

    project = root / contract["project"]
    manifest_a = output / "advanced-a.manifest"
    manifest_b = output / "advanced-b.manifest"
    for name, destination in (("manifest-a", manifest_a), ("manifest-b", manifest_b)):
        result, elapsed = run([str(args.compiler), "--emit-toolchain-manifest", str(project), str(destination)], root)
        timings[name] = round(elapsed, 6)
        if result.returncode != 0:
            failures.append(f"{name}: exit={result.returncode}: {result.stdout}{result.stderr}")
    if manifest_a.exists() and manifest_b.exists():
        if manifest_a.read_bytes() != manifest_b.read_bytes():
            failures.append("toolchain manifests are not deterministic")
        text = manifest_a.read_text(encoding="utf-8")
        for key, value in contract["required_manifest"].items():
            if f"{key}={value}" not in text:
                failures.append(f"manifest missing {key}={value}")

    current = output / "current.nsmeta"
    result, elapsed = run([str(args.compiler), "--emit-nsmeta", str(project), str(current)], root)
    timings["metadata-current"] = round(elapsed, 6)
    if result.returncode != 0:
        failures.append(f"emit current metadata: {result.stdout}{result.stderr}")
    legacy = output / "legacy.nsmeta"
    legacy.write_text("NSMETA/1\nproducer=compat-fixture\nname=AdvancedToolchain\n", encoding="utf-8", newline="\n")
    corrupt = output / "corrupt.nsmeta"
    corrupt.write_bytes(b"NSMETA/3\nunsupported=true\n")
    metadata_results: dict[str, int] = {}
    for name, path, expected in (("current", current, 0), ("legacy", legacy, 0), ("corrupt", corrupt, 1)):
        result, elapsed = run([str(args.compiler), "--verify-nsmeta", str(path)], root)
        timings[f"verify-{name}"] = round(elapsed, 6)
        metadata_results[name] = result.returncode
        if result.returncode != expected:
            failures.append(f"metadata {name}: expected exit {expected}, got {result.returncode}")

    names = contract["ctests"]
    expression = "^(" + "|".join(re.escape(name) for name in names) + ")$"
    result, elapsed = run([str(args.ctest), "--test-dir", str(args.build_dir), "-R", expression,
                           "--output-on-failure", "-j1"], root)
    timings["advanced-ctests"] = round(elapsed, 6)
    if result.returncode != 0:
        failures.append(f"advanced CTest matrix: exit={result.returncode}: {result.stdout}{result.stderr}")
    elif f"{len(names)} tests passed" not in result.stdout and "100% tests passed" not in result.stdout:
        failures.append("advanced CTest matrix did not report complete success")

    report = {
        "schema": "nativesharp-selfhost-advanced-toolchain-report",
        "version": 1,
        "gate": "SH49",
        "tanda": 51,
        "status": "passed" if not failures else "failed",
        "compiler_sha256": digest(args.compiler),
        "contract_sha256": digest(args.contract),
        "manifest_sha256": digest(manifest_a) if manifest_a.exists() else None,
        "manifest_deterministic": manifest_a.exists() and manifest_b.exists() and manifest_a.read_bytes() == manifest_b.read_bytes(),
        "metadata_results": metadata_results,
        "ctests": {"expected": len(names), "passed": len(names) if result.returncode == 0 else 0, "names": names},
        "timings_seconds": timings,
        "failures": failures,
    }
    report_path = args.report or output / "sh49-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    if failures:
        for failure in failures:
            print(f"NSH4902: {failure}", file=sys.stderr)
        return 1
    print(f"SH49 advanced toolchain passed: {len(names)}/{len(names)} tests; metadata N-1/N; deterministic manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
