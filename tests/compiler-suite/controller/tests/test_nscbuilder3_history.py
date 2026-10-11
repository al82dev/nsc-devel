from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3HistoryTests(unittest.TestCase):
    def setUp(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = TemporaryDirectory(dir=builder.STORAGE_ROOT)
        self.root = Path(self.temporary.name) / "20261010-000000-example"
        self.root.mkdir()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _write_json(self, name: str, value: object) -> None:
        (self.root / name).write_text(
            json.dumps(value, ensure_ascii=False) + "\n", encoding="utf-8"
        )

    def _metadata(self) -> dict[str, object]:
        return {
            "schema": 1,
            "kind": "phase-2-suite",
            "run_id": self.root.name,
            "variant_count": 1,
            "case_count": 1,
            "selection": {
                "scope": "Total",
                "value": "",
                "profile": "Todos",
                "build_mode": "Debug",
                "optimization": "O0",
            },
        }

    def _result(self) -> dict[str, object]:
        return {
            "key": "CASE|Debug|O0",
            "case_id": "CASE",
            "contract_id": "CONTRACT",
            "component": "Host.Basic",
            "wave": 1,
            "build_mode": "Debug",
            "optimization": "O0",
            "status": "PASS",
            "duration_seconds": 0.1,
            "detail": "",
            "evidence": "evidence.log",
        }

    def _write_valid_terminal_run(self) -> None:
        self._write_json("metadata.json", self._metadata())
        self._write_json(
            "results.json",
            {
                "run_id": self.root.name,
                "finished_at": "2026-10-10T00:00:01.000+00:00",
                "status": "PASSED",
                "counts": {"PASS": 1},
                "results": [self._result()],
            },
        )
        (self.root / "summary.md").write_text("# summary\n", encoding="utf-8")
        events = (
            {"schema": 1, "kind": "RUN_STARTED", "payload": self._metadata()},
            {
                "schema": 1,
                "kind": "RUN_FINISHED",
                "payload": {"status": "PASSED", "counts": {"PASS": 1}},
            },
        )
        (self.root / "events.jsonl").write_text(
            "".join(json.dumps(item) + "\n" for item in events), encoding="utf-8"
        )
        (self.root / "full.log").write_text("historical output\n", encoding="utf-8")

    @staticmethod
    def _catalog() -> builder.CanonicalCatalog:
        return builder.CanonicalCatalog(
            (
                builder.CaseRecord(
                    case_id="CASE",
                    contract_id="CONTRACT",
                    component="Host.Basic",
                    wave=1,
                    purpose="history test",
                    layer="unit",
                    profile="PR",
                    build_modes=("Debug",),
                    optimizations=("O0",),
                    executor="nsc_component_tests",
                    disposition="Executable",
                    objective="restore one prior result",
                    expected_outcome="PASS",
                    manifest={
                        "class": "nominal",
                        "dependencies": ["PRECONDITION"],
                        "inputs": {"value": 7},
                        "isolation": "in-process-reset",
                        "oracle": {"value": 7},
                        "tags": ["history", "detail"],
                    },
                ),
            )
        )

    def test_complete_consistent_run_keeps_terminal_status(self) -> None:
        self._write_valid_terminal_run()

        record = builder.history_record(self.root)

        self.assertEqual(record[0], self.root.name)
        self.assertEqual(record[1], "phase-2-suite")
        self.assertEqual(record[2], "PASSED")
        self.assertEqual(record[3], "2026-10-10T00:00:01.000+00:00")

    def test_missing_terminal_artifacts_remain_incomplete(self) -> None:
        self._write_json("metadata.json", self._metadata())

        record = builder.history_record(self.root)

        self.assertEqual(record[2], "INCOMPLETE")

    def test_unknown_terminal_status_is_malformed_not_passed(self) -> None:
        self._write_valid_terminal_run()
        payload = json.loads((self.root / "results.json").read_text(encoding="utf-8"))
        payload["status"] = "SUCCESS"
        self._write_json("results.json", payload)

        record = builder.history_record(self.root)

        self.assertEqual(record[2], "MALFORMED")

    def test_result_counts_must_match_terminal_rows(self) -> None:
        self._write_valid_terminal_run()
        payload = json.loads((self.root / "results.json").read_text(encoding="utf-8"))
        payload["counts"] = {"PASS": 2}
        self._write_json("results.json", payload)

        record = builder.history_record(self.root)

        self.assertEqual(record[2], "MALFORMED")

    def test_duplicate_metadata_or_result_keys_are_malformed(self) -> None:
        self._write_valid_terminal_run()
        metadata = (self.root / "metadata.json").read_text(encoding="utf-8")
        (self.root / "metadata.json").write_text(
            metadata.replace('"schema": 1', '"schema": 1, "schema": 2'),
            encoding="utf-8",
        )
        self.assertEqual(builder.history_record(self.root)[2], "MALFORMED")

    def test_completed_suite_can_be_loaded_without_rewriting_evidence(self) -> None:
        self._write_valid_terminal_run()
        before = (self.root / "results.json").read_bytes()

        loaded = builder.load_history_execution(self.root, self._catalog())

        self.assertEqual(loaded.status, "PASSED")
        self.assertEqual([item.case_id for item in loaded.cases], ["CASE"])
        self.assertEqual([item.key for item in loaded.variants], ["CASE|Debug|O0"])
        self.assertEqual([item.status for item in loaded.outcomes], ["PASS"])
        self.assertEqual(loaded.full_log, "historical output\n")
        self.assertEqual((self.root / "results.json").read_bytes(), before)

    def test_completed_timeout_timing_fields_are_restored(self) -> None:
        self._write_valid_terminal_run()
        payload = json.loads((self.root / "results.json").read_text(encoding="utf-8"))
        result = payload["results"][0]
        result.update(
            {
                "status": "TIMEOUT",
                "detail": "tiempo límite individual agotado",
                "timeout_pass_count": 3,
                "timeout_average_seconds": 2.5,
                "timeout_limit_seconds": 8,
            }
        )
        payload["status"] = "FAILED"
        payload["counts"] = {"TIMEOUT": 1}
        self._write_json("results.json", payload)
        events = [
            json.loads(line)
            for line in (self.root / "events.jsonl").read_text(encoding="utf-8").splitlines()
        ]
        events[-1]["payload"] = {"status": "FAILED", "counts": {"TIMEOUT": 1}}
        (self.root / "events.jsonl").write_text(
            "".join(json.dumps(item) + "\n" for item in events), encoding="utf-8"
        )

        loaded = builder.load_history_execution(self.root, self._catalog())

        outcome = loaded.outcomes[0]
        self.assertEqual(outcome.status, "TIMEOUT")
        self.assertEqual(outcome.timeout_pass_count, 3)
        self.assertEqual(outcome.timeout_average_seconds, 2.5)
        self.assertEqual(outcome.timeout_limit_seconds, 8)

    def test_case_detail_projects_contract_result_and_complete_case_logs(self) -> None:
        case = self._catalog().by_id["CASE"]
        variant = builder.RunVariant(case, "Debug", "O0")
        case_root = self.root / "cases" / "CASE-Debug-O0"
        case_root.mkdir(parents=True)
        evidence = case_root / "host.log"
        evidence.write_text("assertion failed\n", encoding="utf-8")
        (case_root / "stderr.log").write_text("compiler diagnostic\n", encoding="utf-8")
        outcome = builder.CaseOutcome(
            key=variant.key,
            case_id=case.case_id,
            contract_id=case.contract_id,
            component=case.component,
            wave=case.wave,
            build_mode=variant.build_mode,
            optimization=variant.optimization,
            status="FAIL",
            duration_seconds=0.25,
            detail="the oracle rejected the observed value",
            evidence=str(evidence),
        )

        summary = builder.case_diagnostic_summary(case, variant, outcome)
        contract = builder.case_contract_payload(case)
        output = builder.case_evidence_output(outcome, self.root)

        self.assertIn("Caso              : CASE", summary)
        self.assertIn("Estado            : FAIL", summary)
        self.assertIn("the oracle rejected the observed value", summary)
        self.assertEqual(contract["inputs"], {"value": 7})
        self.assertEqual(contract["oracle"], {"value": 7})
        self.assertIn("assertion failed", output)
        self.assertIn("compiler diagnostic", output)
        self.assertIn(str(evidence), output)

    def test_loading_reconstructs_only_executable_rows_from_selection(self) -> None:
        self._write_valid_terminal_run()

        loaded = builder.load_history_execution(self.root, self._catalog())

        self.assertEqual(len(loaded.cases), 1)
        self.assertTrue(loaded.cases[0].executable)
        self.assertEqual(len(loaded.variants), 1)
        self.assertEqual(len(loaded.outcomes), 1)

    def test_loading_accepts_complete_results_in_completion_order(self) -> None:
        first = self._catalog().by_id["CASE"]
        second = builder.CaseRecord(
            case_id="CASE-2",
            contract_id="CONTRACT-2",
            component="Host.Basic",
            wave=1,
            purpose="second history test",
            layer="unit",
            profile="PR",
            build_modes=("Debug",),
            optimizations=("O0",),
            executor="nsc_component_tests",
            disposition="Executable",
            objective="preserve completion order",
            expected_outcome="PASS",
            manifest={},
        )
        catalog = builder.CanonicalCatalog((first, second))
        metadata = self._metadata()
        metadata["variant_count"] = 2
        metadata["case_count"] = 2
        second_result = self._result()
        second_result.update(
            {
                "key": "CASE-2|Debug|O0",
                "case_id": "CASE-2",
                "contract_id": "CONTRACT-2",
            }
        )
        self._write_json("metadata.json", metadata)
        self._write_json(
            "results.json",
            {
                "run_id": self.root.name,
                "finished_at": "2026-10-10T00:00:01.000+00:00",
                "status": "PASSED",
                "counts": {"PASS": 2},
                "results": [second_result, self._result()],
            },
        )
        (self.root / "summary.md").write_text("# summary\n", encoding="utf-8")
        events = (
            {"schema": 1, "kind": "RUN_STARTED", "payload": metadata},
            {
                "schema": 1,
                "kind": "RUN_FINISHED",
                "payload": {"status": "PASSED", "counts": {"PASS": 2}},
            },
        )
        (self.root / "events.jsonl").write_text(
            "".join(json.dumps(item) + "\n" for item in events), encoding="utf-8"
        )

        loaded = builder.load_history_execution(self.root, catalog)

        self.assertEqual(
            [item.key for item in loaded.variants],
            ["CASE|Debug|O0", "CASE-2|Debug|O0"],
        )
        self.assertEqual(
            [item.key for item in loaded.outcomes],
            ["CASE-2|Debug|O0", "CASE|Debug|O0"],
        )

    def test_loading_rejects_case_absent_from_current_catalog(self) -> None:
        self._write_valid_terminal_run()

        with self.assertRaisesRegex(ValueError, "absent from the current catalogue"):
            builder.load_history_execution(
                self.root, builder.CanonicalCatalog(())
            )

    def test_loading_rejects_non_suite_history(self) -> None:
        self._write_valid_terminal_run()
        metadata = self._metadata()
        metadata["kind"] = "infrastructure"
        self._write_json("metadata.json", metadata)

        with self.assertRaisesRegex(ValueError, "not a Phase 2 suite"):
            builder.load_history_execution(self.root, self._catalog())

        self._write_valid_terminal_run()
        results = (self.root / "results.json").read_text(encoding="utf-8")
        (self.root / "results.json").write_text(
            results.replace('"status": "PASSED"', '"status": "PASSED", "status": "FAILED"'),
            encoding="utf-8",
        )
        self.assertEqual(builder.history_record(self.root)[2], "MALFORMED")


if __name__ == "__main__":
    unittest.main()
