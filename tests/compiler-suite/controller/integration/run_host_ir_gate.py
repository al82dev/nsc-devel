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


COMPONENTS = (
    "Host.HIR",
    "Host.CFG",
    "Host.Dominance",
    "Host.MIR",
    "Host.SSA",
    "Host.Passes",
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
    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"

    for component in COMPONENTS:
        component_manifests = component_manifest_cases(
            options.source_root,
            options.repository_root,
            component,
            "NIGHTLY",
            options.build_mode,
        )
        slug = component.removeprefix("Host.").lower()
        with tempfile.TemporaryDirectory(prefix=f"host-{slug}-", dir=options.work_parent) as temporary:
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
                    f"host-{slug}",
                    "--revision",
                    f"phase-2-host-{slug}",
                    "--build-mode",
                    options.build_mode,
                    "--component",
                    component,
                    "--profile",
                    "NIGHTLY",
                ]
            )
            if code != 0:
                raise RuntimeError(f"{component} controller returned {code}")
            payload = read_json_object(root / f"host-{slug}" / "results.json")
            assert_component_results(payload, component_manifests, component)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
