#!/usr/bin/env python3
"""Run consolidated direct cases and preserved multi-step verification suites."""

from __future__ import annotations

import argparse
import sys
from datetime import datetime, timezone
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parent
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.catalog import CatalogError, TestCatalog  # noqa: E402
from nsc_test.command import NscCommandBuilder  # noqa: E402
from nsc_test.process import ProcessExecutor  # noqa: E402
from nsc_test.report import ReportWriter  # noqa: E402
from nsc_test.report import TestRunRecord  # noqa: E402
from nsc_test.suites import SuiteAdapter, SuiteCatalog, SuiteError  # noqa: E402
from nsc_test.verify import ResultVerifier  # noqa: E402
from nsc_test.workspace import CaseWorkspace  # noqa: E402


def existing_file(value: str) -> Path:
    path = Path(value).resolve()
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"file does not exist: {path}")
    return path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="NativeSharp compiler test harness")
    parser.add_argument("--nsc", required=True, type=existing_file)
    parser.add_argument("--nslink", type=existing_file)
    parser.add_argument(
        "--catalog",
        type=Path,
        default=HARNESS_ROOT.parent / "cases",
    )
    parser.add_argument("--optimization", choices=("O0",), default="O0")
    parser.add_argument("--work-root", type=Path, default=HARNESS_ROOT.parent.parent / "_test-harness" / "work")
    parser.add_argument("--results-root", type=Path, default=HARNESS_ROOT.parent.parent / "_test-harness" / "results")
    parser.add_argument("--run-id")
    parser.add_argument(
        "--matrix",
        type=Path,
        default=HARNESS_ROOT.parent.parent / "docs" / "compiler-optimization" / "test-infrastructure" / "test-destination-matrix.json",
    )
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--suite-prefix", type=Path)
    parser.add_argument("--suite-evidence-root", type=Path)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--list", action="store_true")
    selection.add_argument("--case")
    selection.add_argument("--component")
    selection.add_argument("--all", action="store_true")
    selection.add_argument("--list-suites", action="store_true")
    selection.add_argument("--suite")
    selection.add_argument("--suite-component")
    selection.add_argument("--all-suites", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)
    suite_mode = any(
        (
            arguments.list_suites,
            arguments.suite is not None,
            arguments.suite_component is not None,
            arguments.all_suites,
        )
    )
    if suite_mode:
        return run_suites(arguments, parser)

    try:
        catalog = TestCatalog.load(arguments.catalog)
        if arguments.list:
            selected = catalog.cases
        elif arguments.case is not None:
            selected = (catalog.require(arguments.case),)
        elif arguments.component is not None:
            selected = catalog.by_component(arguments.component)
            if not selected:
                raise CatalogError(f"component has no cases: {arguments.component}")
        else:
            selected = catalog.cases
    except CatalogError as error:
        parser.error(str(error))

    if arguments.list:
        for case in selected:
            print(f"{case.identifier}\t{case.component}\t{case.kind.value}")
        print(f"cases={len(selected)}")
        return 0

    builder = NscCommandBuilder(arguments.nsc, arguments.nslink)
    executor = ProcessExecutor()
    verifier = ResultVerifier()
    results = []
    for case in selected:
        workspace = CaseWorkspace.create(case, arguments.work_root)
        try:
            request = builder.build(case, workspace)
            process = executor.execute(request)
            result = verifier.verify(case, workspace, process)
            workspace.keep = not result.passed
            results.append(result)
        finally:
            workspace.cleanup()
    run_id = arguments.run_id or datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    output = ReportWriter(arguments.results_root).write_verified(
        run_id,
        arguments.nsc,
        tuple(results),
    )
    passed = sum(1 for result in results if result.passed)
    failed = len(results) - passed
    print(f"nsc={arguments.nsc}")
    if arguments.nslink is not None:
        print(f"nslink={arguments.nslink}")
    print(f"optimization={arguments.optimization}")
    print(f"selected={len(results)}")
    print(f"passed={passed}")
    print(f"failed={failed}")
    print(f"results={output}")
    return 0 if failed == 0 else 1


def run_suites(arguments: argparse.Namespace, parser: argparse.ArgumentParser) -> int:
    if arguments.build_dir is None:
        parser.error("--build-dir is required for specialized suites")
    if arguments.suite_prefix is None:
        parser.error("--suite-prefix is required for specialized suites")
    build_dir = arguments.build_dir.resolve()
    if not build_dir.is_dir():
        parser.error(f"build directory does not exist: {build_dir}")
    prefix = arguments.suite_prefix.resolve()
    if not (prefix / "lib").is_dir() or not (prefix / "bin").is_dir():
        parser.error(f"suite prefix must contain bin and lib: {prefix}")
    repo_root = HARNESS_ROOT.parent.parent.resolve()
    try:
        catalog = SuiteCatalog.load(arguments.matrix, repo_root)
        if arguments.list_suites:
            selected = catalog.suites
        elif arguments.suite is not None:
            selected = (catalog.require(arguments.suite),)
        elif arguments.suite_component is not None:
            selected = catalog.by_component(arguments.suite_component)
            if not selected:
                raise SuiteError(f"component has no specialized suites: {arguments.suite_component}")
        else:
            selected = catalog.runnable()
    except SuiteError as error:
        parser.error(str(error))

    if arguments.list_suites:
        for suite in selected:
            state = "runnable" if suite.runnable else f"blocked: {suite.blocked_reason}"
            print(f"{suite.identifier}\t{suite.component}\t{state}")
        print(f"suites={len(selected)}")
        print(f"runnable={sum(1 for suite in selected if suite.runnable)}")
        return 0

    blocked = tuple(suite for suite in selected if not suite.runnable)
    if blocked:
        details = "; ".join(f"{suite.identifier}: {suite.blocked_reason}" for suite in blocked)
        parser.error(f"selection includes suites blocked by scope: {details}")
    run_id = arguments.run_id or datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    evidence_root = (
        arguments.suite_evidence_root
        if arguments.suite_evidence_root is not None
        else arguments.work_root / "suites" / run_id
    )
    try:
        adapter = SuiteAdapter(
            nsc=arguments.nsc,
            nslink=arguments.nslink,
            repo_root=repo_root,
            build_dir=build_dir,
            prefix=prefix,
            evidence_root=evidence_root,
        )
        executor = ProcessExecutor()
        records = []
        for suite in selected:
            process = executor.execute(adapter.build(suite))
            records.append(
                TestRunRecord(
                    case_id=suite.identifier,
                    component=suite.component,
                    process=process,
                )
            )
        output = ReportWriter(arguments.results_root).write(
            run_id,
            arguments.nsc,
            tuple(records),
        )
    except (SuiteError, ValueError) as error:
        parser.error(str(error))
    passed = sum(
        1 for record in records if not record.process.timed_out and record.process.exit_code == 0
    )
    failed = len(records) - passed
    print(f"nsc={arguments.nsc}")
    if arguments.nslink is not None:
        print(f"nslink={arguments.nslink}")
    print("optimization=O0")
    print(f"selected={len(records)}")
    print(f"passed={passed}")
    print(f"failed={failed}")
    print(f"evidence={evidence_root.resolve()}")
    print(f"results={output}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
