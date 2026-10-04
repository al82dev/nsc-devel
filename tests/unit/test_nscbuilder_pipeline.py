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

    def test_condensed_failures_groups_shared_root_cause_and_omits_gate_closure(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            first = root / "first.log"
            second = root / "second.log"
            diagnostic = (
                "A:\\src\\HashSet.ns:238:21: error[NS3002]: "
                "match expression syntax requires <Language>3</Language>\n"
            )
            first.write_text(diagnostic, encoding="utf-8")
            second.write_text(diagnostic, encoding="utf-8")
            rows = [
                {
                    "linker": "NSLink",
                    "optimization": "O0",
                    "source": "c27",
                    "detail": "falló la emisión",
                    "error_log": str(first),
                },
                {
                    "linker": "NSLink",
                    "optimization": "O0",
                    "source": "c52",
                    "detail": "falló la emisión",
                    "error_log": str(second),
                },
                {
                    "linker": "NSLink",
                    "optimization": "General",
                    "source": "Cerrar validación de alcance reducido",
                    "detail": "La etapa terminó con código 1",
                    "error_log": str(root / "gate.log"),
                },
            ]

            groups = nscbuilder.condensed_failures(rows)

            self.assertEqual(len(groups), 1)
            self.assertEqual(groups[0]["occurrences"], 2)
            self.assertEqual(groups[0]["sources"], ["c27", "c52"])
            self.assertIn("match expression syntax", groups[0]["cause"])

    def test_condensed_failure_extracts_msvc_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            log = Path(temporary) / "build.log"
            log.write_text(
                "FAILED: object\n"
                "A:\\tests\\Fixture.cpp(17,84): error: missing field initializer\n",
                encoding="utf-8",
            )

            cause = nscbuilder.first_actionable_log_diagnostic(log)

            self.assertEqual(
                cause,
                "A:\\tests\\Fixture.cpp(17,84): missing field initializer",
            )


if __name__ == "__main__":
    unittest.main()
