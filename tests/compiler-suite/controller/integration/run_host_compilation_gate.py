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

    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    compilation_manifests = component_manifest_cases(
        options.source_root,
        options.repository_root,
        "Host.Compilation",
        "NIGHTLY",
        options.build_mode,
    )

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
        payload = read_json_object(root / "host-compilation" / "results.json")
        assert_component_results(
            payload,
            compilation_manifests,
            "Host.Compilation",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
