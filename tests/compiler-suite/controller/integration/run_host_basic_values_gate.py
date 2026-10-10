from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from run_suite import run
from component_gate_support import (
    assert_component_results,
    component_manifest_cases,
    read_json_object,
)


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
    manifest = options.source_root / "manifest" / "cases.jsonl"
    with tempfile.TemporaryDirectory(
        prefix="host-basic-values-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(manifest),
                "--components", str(inventory / "components.csv"),
                "--contracts", str(inventory / "contracts.csv"),
                "--host-executable", str(options.host_executable),
                "--work-root", str(root),
                "--run-id", "host-basic-values",
                "--revision", "phase-2-host-basic-values",
                "--build-mode", options.build_mode,
                "--component", "Host.Basic",
                "--profile", "PR",
            ]
        )
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = read_json_object(root / "host-basic-values" / "results.json")
        cases = component_manifest_cases(
            options.source_root,
            options.repository_root,
            "Host.Basic",
            "PR",
            options.build_mode,
        )
        assert_component_results(payload, cases, "Host.Basic PR")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
