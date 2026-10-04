from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
import nscbuilder


class NscBuilderPipelineTests(unittest.TestCase):
    def test_compiler_build_uses_configured_nslink_before_staging_exists(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            source_nslink = root / "source" / "nslink.exe"
            source_nslink.parent.mkdir(parents=True)
            source_nslink.write_bytes(b"directed-test")
            staging = root / "staging"
            plan = nscbuilder.make_baseline_plan(
                1,
                prefix=staging,
                nslink_exe=source_nslink,
                history_root=root / "history",
                test_work_root=root / "work",
                temp_root=root / "temp",
            )
            worker = nscbuilder.PipelineWorker(plan, [])

            environment = worker._compiler_build_environment()

            self.assertEqual(Path(environment["NSC_LINKER"]), source_nslink.resolve())
            self.assertFalse((staging / "bin" / "nslink.exe").exists())

    def test_missing_after_build_snapshot_does_not_invent_binary_drift(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            plan = nscbuilder.make_baseline_plan(
                1,
                prefix=root / "staging",
                nslink_exe=root / "source" / "nslink.exe",
                history_root=root / "history",
                test_work_root=root / "work",
                temp_root=root / "temp",
            )
            worker = nscbuilder.PipelineWorker(plan, [])
            sources = {"source_digest": "same"}
            missing = {
                "moment": "after-build",
                "status": "not-captured",
                "binaries": {},
                "tools": {},
            }
            final = {
                "source_digest": "same",
                "binaries": {"built_nsc": "hash", "installed_nsc": "hash"},
                "tools": {"lld-link": "hash", "nslink": "hash"},
            }

            self.assertEqual(worker._abi_compare(sources, missing, final), [])

    def test_captured_snapshot_still_requires_compiler_hashes(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            plan = nscbuilder.make_baseline_plan(
                1,
                prefix=root / "staging",
                nslink_exe=root / "source" / "nslink.exe",
                history_root=root / "history",
                test_work_root=root / "work",
                temp_root=root / "temp",
            )
            worker = nscbuilder.PipelineWorker(plan, [])
            sources = {"source_digest": "same"}
            captured = {"moment": "after-build", "binaries": {}, "tools": {}}
            final = {"source_digest": "same", "binaries": {}, "tools": {}}

            problems = worker._abi_compare(sources, captured, final)

            self.assertIn(
                "No se pudo hashear el nsc.exe construido antes y despues de la matriz",
                problems,
            )
            self.assertIn(
                "No se pudo hashear el nsc.exe instalado antes y despues de la matriz",
                problems,
            )


if __name__ == "__main__":
    unittest.main()
