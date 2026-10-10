from __future__ import annotations

import argparse
import json
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


@dataclass(frozen=True)
class ComponentGate:
    component: str
    prefix: str
    total: int
    special_infrastructure: frozenset[str] = frozenset()
    known_product_defects: frozenset[str] = frozenset()

    def executable_ids(self) -> list[str]:
        return [
            f"{self.prefix}-{number:03d}"
            for number in range(1, self.total + 1)
            if f"{self.prefix}-{number:03d}" not in self.special_infrastructure
        ]


COMPONENTS = (
    ComponentGate("Host.HIR", "TF-HIR", 232),
    ComponentGate(
        "Host.CFG",
        "TF-CFG",
        208,
        frozenset({"TF-CFG-003", "TF-CFG-004", "TF-CFG-007", "TF-CFG-008"}),
        frozenset({"TF-CFG-076"}),
    ),
    ComponentGate(
        "Host.Dominance",
        "TF-DOM",
        256,
        frozenset(
            {
                "TF-DOM-003",
                "TF-DOM-008",
                "TF-DOM-068",
                "TF-DOM-076",
                "TF-DOM-132",
                "TF-DOM-140",
                "TF-DOM-172",
            }
        ),
        frozenset({"TF-DOM-142"}),
    ),
    ComponentGate("Host.MIR", "TF-MIR", 304),
    ComponentGate("Host.SSA", "TF-SSA", 336),
    ComponentGate(
        "Host.Passes",
        "TF-PAS",
        448,
        frozenset(
            {
                "TF-PAS-059",
                "TF-PAS-060",
                "TF-PAS-063",
                "TF-PAS-064",
                "TF-PAS-398",
                "TF-PAS-400",
            }
        ),
    ),
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
    manifests = [json.loads(line) for line in manifest_path.read_text(encoding="utf-8").splitlines()]
    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"

    for gate in COMPONENTS:
        expected_ids = gate.executable_ids()
        component_manifests = [case for case in manifests if case["component"] == gate.component]
        if [case["case_id"] for case in component_manifests] != expected_ids:
            raise RuntimeError(f"manifest does not contain the complete executable {gate.component} case set")

        slug = gate.component.removeprefix("Host.").lower()
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
                    gate.component,
                    "--profile",
                    "NIGHTLY",
                ]
            )
            if code != 0:
                raise RuntimeError(f"{gate.component} controller returned {code}")
            payload = json.loads(
                (root / f"host-{slug}" / "results.json").read_text(encoding="utf-8")
            )
            if [result["case_id"] for result in payload["results"]] != expected_ids:
                raise RuntimeError(f"controller did not execute the complete {gate.component} case set")
            counts = payload["counts"]
            unexpected = any(
                count != 0
                for status, count in counts.items()
                if status not in {"PASS", "KNOWN_FAILURE"}
            )
            if (
                counts["PASS"] != len(expected_ids) - len(gate.known_product_defects)
                or counts["KNOWN_FAILURE"] != len(gate.known_product_defects)
                or unexpected
            ):
                raise RuntimeError(f"unexpected {gate.component} result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
