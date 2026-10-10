from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


SELF_CONTAINED_CONTRACTS = frozenset({1, 7, 8, 9, 10, 11, 17, 18, 21})


def executable_ids() -> list[str]:
    ids: list[str] = []
    for contract in sorted(SELF_CONTAINED_CONTRACTS):
        first = (contract - 1) * 8 + 1
        ids.extend(f"TF-LLVM-{ordinal:03d}" for ordinal in range(first, first + 8))
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
    manifests = [json.loads(line) for line in manifest_path.read_text(encoding="utf-8").splitlines()]
    llvm_manifests = [case for case in manifests if case["component"] == "Host.LLVM"]
    expected_ids = executable_ids()
    if [case["case_id"] for case in llvm_manifests] != expected_ids:
        raise RuntimeError("manifest does not contain the complete executable Host.LLVM case set")

    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(prefix="host-llvm-", dir=options.work_parent) as temporary:
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
                "host-llvm",
                "--revision",
                "phase-2-host-llvm",
                "--build-mode",
                options.build_mode,
                "--component",
                "Host.LLVM",
                "--profile",
                "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"Host.LLVM controller returned {code}")
        payload = json.loads((root / "host-llvm" / "results.json").read_text(encoding="utf-8"))
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError("controller did not execute the complete executable Host.LLVM case set")
        counts = payload["counts"]
        if counts["PASS"] != len(expected_ids) or any(
            count != 0 for status, count in counts.items() if status != "PASS"
        ):
            raise RuntimeError(f"unexpected Host.LLVM result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
