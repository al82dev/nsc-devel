from __future__ import annotations

import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch


MODULAR_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = MODULAR_ROOT.parent
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder import baseline, deploy
from nsc_builder.configuration import BuilderConfig
from nsc_builder.gui import _smoke_catalog
from nsc_builder.models import CaseRecord
from nsc_builder.process_control import ProcessController
from nsc_builder.unit_tests import select_unit_cases


class ReleaseOperationPlanTests(unittest.TestCase):
    def config(self, root: Path) -> BuilderConfig:
        nslink = root / "tools" / "nslink.exe"
        nslink.parent.mkdir(parents=True)
        nslink.write_bytes(b"nslink")
        build = root / "build-clang"
        build.mkdir()
        (build / "cmake_install.cmake").write_text("# fixture\n", encoding="utf-8")
        return BuilderConfig(
            nsc_executable=str(build / "bin" / "nsc.exe"),
            nslink_executable=str(nslink),
            compiler_build_root=str(build),
            install_prefix=str(root / "baseline"),
            build_root=str(root / "tests"),
            compiler_llvm_root=str(root / "llvm"),
            llvm_dir=str(root / "llvm" / "lib" / "cmake" / "llvm"),
            jobs=7,
        )

    def test_deploy_is_independent_from_baseline_and_uses_fixed_c_prefix(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT) as temporary:
            config = self.config(Path(temporary))
            with patch.object(
                deploy,
                "discover_compiler_build_tools",
                return_value={"cmake": "A:/apps/cmake/bin/cmake.exe"},
            ):
                plan = deploy.create_deploy_plan(config, "Release")

        self.assertEqual(plan.prefix, Path("C:/native-sharp").resolve())
        self.assertEqual(plan.commands[0][1][1:3], ["--install", plan.build_dir.as_posix()])
        self.assertEqual(plan.commands[0][1][-2:], ["--prefix", plan.prefix.as_posix()])
        self.assertEqual(plan.commands[1][1][0], str(plan.prefix / "bin" / "nsc.exe"))

    def test_baseline_requires_a_drive_nslink_outside_fixed_staging(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT) as temporary:
            root = Path(temporary)
            config = self.config(root)
            staging = root / "baseline"
            with (
                patch.object(baseline, "BASELINE_PREFIX", staging),
                patch.object(baseline, "BASELINE_PROMOTION_PREFIX", Path("C:/native-sharp")),
                patch.object(
                    baseline,
                    "discover_compiler_build_tools",
                    return_value={"cmake": "A:/apps/cmake/bin/cmake.exe"},
                ),
                patch.object(baseline, "compiler_configure_commands", return_value=[]),
                patch.object(baseline, "compiler_build_commands", return_value=[]),
            ):
                plan = baseline.create_baseline_plan(config)
                inside = staging / "bin" / "nslink.exe"
                inside.parent.mkdir(parents=True)
                inside.write_bytes(b"stale")
                with self.assertRaisesRegex(ValueError, "reset staging"):
                    baseline.create_baseline_plan(
                        replace(config, nslink_executable=str(inside))
                    )

        self.assertEqual(plan.optimizations, ("O0", "O1", "O2", "O3"))
        self.assertEqual(plan.compiler_config.install_prefix, str(staging.resolve()))
        self.assertEqual(plan.promotion_prefix, Path("C:/native-sharp").resolve())

    def test_baseline_rejects_partial_or_unknown_profile_sets(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT) as temporary:
            config = self.config(Path(temporary))
            with self.assertRaisesRegex(ValueError, "O0..O3"):
                baseline.create_baseline_plan(config, ("O4",))

    def test_unit_filter_uses_only_unique_canonical_unit_rows(self) -> None:
        def case(case_id: str, layer: str, profile: str) -> CaseRecord:
            return CaseRecord(
                case_id,
                "CI-TEST-001",
                "Test.Component",
                1,
                "fixture",
                layer,
                profile,
                ("Debug",),
                ("NA",),
                "nsc_component_tests",
                "Executable",
                "fixture",
                "PASS",
                {},
            )

        rows = (
            case("TU-001", "unit", "PR"),
            case("TU-002", "unit", "WEEKLY"),
            case("TE-001", "integration", "PR"),
        )
        self.assertEqual(
            [item.case_id for item in select_unit_cases(rows, profile="PR")],
            ["TU-001"],
        )
        self.assertEqual(
            [item.case_id for item in select_unit_cases(rows, "TU-002")],
            ["TU-002"],
        )

    def test_cancellation_state_does_not_launch_a_process(self) -> None:
        controller = ProcessController(lambda _line: None)
        controller.cancel()
        self.assertTrue(controller.cancelled.is_set())
        self.assertEqual(controller.run(("never-launched",), REPOSITORY_ROOT)[0], 130)

    def test_gui_smoke_catalog_is_in_memory_and_unit_scoped(self) -> None:
        catalog = _smoke_catalog()

        self.assertEqual(len(catalog.cases), 1)
        self.assertEqual(catalog.cases[0].layer, "unit")
        self.assertEqual(catalog.cases[0].case_id, "SMOKE-UNIT-001")


if __name__ == "__main__":
    unittest.main()
