from __future__ import annotations

import argparse
import json
import sys
import tempfile
from collections import Counter
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


EXPECTED_IDS = [f"TF-COMPILATION-{number:03d}" for number in range(1, 417)]
EXPECTED_PROFILES = {"PR": 364, "NIGHTLY": 52}
EXPECTED_LAYERS = {"unit": 328, "integration": 88}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)

    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    manifests = [
        json.loads(line)
        for line in manifest_path.read_text(encoding="utf-8").splitlines()
    ]
    compilation_manifests = [
        case for case in manifests if case["component"] == "Host.Compilation"
    ]
    if [case["case_id"] for case in compilation_manifests] != EXPECTED_IDS:
        raise RuntimeError(
            "manifest does not contain exactly TF-COMPILATION-001..416 "
            "in canonical order"
        )
    if Counter(case["profile"] for case in compilation_manifests) != EXPECTED_PROFILES:
        raise RuntimeError("Host.Compilation manifest profile distribution is not canonical")
    if Counter(case["layer"] for case in compilation_manifests) != EXPECTED_LAYERS:
        raise RuntimeError("Host.Compilation manifest layer distribution is not canonical")

    inventory = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    with tempfile.TemporaryDirectory(
        prefix="host-compilation-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest",
                str(manifest_path),
                "--components",
                str(inventory / "components.csv"),
                "--contracts",
                str(inventory / "contracts.csv"),
                "--host-executable",
                str(options.host_executable),
                "--work-root",
                str(root),
                "--run-id",
                "host-compilation",
                "--revision",
                "phase-2-host-compilation",
                "--build-mode",
                options.build_mode,
                "--component",
                "Host.Compilation",
                "--profile",
                "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"Host.Compilation controller returned {code}")
        payload = json.loads(
            (root / "host-compilation" / "results.json").read_text(encoding="utf-8")
        )
        if [result["case_id"] for result in payload["results"]] != EXPECTED_IDS:
            raise RuntimeError(
                "controller did not execute exactly TF-COMPILATION-001..416 "
                "in canonical order"
            )
        counts = payload["counts"]
        unexpected = any(
            count != 0 for status, count in counts.items() if status != "PASS"
        )
        if counts["PASS"] != len(EXPECTED_IDS) or unexpected:
            raise RuntimeError(f"unexpected Host.Compilation result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
