from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
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
    ProducerPlan,
    ProducerPreparation,
    TestCase,
    TestProducer,
    TestVariant,
)


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

    def test_producer_failure_closes_consumer_as_not_run(self) -> None:
        variant = TestVariant(make_case("TP-004"), "Debug", "NA")
        preparation = ProducerPreparation(
            key="nsc_component_tests|Debug",
            status="FAIL",
            detail="build command failed",
            executable=Path("A:/missing/nsc_component_tests.exe"),
            consumer_keys=(variant.key,),
            evidence="A:/evidence/producer.json",
        )

        outcome = ExecutionEngine._producer_blocked_outcome(variant, preparation)

        self.assertEqual(outcome.status, "NOT_RUN")
        self.assertIn("BLOQUEADO_POR productor", outcome.detail)
        self.assertEqual(outcome.evidence, preparation.evidence)

    def test_grouped_build_command_contains_each_target_once(self) -> None:
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

        self.assertEqual(len(commands), 1)
        command = commands[0][1]
        self.assertEqual(command.count("host-a"), 1)
        self.assertEqual(command.count("host-b"), 1)

    def test_failed_producer_prevents_dispatch_of_all_consumers(self) -> None:
        variants = (
            TestVariant(make_case("TP-006"), "Debug", "NA"),
            TestVariant(make_case("TP-007"), "Debug", "NA"),
        )
        preparation = ProducerPreparation(
            key="nsc_component_tests|Debug",
            status="FAIL",
            detail="single producer cause",
            executable=Path("A:/missing/nsc_component_tests.exe"),
            consumer_keys=tuple(variant.key for variant in variants),
            evidence="A:/evidence/producer.json",
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
                patch.object(engine, "_prepare_producers", return_value=(preparation,)),
                patch.object(engine, "_run_controlled_variant") as dispatch,
            ):
                outcomes, cancelled = engine.run(variants)

        dispatch.assert_not_called()
        self.assertFalse(cancelled)
        self.assertEqual([item.status for item in outcomes], ["NOT_RUN", "NOT_RUN"])
        self.assertEqual(len({item.detail for item in outcomes}), 1)

    def test_missing_executable_is_one_failed_producer(self) -> None:
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
                patch("nsc_builder.execution.plan_test_producers", return_value=(plan,)),
                patch("nsc_builder.execution.producer_preparation_commands", return_value=[]),
                patch("nsc_builder.execution.component_test_executable", return_value=missing),
            ):
                results = engine._prepare_producers((variant,))

        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].status, "FAIL")
        self.assertIn("sin ejecutable válido", results[0].detail)

    def test_five_tagged_cases_load_as_known_product_defects(self) -> None:
        catalog = CanonicalCatalog.load()
        expected = {
            "TSH-00903",
            "TSH-01011",
            "TSH-01023",
            "T12B-00021",
            "T12B-00022",
        }

        for case_id in expected:
            with self.subTest(case_id=case_id):
                record = catalog.by_id[case_id]
                self.assertEqual(record.expected_outcome, "KNOWN_FAILURE")
                self.assertEqual(record.disposition, "KnownProductDefect")

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
