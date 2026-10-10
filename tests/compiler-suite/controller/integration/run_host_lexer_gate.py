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

    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    manifests = [json.loads(line) for line in manifest_path.read_text(encoding="utf-8").splitlines()]
    lexer_manifests = [case for case in manifests if case["component"] == "Host.Lexer"]
    expected_ids = [f"TL-LEX-{number:03d}" for number in range(1, 111)]
    if [case["case_id"] for case in lexer_manifests] != expected_ids:
        raise RuntimeError("manifest does not contain the complete ordered Host.Lexer case set")
    expected_known_failures = sum(
        case.get("expected_outcome", "PASS") == "KNOWN_FAILURE" for case in lexer_manifests
    )

    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(prefix="host-lexer-", dir=options.work_parent) as temporary:
        root = Path(temporary)
        code = run([
            "--manifest", str(manifest_path),
            "--components", str(inventory / "components.csv"),
            "--contracts", str(inventory / "contracts.csv"),
            "--host-executable", str(options.host_executable),
            "--work-root", str(root),
            "--run-id", "host-lexer",
            "--revision", "phase-2-host-lexer",
            "--build-mode", options.build_mode,
            "--component", "Host.Lexer",
            "--profile", "NIGHTLY",
        ])
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads((root / "host-lexer" / "results.json").read_text(encoding="utf-8"))
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError("controller did not execute the complete ordered Host.Lexer case set")
        counts = payload["counts"]
        allowed = {"PASS", "KNOWN_FAILURE"}
        unexpected = any(count != 0 for status, count in counts.items() if status not in allowed)
        expected_passes = len(expected_ids) - expected_known_failures
        if (counts["PASS"] != expected_passes or
                counts["KNOWN_FAILURE"] != expected_known_failures or unexpected):
            raise RuntimeError(f"unexpected Host.Lexer result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
