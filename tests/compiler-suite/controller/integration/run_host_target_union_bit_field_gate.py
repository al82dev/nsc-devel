from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(prefix="host-target-union-bit-field-", dir=options.work_parent) as temporary:
        root = Path(temporary)
        code = run([
            "--manifest", str(options.source_root / "manifest" / "cases.jsonl"),
            "--components", str(inventory / "components.csv"),
            "--contracts", str(inventory / "contracts.csv"),
            "--host-executable", str(options.host_executable),
            "--work-root", str(root),
            "--run-id", "host-target-union-bit-field",
            "--revision", "phase-2-host-target-union-bit-field",
            "--build-mode", options.build_mode,
            "--contract", "CF-TGT-020",
            "--profile", "NIGHTLY",
        ])
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads((root / "host-target-union-bit-field" / "results.json").read_text(encoding="utf-8"))
        expected_ids = [f"TF-TGT-{number:03d}" for number in range(153, 161)]
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError("controller did not execute the canonical TF-TGT-153..160 set")
        counts = payload["counts"]
        if counts["PASS"] != len(expected_ids) or any(count != 0 for status, count in counts.items() if status != "PASS"):
            raise RuntimeError(f"unexpected union-bit-field result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
