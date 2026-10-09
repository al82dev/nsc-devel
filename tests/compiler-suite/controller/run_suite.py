from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite import (
    CancellationToken,
    CompilerIdentityExecutor,
    EventWriter,
    ExecutorRouter,
    HostExecutor,
    InventoryIndex,
    ReportWriter,
    RunMetadata,
    Selection,
    SuiteRunner,
    ToolIdentity,
    WorkspaceManager,
    load_manifest,
    select_cases,
)


def parse_arguments(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="NativeSharp compiler suite controller")
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--components", type=Path, required=True)
    parser.add_argument("--contracts", type=Path, required=True)
    parser.add_argument("--host-executable", type=Path)
    parser.add_argument("--nsc-executable", type=Path)
    parser.add_argument("--comparison-nsc-executable", type=Path)
    parser.add_argument("--build-id-header", type=Path)
    parser.add_argument("--work-root", type=Path, required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--optimization", choices=("NA", "O0", "O1", "O2", "O3"))
    parser.add_argument("--profile", choices=("PR", "NIGHTLY", "WEEKLY"))
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--contract", action="append", default=[])
    parser.add_argument("--component", action="append", default=[])
    parser.add_argument("--layer", action="append", default=[])
    parser.add_argument("--tag", action="append", default=[])
    parser.add_argument("--all", action="store_true")
    return parser.parse_args(arguments)


def run(arguments: list[str]) -> int:
    options = parse_arguments(arguments)
    inventory = InventoryIndex.from_csv(options.components, options.contracts)
    cases = load_manifest(options.manifest, inventory)
    selection = Selection(
        all_cases=options.all,
        case_ids=frozenset(options.case),
        contract_ids=frozenset(options.contract),
        components=frozenset(options.component),
        layers=frozenset(options.layer),
        profile=options.profile,
        build_mode=options.build_mode,
        optimization=options.optimization,
        tags=frozenset(options.tag),
    )
    selected = select_cases(cases, selection)
    supported_executors = {"nsc_component_tests", "nsc_suite_controller+nsc"}
    unsupported = tuple(
        case.case_id for case in selected if case.executor not in supported_executors
    )
    if unsupported:
        raise ValueError(
            "selected cases require executors not implemented by this controller increment: "
            + ", ".join(unsupported)
        )

    required_executors = {case.executor for case in selected}
    executors = {}
    tools: list[ToolIdentity] = []
    if "nsc_component_tests" in required_executors:
        host = _required_file(options.host_executable, "host executable")
        executors["nsc_component_tests"] = HostExecutor((str(host),))
        tools.append(ToolIdentity("nsc_component_tests", str(host), _sha256(host)))
    if "nsc_suite_controller+nsc" in required_executors:
        nsc = _required_file(options.nsc_executable, "NSC executable")
        comparison = _optional_file(
            options.comparison_nsc_executable, "comparison NSC executable"
        )
        header = _optional_file(options.build_id_header, "generated build identity header")
        executors["nsc_suite_controller+nsc"] = CompilerIdentityExecutor(
            (str(nsc),),
            (str(comparison),) if comparison is not None else None,
            header,
        )
        tools.append(ToolIdentity("nsc", str(nsc), _sha256(nsc)))
        if comparison is not None:
            tools.append(ToolIdentity("comparison-nsc", str(comparison), _sha256(comparison)))
        if header is not None:
            tools.append(ToolIdentity("build-id-header", str(header), _sha256(header)))

    work_root = options.work_root.resolve()
    event_path = work_root / options.run_id / "events.jsonl"
    cancellation = CancellationToken()
    with EventWriter(event_path) as events:
        runner = SuiteRunner(WorkspaceManager(work_root), events, cancellation)
        results = runner.run(options.run_id, selected, ExecutorRouter(executors))
    metadata = RunMetadata(
        run_id=options.run_id,
        revision=options.revision,
        build_mode=options.build_mode,
        optimization=options.optimization or "NA",
        profile=options.profile or "explicit",
        scope="all" if options.all else "focused",
        selected_case_ids=tuple(case.case_id for case in selected),
        tools=tuple(tools),
    )
    ReportWriter(work_root / options.run_id).write(metadata, results)
    return 0 if all(result.status.value == "PASS" for result in results) else 1


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def _required_file(path: Path | None, label: str) -> Path:
    if path is None:
        raise ValueError(f"{label} is required by the selected cases")
    resolved = path.resolve()
    if not resolved.is_file():
        raise ValueError(f"{label} does not exist: {resolved}")
    return resolved


def _optional_file(path: Path | None, label: str) -> Path | None:
    if path is None:
        return None
    resolved = path.resolve()
    if not resolved.is_file():
        raise ValueError(f"{label} does not exist: {resolved}")
    return resolved


def main() -> int:
    try:
        return run(sys.argv[1:])
    except Exception as error:
        print(f"nsc_suite_controller: error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
