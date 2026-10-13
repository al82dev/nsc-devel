from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder.configuration import discover_archive_executable
from nsc_builder.execution import ExecutionEngine
from nsc_builder.isolation import isolated_test_environment
from nsc_builder.models import TestCase, TestVariant
from nsc_builder.paths import STORAGE_ROOT


class FakeStore:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.events: list[tuple[str, object]] = []

    def event(self, kind: str, payload: object) -> None:
        self.events.append((kind, payload))

    def log(self, _line: str) -> None:
        pass


def make_variant(case_id: str, component: str = "Host.Parser") -> TestVariant:
    case = TestCase(
        case_id=case_id,
        contract_id="CI-ISOLATION-001",
        component=component,
        wave=1,
        purpose="verify A-only execution isolation",
        layer="component",
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        executor="nsc_component_tests",
        disposition="Executable",
        objective="isolate every child process",
        expected_outcome="",
        manifest={},
    )
    return TestVariant(case, "Debug", "NA")


class ADriveExecutionIsolationTests(unittest.TestCase):
    def setUp(self) -> None:
        STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="builder-isolation-", dir=STORAGE_ROOT
        )
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_environment_overrides_inherited_temp_with_private_a_roots(self) -> None:
        owner = self.root / "run-a" / "cases" / "TP-001"
        environment = isolated_test_environment(
            owner,
            {"TEMP": "Z:/leaked", "TMP": "Z:/leaked", "PATH": "A:/tools"},
        )

        for key in ("TEMP", "TMP", "TMPDIR", "NSC_TEST_TEMP_ROOT"):
            path = Path(environment[key])
            self.assertEqual(path.drive.casefold(), "a:")
            self.assertIn(STORAGE_ROOT.resolve(), path.resolve().parents)
        self.assertEqual(environment["PATH"], "A:/tools")

    def test_variant_and_retry_roots_are_disjoint(self) -> None:
        first = isolated_test_environment(
            self.root / "run-a" / "cases" / "TP-001"
        )
        other_variant = isolated_test_environment(
            self.root / "run-a" / "cases" / "TP-002"
        )
        retry = isolated_test_environment(
            self.root / "run-b" / "cases" / "TP-001"
        )

        roots = {first["TEMP"], other_variant["TEMP"], retry["TEMP"]}
        self.assertEqual(len(roots), 3)

    def test_non_a_owner_is_rejected_before_directory_creation(self) -> None:
        with self.assertRaisesRegex(ValueError, "absolute A-drive path"):
            isolated_test_environment(Path("Z:/forbidden/run"))

    def test_archive_preflight_is_selection_specific(self) -> None:
        run_root = self.root / "selection-specific"
        engine = ExecutionEngine(
            SimpleNamespace(archive_executable="", nslink_executable="A:/nslink.exe"),
            FakeStore(run_root),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )
        engine._validate_run_infrastructure(
            (make_variant("TP-PAR-001"),), run_root / "preparation"
        )
        with self.assertRaisesRegex(FileNotFoundError, "archive executable"):
            engine._validate_run_infrastructure(
                (make_variant("T12B-00457", "Packaging"),),
                run_root / "preparation-packaging",
            )

    def test_archive_discovery_never_uses_path_or_non_a_override(self) -> None:
        with patch("nsc_builder.configuration.Path.is_file", return_value=False):
            self.assertEqual(discover_archive_executable("Z:/tools/7z.exe"), "")

    def test_archive_discovery_accepts_explicit_a_file(self) -> None:
        archive = self.root / "tools" / "7z.exe"
        archive.parent.mkdir(parents=True)
        archive.write_bytes(b"archive")

        self.assertEqual(discover_archive_executable(str(archive)), str(archive.resolve()))

    def test_host_dispatch_receives_its_variant_private_environment(self) -> None:
        run_root = self.root / "host-dispatch"
        executable = run_root / "producer" / "nsc_component_tests.exe"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"producer")
        variant = make_variant("TP-PAR-001")
        engine = ExecutionEngine(
            SimpleNamespace(
                build_root=str(run_root / "build"),
                nslink_executable="A:/nslink.exe",
            ),
            FakeStore(run_root),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )
        payload = {
            "case_id": variant.case.case_id,
            "contract_id": variant.case.contract_id,
            "component": variant.case.component,
            "duration_us": 1,
            "status": "PASS",
            "failures": [],
        }
        captured_environment: dict[str, str] = {}

        def fake_run(_command: object, _cwd: Path, environment: dict[str, str]):
            captured_environment.update(environment)
            return 0, json.dumps(payload) + "\n"

        engine.process.run = fake_run
        with patch(
            "nsc_builder.execution.component_test_executable",
            return_value=executable,
        ):
            status, _, _ = engine._run_host(variant)

        self.assertEqual(status, "PASS")
        self.assertIn(run_root.resolve(), Path(captured_environment["TEMP"]).resolve().parents)

    def test_packaging_helper_runs_in_process_with_private_environment(self) -> None:
        run_root = self.root / "packaging-api"
        case_root = run_root / "case"
        case_root.mkdir(parents=True)
        archive = run_root / "tools" / "7z.exe"
        archive.parent.mkdir(parents=True)
        archive.write_bytes(b"archive")
        engine = ExecutionEngine(
            SimpleNamespace(),
            FakeStore(run_root),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )
        module = engine._load_gate("run_packaging_gate.py")
        received: dict[str, str] = {}

        class FakePackagingApi:
            @staticmethod
            def create_archive(
                _prefix: Path,
                _archive: Path,
                _output: Path,
                _compression: int,
                _timestamp: str,
                _dry_run: bool,
                environment: dict[str, str],
            ) -> None:
                received.update(environment)
                print("Version    : 3.0.3")
                print("Build hash : deadbeef")
                print("Dry run: no archive was created.")

        external_runner = Mock(
            side_effect=AssertionError("external Python process used")
        )
        module.run_process = external_runner
        environment = isolated_test_environment(case_root)
        with patch.object(
            module,
            "stage_installation",
            return_value=case_root / "installation",
        ), patch.object(
            module,
            "load_packaging_api",
            return_value=FakePackagingApi(),
        ):
            satisfied, _detail = module.run_package_native_sharp(
                SimpleNamespace(
                    repository_root=Path("A:/nsc-devel"),
                    installation_root=case_root / "source-installation",
                    archive_executable=archive,
                ),
                case_root,
                1,
                environment,
            )

        self.assertTrue(satisfied)
        external_runner.assert_not_called()
        self.assertEqual(Path(received["TEMP"]).drive.casefold(), "a:")


if __name__ == "__main__":
    unittest.main()
