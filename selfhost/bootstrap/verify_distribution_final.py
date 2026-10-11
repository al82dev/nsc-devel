#!/usr/bin/env python3
"""SH53: install, update, coexistence, rollback, migration and recovery."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--selfhost-compiler", type=Path, required=True)
    parser.add_argument("--bootstrap-compiler", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
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
    if (contract.get("schema") != "nativesharp-selfhost-distribution-final-contract" or
            contract.get("gate") != "SH53"):
        print("NSH5301: unsupported SH53 contract", file=sys.stderr)
        return 1

    prefix = output / "package"
    install_command = [str(args.cmake), "--install", str(args.build_dir),
                       "--config", "Release", "--prefix", str(prefix)]
    first_install = run(install_command, root)
    installed = prefix / "bin/nsc.exe"
    manifest = prefix / "share/nativesharp/toolchain/NativeSharpInstallation.manifest"
    if first_install.returncode != 0 or not installed.is_file() or not manifest.is_file():
        failures.append(f"fresh package install failed: {first_install.stdout}{first_install.stderr}")

    first_manifest_hash = digest(manifest) if manifest.is_file() else None
    verify = run([str(installed), "--verify-installation", str(prefix)], root) if installed.is_file() else None
    if verify is None or verify.returncode != 0:
        failures.append("fresh installation did not verify")

    second_install = run(install_command, root) if installed.is_file() else None
    if second_install is None or second_install.returncode != 0:
        failures.append("in-place update failed")
    if not manifest.is_file() or digest(manifest) != first_manifest_hash:
        failures.append("no-change update changed the canonical manifest")
    leaked_update_files = [name for name in (".nativesharp-previous.manifest", ".nativesharp-update.plan")
                           if (prefix / name).exists()]
    if leaked_update_files:
        failures.append(f"update transaction leaked files: {leaked_update_files}")

    coexist = output / "side-by-side"
    coexist.mkdir(exist_ok=True)
    nsc0 = coexist / "nsc0.exe"
    nsc2 = coexist / "nsc2.exe"
    shutil.copy2(args.bootstrap_compiler, nsc0)
    shutil.copy2(args.selfhost_compiler, nsc2)
    nsc0_version = run([str(nsc0), "--version"], root)
    nsc2_version = run([str(nsc2), "--version"], root)
    coexist_ok = (nsc0_version.returncode == 0 and nsc2_version.returncode == 0 and
                  nsc0.resolve() != nsc2.resolve() and digest(nsc0) != digest(nsc2))
    if not coexist_ok:
        failures.append("nsc0/nsc2 side-by-side selection is not explicit and executable")

    backup = output / "known-good"
    if prefix.is_dir():
        shutil.copytree(prefix, backup)
        with installed.open("ab") as stream:
            stream.write(b"SH53-TAMPER")
        rejected = run([str(installed), "--verify-installation-manifest", str(prefix), str(manifest)], root)
        if rejected.returncode == 0:
            failures.append("tampered update candidate was accepted")
        shutil.rmtree(prefix)
        shutil.copytree(backup, prefix)
        installed = prefix / "bin/nsc.exe"
        manifest = prefix / "share/nativesharp/toolchain/NativeSharpInstallation.manifest"
        rolled_back = run([str(installed), "--verify-installation-manifest", str(prefix), str(manifest)], root)
        if rolled_back.returncode != 0 or digest(manifest) != first_manifest_hash:
            failures.append("rollback did not restore the known-good package")
    else:
        rolled_back = None

    recovery = root / "selfhost/bootstrap/recovery.md"
    recovery_text = recovery.read_text(encoding="utf-8", errors="replace")
    missing_markers = [marker for marker in contract["recovery_markers"] if marker not in recovery_text]
    if missing_markers:
        failures.append(f"recovery recipe omits {missing_markers}")
    names = contract["ctests"]
    expression = "^(" + "|".join(re.escape(name) for name in names) + ")$"
    ctest = run([str(args.ctest), "--test-dir", str(args.build_dir), "-C", "Release",
                 "-R", expression, "--output-on-failure", "-j1"], root)
    if ctest.returncode != 0:
        failures.append(f"distribution CTest matrix: {ctest.stdout}{ctest.stderr}")

    report = {
        "schema": "nativesharp-selfhost-distribution-final-report",
        "version": 1,
        "gate": "SH53",
        "tanda": 55,
        "status": "passed" if not failures else "failed",
        "compiler_sha256": digest(args.compiler),
        "bootstrap_compiler_sha256": digest(args.bootstrap_compiler),
        "nslink_sha256": digest(args.nslink),
        "selfhost_compiler_sha256": digest(args.selfhost_compiler),
        "contract_sha256": digest(args.contract),
        "fresh_install": first_install.returncode == 0,
        "in_place_update": second_install is not None and second_install.returncode == 0,
        "manifest_sha256": first_manifest_hash,
        "update_transaction_leaks": leaked_update_files,
        "side_by_side": {"passed": coexist_ok, "nsc0_sha256": digest(nsc0), "nsc2_sha256": digest(nsc2)},
        "rollback": rolled_back is not None and rolled_back.returncode == 0,
        "ctests": {"expected": len(names), "passed": len(names) if ctest.returncode == 0 else 0,
                    "names": names},
        "failures": failures,
    }
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8", newline="\n")
    if failures:
        for failure in failures:
            print(f"NSH5302: {failure}", file=sys.stderr)
        return 1
    print(f"SH53 distribution passed: install/update/rollback, {len(names)}/{len(names)} compiler CTests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
