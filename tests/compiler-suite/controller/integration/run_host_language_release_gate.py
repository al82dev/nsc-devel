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
    with tempfile.TemporaryDirectory(prefix="host-language-release-", dir=options.work_parent) as temporary:
        root = Path(temporary)
        code = run([
            "--manifest", str(options.source_root / "manifest" / "cases.jsonl"),
            "--components", str(inventory / "components.csv"),
            "--contracts", str(inventory / "contracts.csv"),
            "--host-executable", str(options.host_executable),
            "--work-root", str(root),
            "--run-id", "host-language-release",
            "--revision", "phase-2-host-language-release",
            "--build-mode", options.build_mode,
            "--contract", "CL-LANG-018",
            "--profile", "PR",
        ])
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads(
            (root / "host-language-release" / "results.json").read_text(encoding="utf-8")
        )
        expected_ids = [f"TL-LANG-{number:03d}" for number in range(231, 249)]
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError("controller did not execute the complete CL-LANG-018 case set")
        counts = payload["counts"]
        unexpected = any(count != 0 for status, count in counts.items() if status != "PASS")
        if counts["PASS"] != len(expected_ids) or unexpected:
            raise RuntimeError(f"unexpected release-semantics result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
