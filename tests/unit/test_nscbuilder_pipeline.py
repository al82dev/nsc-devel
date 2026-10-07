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
                "A:\\src\\HashSet.ns:238:21: error[NS9002]: "
                "invalid match expression syntax\n"
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

    def test_optimization_logs_are_complete_and_general_is_aggregate(self) -> None:
        selected = ("-O0", "-O2")

        self.assertEqual(
            nscbuilder.output_optimization_targets("NSLink", "General", selected),
            ("General", "O0", "O2"),
        )
        self.assertEqual(
            nscbuilder.output_optimization_targets("NSLink", "O0", selected),
            ("General", "O0"),
        )
        self.assertEqual(
            nscbuilder.output_optimization_targets("Compilador", "O0", selected),
            ("General",),
        )

    def test_worker_writes_shared_context_and_specific_output_to_complete_logs(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            plan = nscbuilder.make_baseline_plan(
                1,
                prefix=root / "staging",
                nslink_exe=root / "nslink.exe",
                history_root=root / "history",
                test_work_root=root / "work",
                temp_root=root / "temp",
                optimizations=("-O0", "-O2"),
            )
            worker = nscbuilder.PipelineWorker(plan, [])
            worker.run_dir = root / "run"
            worker.run_dir.mkdir()
            worker.console_path = worker.run_dir / "console.log"
            worker.console_path.touch()

            worker._emit("shared context", channel="NSLink")
            worker._emit("only O0", channel="NSLink", optimization="O0")

            general = (worker.run_dir / "NSLink" / "General" / "console.log").read_text()
            o0 = (worker.run_dir / "NSLink" / "O0" / "console.log").read_text()
            o2 = (worker.run_dir / "NSLink" / "O2" / "console.log").read_text()
            self.assertIn("shared context", general)
            self.assertIn("only O0", general)
            self.assertIn("shared context", o0)
            self.assertIn("only O0", o0)
            self.assertIn("shared context", o2)
            self.assertNotIn("only O0", o2)

    def test_condensed_report_embeds_complete_logs_without_clicks(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            first = root / "first.log"
            second = root / "second.log"
            gate = root / "gate.log"
            first.write_text(
                "A:\\src\\HashSet.ns:238:21: error[NS9002]: shared cause\n"
                "first complete evidence line\n",
                encoding="utf-8",
            )
            second.write_text(
                "A:\\src\\HashSet.ns:238:21: error[NS9002]: shared cause\n"
                "second complete evidence line\n",
                encoding="utf-8",
            )
            gate.write_text("derived gate detail\n", encoding="utf-8")
            rows = [
                {
                    "linker": "NSLink",
                    "optimization": "O0",
                    "source": "c27",
                    "error_log": str(first),
                },
                {
                    "linker": "NSLink",
                    "optimization": "O0",
                    "source": "c52",
                    "error_log": str(second),
                },
                {
                    "linker": "NSLink",
                    "optimization": "General",
                    "source": "Cerrar validación de alcance reducido",
                    "error_log": str(gate),
                },
            ]

            report = nscbuilder.render_condensed_error_report(rows, "NSLink")

            self.assertIn("Causas únicas      : 1", report)
            self.assertIn("Afecta        : c27, c52", report)
            self.assertIn("first complete evidence line", report)
            self.assertIn("second complete evidence line", report)
            self.assertNotIn("derived gate detail", report)

    def test_cancelled_ctest_log_preserves_exact_rerunnable_failures(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            run_dir = Path(temporary) / "run"
            stages = run_dir / "stages"
            stages.mkdir(parents=True)
            plan = nscbuilder.make_baseline_plan(
                1,
                prefix=Path(temporary) / "staging",
                nslink_exe=Path(temporary) / "nslink.exe",
                history_root=Path(temporary) / "history",
                test_work_root=Path(temporary) / "work",
                temp_root=Path(temporary) / "temp",
            )
            (run_dir / "metadata.json").write_text(
                nscbuilder.json.dumps(
                    {
                        "plan": nscbuilder.asdict(plan),
                        "status": "cancelled",
                        "failures": [],
                        "stages": [
                            {
                                "name": "CTest [NSLink]",
                                "log": "stages/CTest_NSLink.log",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            (stages / "CTest_NSLink.log").write_text(
                "17/610 Test #4: nsc.ns3.first ....***Failed 0.44 sec\n"
                "18/610 Test #5: nsc.ns3.ok .... Passed 0.12 sec\n"
                "19/610 Test #6: nsc.ns3.second ....***Exception 0.20 sec\n"
                "Cancelado\n",
                encoding="utf-8",
            )

            _metadata, names = nscbuilder.rerunnable_ctest_failures(run_dir)

            self.assertEqual(names, ["nsc.ns3.first", "nsc.ns3.second"])

    def test_failure_rerun_plan_cannot_rebuild_or_certify_baseline(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            original = nscbuilder.make_baseline_plan(
                12,
                prefix=root / "staging",
                nslink_exe=root / "nslink.exe",
                test_build_nslink=root / "build-test-nslink",
                history_root=root / "history",
                test_work_root=root / "work",
                temp_root=root / "temp",
            )

            rerun = nscbuilder.make_failure_rerun_plan(
                {"plan": nscbuilder.asdict(original)},
                ["nsc.ns3.first", "nsc.ns3.second", "nsc.ns3.first"],
            )

            self.assertEqual(rerun.operation, "language")
            self.assertEqual(rerun.linkers, ["NSLink"])
            self.assertEqual(rerun.selected_ctests, ["nsc.ns3.first", "nsc.ns3.second"])
            self.assertFalse(rerun.run_build)
            self.assertFalse(rerun.build_test_trees)
            self.assertFalse(rerun.sync_libraries)
            self.assertFalse(rerun.run_cases)
            self.assertFalse(rerun.run_triage)
            self.assertFalse(rerun.certify_baseline)
            self.assertFalse(rerun.lld_diagnostics)

    def test_successful_newer_ctest_run_hides_stale_failures(self) -> None:
        with tempfile.TemporaryDirectory(dir=nscbuilder.ROOT) as temporary:
            root = Path(temporary)
            history = root / "history"
            original = nscbuilder.make_baseline_plan(
                1,
                prefix=root / "staging",
                nslink_exe=root / "nslink.exe",
                test_build_nslink=root / "build-test-nslink",
                history_root=history,
                test_work_root=root / "work",
                temp_root=root / "temp",
            )
            old = history / "old"
            new = history / "new"
            old.mkdir(parents=True)
            new.mkdir(parents=True)
            common = {
                "plan": nscbuilder.asdict(original),
                "finished_at": "2026-10-04T06:00:00+00:00",
                "stages": [],
                "cases": [],
            }
            (old / "metadata.json").write_text(
                nscbuilder.json.dumps(
                    {
                        **common,
                        "id": "old",
                        "started_at": "2026-10-04T05:00:00+00:00",
                        "status": "failed",
                        "failed": 1,
                        "failures": [{"kind": "CTest", "source": "nsc.old.failure"}],
                    }
                ),
                encoding="utf-8",
            )
            (new / "metadata.json").write_text(
                nscbuilder.json.dumps(
                    {
                        **common,
                        "id": "new",
                        "started_at": "2026-10-04T06:00:00+00:00",
                        "status": "passed",
                        "failed": 0,
                        "failures": [],
                    }
                ),
                encoding="utf-8",
            )

            self.assertIsNone(nscbuilder.latest_rerunnable_ctest_run(history))


if __name__ == "__main__":
    unittest.main()
