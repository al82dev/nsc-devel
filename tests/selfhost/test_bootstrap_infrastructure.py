from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

import nscbuilder
from selfhost.bootstrap import clean_selfhost, verify_environment


TEST_TEMP_ROOT = ROOT / "_nscbuilder" / "temp" / "selfhost-unit-tests"


class BootstrapInfrastructureTests(unittest.TestCase):
    def setUp(self) -> None:
        TEST_TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=TEST_TEMP_ROOT)
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_configuration_precedence_environment_over_file(self) -> None:
        config = {"paths": {"prefix": "A:\\from-config"}}
        with patch.dict(os.environ, {"NSC_INSTALL_ROOT": "A:\\from-environment"}, clear=False):
            self.assertEqual(
                nscbuilder.configured_path(config, "prefix", "A:\\default"),
                "A:\\from-environment",
            )

    def test_configuration_uses_file_before_default(self) -> None:
        config = {"paths": {"prefix": "A:\\from-config"}}
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(
                nscbuilder.configured_path(config, "prefix", "A:\\default"),
                "A:\\from-config",
            )

    def test_reproducible_build_id_uses_environment_before_file(self) -> None:
        config = {"build_id_override": "2000000000000000"}
        with patch.dict(
            os.environ,
            {"NSC_BUILD_ID_OVERRIDE": "2000000000000001"},
            clear=False,
        ):
            self.assertEqual(nscbuilder.configured_build_id(config), "2000000000000001")

    def test_reproducible_build_id_rejects_invalid_value(self) -> None:
        with patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(ValueError):
                nscbuilder.configured_build_id({"build_id_override": "not-a-build-id"})

    def test_source_manifest_includes_selfhost_but_not_generated_state(self) -> None:
        (self.root / "selfhost" / "bootstrap").mkdir(parents=True)
        (self.root / "selfhost" / "bootstrap" / "bootstrap.lock").write_text("lock")
        (self.root / "selfhost" / "out").mkdir()
        (self.root / "selfhost" / "out" / "generated.txt").write_text("generated")
        (self.root / "selfhost" / "evidence" / "runs").mkdir(parents=True)
        (self.root / "selfhost" / "evidence" / "runs" / "report.md").write_text("report")
        (self.root / "selfhost" / "evidence" / "gates").mkdir()
        (self.root / "selfhost" / "evidence" / "gates" / "gate.json").write_text("gate")
        relative = {
            path.relative_to(self.root).as_posix()
            for path in nscbuilder.collect_source_files(self.root)
        }
        self.assertIn("selfhost/bootstrap/bootstrap.lock", relative)
        self.assertNotIn("selfhost/out/generated.txt", relative)
        self.assertNotIn("selfhost/evidence/runs/report.md", relative)
        self.assertNotIn("selfhost/evidence/gates/gate.json", relative)

    def test_library_source_marker_matches_tracked_file(self) -> None:
        self.assertEqual((ROOT / "lib" / ".source.txt").read_bytes(), nscbuilder.LIBRARY_SOURCE_MARKER)

    def test_write_guard_rejects_destination_outside_allowed_root(self) -> None:
        allowed = self.root / "allowed"
        plan = nscbuilder.make_baseline_plan(
            1,
            compiler_build_dir=allowed / "build",
            prefix=self.root / "forbidden",
            test_build_lld=allowed / "test-lld",
            test_build_nslink=allowed / "test-nslink",
            history_root=allowed / "history",
            test_work_root=allowed / "work",
            temp_root=allowed / "temp",
            allowed_write_roots=[allowed],
        )
        rejected = nscbuilder.validate_plan_write_paths(plan)
        self.assertIn(str((self.root / "forbidden").resolve()), rejected)
        self.assertNotIn(str((allowed / "build").resolve()), rejected)

    def test_configured_build_is_usable_but_not_deletable_until_marked(self) -> None:
        allowed = self.root / "allowed"
        build = allowed / "arbitrary-name"
        self.assertTrue(
            nscbuilder.build_directory_allowed(build, ROOT, "build-test-", [allowed])
        )
        self.assertFalse(nscbuilder.build_directory_can_be_deleted(build, ROOT, "build-test-"))
        nscbuilder.mark_managed_build(build)
        self.assertTrue(nscbuilder.build_directory_can_be_deleted(build, ROOT, "build-test-"))

    def test_environment_verifier_accepts_locked_file(self) -> None:
        artifact = self.root / "artifact.bin"
        artifact.write_bytes(b"NativeSharp bootstrap fixture\n")
        digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
        lock = {
            "schema": "nativesharp-bootstrap-lock",
            "version": 1,
            "target": {"host": "test"},
            "artifacts": [
                {
                    "id": "fixture",
                    "path": "${FIXTURE}",
                    "state": "locked",
                    "sha256": digest,
                }
            ],
        }
        report = verify_environment.verify(lock, {"FIXTURE": str(artifact)})
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["problems"], [])

    def test_environment_verifier_rejects_pending_hash(self) -> None:
        artifact = self.root / "artifact.bin"
        artifact.write_bytes(b"pending")
        lock = {
            "schema": "nativesharp-bootstrap-lock",
            "version": 1,
            "target": {"host": "test"},
            "artifacts": [
                {
                    "id": "fixture",
                    "path": "${FIXTURE}",
                    "state": "pending",
                    "sha256": None,
                }
            ],
        }
        report = verify_environment.verify(lock, {"FIXTURE": str(artifact)})
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len(report["problems"]), 2)

    def test_cleanup_requires_marker_and_only_selects_named_children(self) -> None:
        state = self.root / "state"
        state.mkdir()
        with self.assertRaises(ValueError):
            clean_selfhost.cleanup_targets(state)
        clean_selfhost.initialize(state)
        generated = state / "build"
        generated.mkdir()
        preserved = state / "sources"
        preserved.mkdir()
        self.assertEqual(clean_selfhost.cleanup_targets(state), [generated.resolve()])
        self.assertTrue(preserved.is_dir())

    def test_structured_report_matches_public_v1_contract(self) -> None:
        report = nscbuilder.structured_run_report(
            {
                "id": "fixture-run",
                "started_at": "2026-09-19T00:00:00Z",
                "finished_at": "2026-09-19T00:00:01Z",
                "status": "passed",
                "plan": {"operation": "baseline"},
                "stages": [{"name": "fixture", "status": "passed"}],
                "cases": [],
                "failed": 0,
                "total_seconds": 1.0,
                "failures": [],
            }
        )
        self.assertEqual(report["schema"], "nativesharp-selfhost-report")
        self.assertEqual(report["version"], 1)
        self.assertEqual(report["run_id"], "fixture-run")
        self.assertEqual(report["inputs"]["operation"], "baseline")
        self.assertEqual(report["problems"], [])


if __name__ == "__main__":
    unittest.main()
