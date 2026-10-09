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
    with tempfile.TemporaryDirectory(
        prefix="host-basic-sha256-file-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(options.source_root / "manifest" / "cases.jsonl"),
                "--components", str(inventory / "components.csv"),
                "--contracts", str(inventory / "contracts.csv"),
                "--host-executable", str(options.host_executable),
                "--work-root", str(root),
                "--run-id", "host-basic-sha256-file",
                "--revision", "phase-2-host-basic-sha256-file",
                "--build-mode", options.build_mode,
                "--contract", "CB-BAS-008",
                "--profile", "WEEKLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads(
            (root / "host-basic-sha256-file" / "results.json").read_text(encoding="utf-8")
        )
        results = payload["results"]
        expected_ids = [f"TB-BAS-{index:03d}" for index in range(35, 41)]
        if [result["case_id"] for result in results] != expected_ids:
            raise RuntimeError("controller did not execute the canonical TB-BAS-035..040 set")
        counts = payload["counts"]
        if counts["PASS"] != len(expected_ids) or any(
            count != 0 for status, count in counts.items() if status != "PASS"
        ):
            raise RuntimeError(f"unexpected SHA-256 file result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
