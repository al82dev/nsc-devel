#!/usr/bin/env python3
"""SH54 precertification; the cumulative baseline is executed by nscbuilder after this gate."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
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
    if (contract.get("schema") != "nativesharp-selfhost-final-certification-contract" or
            contract.get("gate") != "SH54"):
        print("NSH5401: unsupported SH54 contract", file=sys.stderr)
        return 1

    summaries: dict[str, dict[str, object]] = {}
    for gate in contract["required_gate_summaries"]:
        path = root / f"selfhost/evidence/gates/{gate}/{gate.lower()}-summary.json"
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exception:
            failures.append(f"unreadable {gate} summary: {exception}")
            continue
        summaries[gate] = value
        if value.get("status") not in ("passed", "LANDED") or value.get("failures", []) != []:
            failures.append(f"{gate} is not a clean prerequisite")

    version = run([str(args.compiler), "--version"], root)
    if version.returncode != 0 or "NativeSharp 2.0.0, stage 70" not in version.stdout:
        failures.append("host compiler is not the promoted NativeSharp 2.0 stage 70 line")
    selfhost_version = run([str(args.selfhost_compiler), "--version"], root)
    if selfhost_version.returncode != 0 or "NativeSharp 2.0.0" not in selfhost_version.stdout:
        failures.append("self-hosted compiler identity is not NativeSharp 2.0.0")

    cli_artifacts: dict[str, dict[str, object]] = {}
    for option in contract["required_cli_artifacts"]:
        result = run([str(args.compiler), option], root)
        path = output / (option.removeprefix("--print-") + ".txt")
        path.write_text(result.stdout, encoding="utf-8", newline="\n")
        if result.returncode != 0 or not result.stdout.strip() or result.stderr:
            failures.append(f"canonical artifact {option} failed")
        cli_artifacts[option] = {"sha256": digest(path), "bytes": path.stat().st_size}

    promotion = output / "NativeSharp2Promotion.act"
    write_act = run([str(args.compiler), "--write-promotion-act", str(promotion)], root)
    verify_act = run([str(args.compiler), "--verify-promotion-act", str(promotion)], root)
    if write_act.returncode != 0 or verify_act.returncode != 0:
        failures.append("official promotion act did not round-trip")

    docs = root / "docs/2.0/self-hosting/tandas"
    missing_tandas = [number for number in range(1, 56)
                      if not list(docs.glob(f"TANDA-{number:02d}-*.md"))]
    if missing_tandas:
        failures.append(f"missing closed tanda dossiers: {missing_tandas}")

    names = contract["ctests"]
    expression = "^(" + "|".join(re.escape(name) for name in names) + ")$"
    ctest = run([str(args.ctest), "--test-dir", str(args.build_dir), "-C", "Release",
                 "-R", expression, "--output-on-failure", "-j1"], root)
    if ctest.returncode != 0:
        failures.append(f"final contract CTest matrix: {ctest.stdout}{ctest.stderr}")

    report = {
        "schema": "nativesharp-selfhost-final-precertification-report",
        "version": 1,
        "gate": "SH54",
        "tanda": 56,
        "status": "passed" if not failures else "failed",
        "baseline_run_id": contract["baseline_run_id"],
        "baseline_build_id": contract["baseline_build_id"],
        "compiler_sha256": digest(args.compiler),
        "selfhost_compiler_sha256": digest(args.selfhost_compiler),
        "contract_sha256": digest(args.contract),
        "prerequisite_summaries": {gate: summaries[gate].get("status")
                                    for gate in summaries},
        "cli_artifacts": cli_artifacts,
        "promotion_act_sha256": digest(promotion) if promotion.is_file() else None,
        "closed_tanda_dossiers": 55 - len(missing_tandas),
        "ctests": {"expected": len(names), "passed": len(names) if ctest.returncode == 0 else 0,
                    "names": names},
        "failures": failures,
    }
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8", newline="\n")
    if failures:
        for failure in failures:
            print(f"NSH5402: {failure}", file=sys.stderr)
        return 1
    print(f"SH54 precertification passed: {len(summaries)} prerequisites, "
          f"{len(names)}/{len(names)} final contract CTests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
