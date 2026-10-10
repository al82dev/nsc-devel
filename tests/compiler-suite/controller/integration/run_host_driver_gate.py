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


EXECUTABLE_CONTRACTS = frozenset(
    [*range(1, 21), 24, *range(44, 55), 77]
)
EXPECTED_PROFILES = {"PR": 231, "NIGHTLY": 33}
EXPECTED_LAYERS = {"unit": 208, "integration": 56}


def executable_ids() -> list[str]:
    ids: list[str] = []
    for contract in sorted(EXECUTABLE_CONTRACTS):
        first = (contract - 1) * 8 + 1
        ids.extend(f"TF-DRIVER-{ordinal:03d}" for ordinal in range(first, first + 8))
    return ids


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
    driver_manifests = [
        case for case in manifests if case["component"] == "Host.Driver"
    ]
    expected_ids = executable_ids()
    if [case["case_id"] for case in driver_manifests] != expected_ids:
        raise RuntimeError(
            "manifest does not contain the complete executable Host.Driver case set"
        )
    if Counter(case["profile"] for case in driver_manifests) != EXPECTED_PROFILES:
        raise RuntimeError("Host.Driver manifest profile distribution is not canonical")
    if Counter(case["layer"] for case in driver_manifests) != EXPECTED_LAYERS:
        raise RuntimeError("Host.Driver manifest layer distribution is not canonical")

    inventory = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    with tempfile.TemporaryDirectory(
        prefix="host-driver-", dir=options.work_parent
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
                "host-driver",
                "--revision",
                "phase-2-host-driver",
                "--build-mode",
                options.build_mode,
                "--component",
                "Host.Driver",
                "--profile",
                "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"Host.Driver controller returned {code}")
        payload = json.loads(
            (root / "host-driver" / "results.json").read_text(encoding="utf-8")
        )
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError(
                "controller did not execute the complete executable Host.Driver case set"
            )
        counts = payload["counts"]
        unexpected = any(
            count != 0 for status, count in counts.items() if status != "PASS"
        )
        if counts["PASS"] != len(expected_ids) or unexpected:
            raise RuntimeError(f"unexpected Host.Driver result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
