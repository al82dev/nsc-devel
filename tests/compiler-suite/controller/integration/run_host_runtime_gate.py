from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


EXECUTABLE_CONTRACTS = tuple(range(1, 11))


def expected_ids() -> list[str]:
    identifiers: list[str] = []
    for contract in EXECUTABLE_CONTRACTS:
        first = ((contract - 1) * 8) + 1
        identifiers.extend(
            f"TF-RUNTIME-{ordinal:03d}" for ordinal in range(first, first + 8)
        )
    return identifiers


EXPECTED_IDS = expected_ids()


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
    runtime_manifests = [
        case for case in manifests if case["component"] == "Host.Runtime"
    ]
    if [case["case_id"] for case in runtime_manifests] != EXPECTED_IDS:
        raise RuntimeError(
            "fragment does not contain exactly the 80 executable Host.Runtime "
            "cases in canonical order"
        )
    inventory = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    with tempfile.TemporaryDirectory(
        prefix="host-runtime-", dir=options.work_parent
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
                "host-runtime",
                "--revision",
                "phase-2-host-runtime",
                "--build-mode",
                options.build_mode,
                "--component",
                "Host.Runtime",
                "--profile",
                "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"Host.Runtime controller returned {code}")
        payload = json.loads(
            (root / "host-runtime" / "results.json").read_text(encoding="utf-8")
        )
        result_ids = [result["case_id"] for result in payload["results"]]
        if result_ids != EXPECTED_IDS:
            raise RuntimeError(
                "controller did not execute exactly the 80 executable "
                "Host.Runtime cases in canonical order"
            )
        counts = payload["counts"]
        unexpected = any(
            count != 0 for status, count in counts.items() if status != "PASS"
        )
        if counts["PASS"] != len(EXPECTED_IDS) or unexpected:
            raise RuntimeError(f"unexpected Host.Runtime result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
