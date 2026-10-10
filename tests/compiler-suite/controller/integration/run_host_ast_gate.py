from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


SPECIAL_INFRASTRUCTURE_CASES = {
    "TA-AST-014",
    "TA-AST-016",
    "TA-AST-017",
    "TA-AST-215",
    "TA-AST-223",
    "TA-AST-224",
}


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
    ast_manifests = [case for case in manifests if case["component"] == "Host.AST"]
    expected_ids = [
        f"TA-AST-{number:03d}"
        for number in range(1, 231)
        if f"TA-AST-{number:03d}" not in SPECIAL_INFRASTRUCTURE_CASES
    ]
    if [case["case_id"] for case in ast_manifests] != expected_ids:
        raise RuntimeError("manifest does not contain the complete executable Host.AST case set")

    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(prefix="host-ast-", dir=options.work_parent) as temporary:
        root = Path(temporary)
        code = run([
            "--manifest", str(manifest_path),
            "--components", str(inventory / "components.csv"),
            "--contracts", str(inventory / "contracts.csv"),
            "--host-executable", str(options.host_executable),
            "--work-root", str(root),
            "--run-id", "host-ast",
            "--revision", "phase-2-host-ast",
            "--build-mode", options.build_mode,
            "--component", "Host.AST",
            "--profile", "NIGHTLY",
        ])
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads((root / "host-ast" / "results.json").read_text(encoding="utf-8"))
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError("controller did not execute the complete executable Host.AST case set")
        counts = payload["counts"]
        unexpected = any(count != 0 for status, count in counts.items() if status != "PASS")
        if counts["PASS"] != len(expected_ids) or unexpected:
            raise RuntimeError(f"unexpected Host.AST result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
