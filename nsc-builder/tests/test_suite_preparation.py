from __future__ import annotations

import subprocess
import json
import sys
import tempfile
import unittest
from dataclasses import asdict
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder.build import plan_test_producers, producer_preparation_commands
from nsc_builder.catalog import CanonicalCatalog
from nsc_builder.execution import (
    ExecutionEngine,
    isolate_missing_llvm_discovery,
    validate_missing_llvm_failure,
)
from nsc_builder.models import (
    PreparationCommand,
    PreparationFailure,
    ProducerPlan,
    ProducerPreparation,
    TestExecution,
    TestCase,
    TestProducer,
    TestVariant,
)
from nsc_builder.history import EvidenceStore, history_record, load_history_execution
from nsc_builder.reporting import condensed_error_report


def make_case(case_id: str, executor: str = "nsc_component_tests") -> TestCase:
    return TestCase(
        case_id=case_id,
        contract_id="CI-PREP-001",
        component="Host.Test",
        wave=1,
        purpose="test producer preparation",
        layer="component",
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        executor=executor,
        disposition="Executable",
        objective="prepare once",
        expected_outcome="",
        manifest={},
    )


class FakeStore:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.events: list[tuple[str, object]] = []

    def event(self, kind: str, payload: object) -> None:
        self.events.append((kind, payload))

    def log(self, _line: str) -> None:
        pass


class SuitePreparationTests(unittest.TestCase):
    def test_planning_deduplicates_and_derives_future_host_targets(self) -> None:
        config = SimpleNamespace(build_root="A:/synthetic-tests")
        variants = (
            TestVariant(make_case("TP-001", "nsc_component_tests_custom"), "Debug", "NA"),
            TestVariant(make_case("TP-002", "nsc_component_tests_custom"), "Debug", "NA"),
            TestVariant(make_case("TP-003", "nsc_suite_controller+nsc"), "Debug", "NA"),
        )

        plans = plan_test_producers(config, variants)

        self.assertEqual(len(plans), 1)
        self.assertEqual(plans[0].producer.target, "nsc_component_tests_custom")
        self.assertEqual(len(plans[0].consumer_keys), 2)

    def test_build_commands_are_sequential_and_contain_each_target_once(self) -> None:
        config = SimpleNamespace(
            build_root="A:/synthetic-tests",
            jobs=8,
            llvm_root_path=lambda: Path("C:/clang-llvm"),
        )
        plans = (
            ProducerPlan(
                TestProducer("host-a", "host-a", "host-a.exe"),
                "Debug",
                Path("A:/synthetic-tests/debug/host-a.exe"),
                ("a",),
            ),
            ProducerPlan(
                TestProducer("host-b", "host-b", "host-b.exe"),
                "Debug",
                Path("A:/synthetic-tests/debug/host-b.exe"),
                ("b",),
            ),
        )
        with (
            patch(
                "nsc_builder.build.discover_build_tools",
                return_value={"cmake": "cmake"},
            ),
            patch("nsc_builder.build.test_build_tree_is_configured", return_value=True),
        ):
            commands = producer_preparation_commands(config, plans)

        self.assertEqual(len(commands), 2)
        self.assertEqual([item.stage for item in commands], ["build-producer"] * 2)
        self.assertEqual([item.producer_keys for item in commands], [("host-a|Debug",), ("host-b|Debug",)])
        self.assertEqual(commands[0].command.count("host-a"), 1)
        self.assertEqual(commands[1].command.count("host-b"), 1)

    def test_failed_preparation_prevents_every_dispatch_without_not_run_rows(self) -> None:
        variants = (
            TestVariant(make_case("TP-006"), "Debug", "NA"),
            TestVariant(make_case("TP-007"), "Debug", "NA"),
        )
        preparation = PreparationFailure(
            status="PREPARATION_FAILED",
            stage="build-producer",
            detail="single producer cause",
            evidence="A:/evidence/producer.json",
            producer_key="nsc_component_tests|Debug",
        )
        with tempfile.TemporaryDirectory() as directory:
            engine = ExecutionEngine(
                SimpleNamespace(),
                FakeStore(Path(directory)),
                lambda _line: None,
                lambda *_args: None,
                lambda *_args: None,
            )
            with (
                patch.object(engine, "_prepare_producers", return_value=((), preparation)),
                patch.object(engine, "_run_controlled_variant") as dispatch,
            ):
                outcomes, cancelled = engine.run(variants)

        dispatch.assert_not_called()
        self.assertFalse(cancelled)
        self.assertEqual(outcomes, [])
        self.assertEqual(engine.preparation_failure, preparation)
        self.assertNotIn("CASE_STARTED", [kind for kind, _ in engine.store.events])

    def test_compiler_precondition_failure_is_a_single_run_cause(self) -> None:
        variant = TestVariant(make_case("TP-004"), "Debug", "NA")
        with tempfile.TemporaryDirectory() as directory:
            store = FakeStore(Path(directory))
            engine = ExecutionEngine(
                SimpleNamespace(),
                store,
                lambda _line: None,
                lambda *_args: None,
                lambda *_args: None,
            )
            with (
                patch(
                    "nsc_builder.execution.compiler_receipt",
                    side_effect=FileNotFoundError("missing compiler"),
                ),
                patch.object(engine, "_run_controlled_variant") as dispatch,
            ):
                outcomes, cancelled = engine.run((variant,))

        self.assertEqual(outcomes, [])
        self.assertFalse(cancelled)
        dispatch.assert_not_called()
        self.assertEqual(engine.preparation_failure.stage, "compiler-precondition")
        self.assertEqual(
            [kind for kind, _ in store.events].count("PREPARATION_FAILED"), 1
        )

    def test_missing_executable_closes_the_global_artifact_barrier(self) -> None:
        variant = TestVariant(make_case("TP-005"), "Debug", "NA")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            missing = root / "nsc_component_tests.exe"
            plan = ProducerPlan(
                producer=TestProducer(
                    "nsc_component_tests",
                    "nsc_component_tests",
                    "nsc_component_tests.exe",
                ),
                build_mode="Debug",
                executable=missing,
                consumer_keys=(variant.key,),
            )
            engine = ExecutionEngine(
                SimpleNamespace(),
                FakeStore(root),
                lambda _line: None,
                lambda *_args: None,
                lambda *_args: None,
            )
            with (
                patch("nsc_builder.execution.compiler_receipt", return_value={"ok": True}),
                patch("nsc_builder.execution.plan_test_producers", return_value=(plan,)),
                patch("nsc_builder.execution.producer_preparation_commands", return_value=[]),
                patch("nsc_builder.execution.component_test_executable", return_value=missing),
            ):
                results, failure = engine._prepare_producers((variant,))

        self.assertEqual(results, ())
        self.assertIsNotNone(failure)
        self.assertEqual(failure.status, "PREPARATION_FAILED")
        self.assertEqual(failure.stage, "validate-artifacts")
        self.assertIn("sin ejecutable válido", failure.detail)

    def test_configure_or_build_failure_stops_at_first_command(self) -> None:
        variant = TestVariant(make_case("TP-008"), "Debug", "NA")
        plan = ProducerPlan(
            TestProducer(
                "nsc_component_tests",
                "nsc_component_tests",
                "nsc_component_tests.exe",
            ),
            "Debug",
            Path("A:/missing/nsc_component_tests.exe"),
            (variant.key,),
        )
        for stage in ("configure-tests", "build-producer"):
            with self.subTest(stage=stage), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                store = FakeStore(root)
                engine = ExecutionEngine(
                    SimpleNamespace(),
                    store,
                    lambda _line: None,
                    lambda *_args: None,
                    lambda *_args: None,
                )
                commands = (
                    PreparationCommand(
                        stage=stage,
                        label=stage,
                        command=("cmake", "--synthetic-failure"),
                        cwd=root,
                        producer_keys=(plan.key,),
                    ),
                    PreparationCommand(
                        stage="build-producer",
                        label="must-not-run",
                        command=("cmake", "--must-not-run"),
                        cwd=root,
                        producer_keys=(plan.key,),
                    ),
                )
                with (
                    patch("nsc_builder.execution.compiler_receipt", return_value={"ok": True}),
                    patch("nsc_builder.execution.plan_test_producers", return_value=(plan,)),
                    patch(
                        "nsc_builder.execution.producer_preparation_commands",
                        return_value=list(commands),
                    ),
                    patch.object(engine.process, "run", return_value=(1, "first cause")) as run,
                    patch.object(engine, "_run_controlled_variant") as dispatch,
                ):
                    outcomes, cancelled = engine.run((variant,))

                self.assertEqual(outcomes, [])
                self.assertFalse(cancelled)
                self.assertEqual(run.call_count, 1)
                dispatch.assert_not_called()
                self.assertEqual(engine.preparation_failure.stage, stage)
                self.assertNotIn("CASE_STARTED", [kind for kind, _ in store.events])

    def test_successful_barrier_finishes_before_first_case_started(self) -> None:
        variant = TestVariant(make_case("TP-009"), "Debug", "NA")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "nsc_component_tests.exe"
            executable.write_bytes(b"producer")
            plan = ProducerPlan(
                TestProducer(
                    "nsc_component_tests",
                    "nsc_component_tests",
                    "nsc_component_tests.exe",
                ),
                "Debug",
                executable,
                (variant.key,),
            )
            store = FakeStore(root)
            engine = ExecutionEngine(
                SimpleNamespace(),
                store,
                lambda _line: None,
                lambda *_args: None,
                lambda *_args: None,
            )
            outcome = TestExecution(
                key=variant.key,
                case_id=variant.case.case_id,
                contract_id=variant.case.contract_id,
                component=variant.case.component,
                wave=variant.case.wave,
                build_mode=variant.build_mode,
                optimization=variant.optimization,
                status="PASS",
            )
            with (
                patch("nsc_builder.execution.compiler_receipt", return_value={"ok": True}),
                patch("nsc_builder.execution.plan_test_producers", return_value=(plan,)),
                patch("nsc_builder.execution.producer_preparation_commands", return_value=[]),
                patch("nsc_builder.execution.component_test_executable", return_value=executable),
                patch.object(engine, "_run_controlled_variant", return_value=outcome),
            ):
                outcomes, cancelled = engine.run((variant,))

        kinds = [kind for kind, _ in store.events]
        self.assertFalse(cancelled)
        self.assertEqual(outcomes, [outcome])
        self.assertLess(kinds.index("PREPARATION_FINISHED"), kinds.index("CASE_STARTED"))

    def test_preparation_cancellation_has_no_case_outcomes(self) -> None:
        variant = TestVariant(make_case("TP-010"), "Debug", "NA")
        with tempfile.TemporaryDirectory() as directory:
            store = FakeStore(Path(directory))
            engine = ExecutionEngine(
                SimpleNamespace(),
                store,
                lambda _line: None,
                lambda *_args: None,
                lambda *_args: None,
            )
            engine.process.cancelled.set()
            with (
                patch("nsc_builder.execution.compiler_receipt", return_value={"ok": True}),
                patch.object(engine, "_run_controlled_variant") as dispatch,
            ):
                outcomes, cancelled = engine.run((variant,))

        self.assertEqual(outcomes, [])
        self.assertTrue(cancelled)
        dispatch.assert_not_called()
        self.assertEqual(engine.preparation_failure.status, "CANCELLED")

    def test_preparation_failure_round_trips_without_case_rows(self) -> None:
        variant = TestVariant(make_case("TP-011"), "Debug", "NA")
        metadata = {
            "schema": 1,
            "kind": "phase-2-suite",
            "run_id": "synthetic-preparation-run",
            "variant_count": 1,
            "case_count": 1,
            "selection": {
                "scope": "Caso",
                "value": variant.case.case_id,
                "profile": "Todos",
                "build_mode": "Debug",
                "optimization": "O0",
            },
        }
        failure = PreparationFailure(
            status="PREPARATION_FAILED",
            stage="build-producer",
            detail="producer failed",
            evidence="producer.log",
            command=("cmake", "--build"),
            producer_key="nsc_component_tests|Debug",
        )
        with tempfile.TemporaryDirectory() as directory:
            history_root = Path(directory)
            with patch("nsc_builder.history.HISTORY_ROOT", history_root):
                store = EvidenceStore(metadata["run_id"], metadata)
                store.event("PREPARATION_FAILED", asdict(failure))
                store.finish([], False, failure)
                root = store.root
                self.assertEqual(history_record(root)[2], "PREPARATION_FAILED")
                payload = json.loads((root / "results.json").read_text(encoding="utf-8"))
                self.assertEqual(payload["results"], [])
                self.assertEqual(payload["counts"], {})
                catalog = SimpleNamespace(
                    select=lambda *_args: (variant.case,),
                    by_id={variant.case.case_id: variant.case},
                )
                with patch(
                    "nsc_builder.history.expand_variants", return_value=(variant,)
                ):
                    loaded = load_history_execution(root, catalog)

        self.assertEqual(loaded.variants, (variant,))
        self.assertEqual(loaded.outcomes, ())
        self.assertEqual(loaded.preparation, failure)

        report = condensed_error_report([], False, str(root), failure)
        self.assertIn("PREPARACIÓN PREPARATION_FAILED", report)
        self.assertIn("etapa build-producer", report)
        self.assertIn("producer failed", report)

    def test_catalog_has_no_known_product_defects_after_part_a(self) -> None:
        catalog = CanonicalCatalog.load()
        known_product_defects = {
            record.case_id
            for record in catalog.cases
            if record.expected_outcome == "KNOWN_FAILURE"
        }
        self.assertEqual(known_product_defects, set())

    def test_00453_disables_all_inherited_llvm_discovery(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package_root = Path(directory) / "empty-package-root"
            command, environment = isolate_missing_llvm_discovery(
                "T12B-00453",
                [
                    "cmake",
                    "-S",
                    "A:/repo",
                    "-B",
                    "A:/work",
                    "-DLLVM_DIR=C:/clang-llvm/lib/cmake/llvm",
                    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=TRUE",
                ],
                {
                    "PATH": "C:/clang-llvm/bin",
                    "LLVM_DIR": "C:/clang-llvm/lib/cmake/llvm",
                    "CMAKE_PREFIX_PATH": "C:/clang-llvm",
                },
                package_root,
            )
            self.assertTrue(package_root.is_dir())
            self.assertEqual(list(package_root.iterdir()), [])
            self.assertIn(
                f"-DCMAKE_FIND_ROOT_PATH:PATH={package_root.resolve().as_posix()}",
                command,
            )
            self.assertIn("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE:STRING=ONLY", command)
            self.assertIn("-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=FALSE", command)
            self.assertIn("-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=FALSE", command)
            self.assertIn("-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH:BOOL=FALSE", command)
            self.assertIn("-DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH:BOOL=FALSE", command)
            self.assertIn("-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH:BOOL=FALSE", command)
            self.assertNotIn("-DLLVM_DIR=C:/clang-llvm/lib/cmake/llvm", command)
            self.assertNotIn("-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=TRUE", command)
            self.assertNotIn("LLVM_DIR", environment)
            self.assertNotIn("CMAKE_PREFIX_PATH", environment)
            self.assertIn("PATH", environment)

    def test_00453_accepts_only_the_canonical_missing_llvm_failure(self) -> None:
        missing = subprocess.CompletedProcess(
            ["cmake", "-S", "A:/repo"],
            1,
            "",
            "CMake Error: NativeSharp requires the LLVM 23 CMake package and its "
            "static component\nlibraries",
        )
        unrelated = subprocess.CompletedProcess(
            ["cmake", "-S", "A:/repo"],
            1,
            "",
            "CMake Error: source directory does not exist",
        )
        success = subprocess.CompletedProcess(
            ["cmake", "-S", "A:/repo"], 0, "configured", ""
        )

        self.assertEqual(validate_missing_llvm_failure((missing,)), (True, ""))
        for invocations in ((unrelated,), (success,), (), (missing, missing)):
            with self.subTest(invocations=len(invocations)):
                accepted, detail = validate_missing_llvm_failure(invocations)
                self.assertFalse(accepted)
                self.assertTrue(detail)

    def test_other_llvm_cases_keep_the_locked_toolchain(self) -> None:
        original = ["cmake", "-DLLVM_DIR=C:/clang-llvm/lib/cmake/llvm"]
        command, environment = isolate_missing_llvm_discovery(
            "T12B-00451", original, {"LLVM_DIR": "C:/clang-llvm/lib/cmake/llvm"}
        )

        self.assertEqual(command, original)
        self.assertIn("LLVM_DIR", environment)


if __name__ == "__main__":
    unittest.main()
