from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.report import ReportError, ReportWriter, RunMetadata, ToolIdentity
from nsc_suite.result import CaseResult, CaseStatus


def metadata(*case_ids: str) -> RunMetadata:
    return RunMetadata(
        run_id="run-001",
        revision="test-revision",
        build_mode="Debug",
        optimization="O0",
        profile="PR",
        scope="focused",
        selected_case_ids=case_ids,
        tools=(ToolIdentity("nsc", "A:/build/bin/nsc.exe", "abc123"),),
    )


class ReportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="nsc-suite-report-", dir=Path.cwd()
        )
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_writes_complete_deterministic_report_set(self) -> None:
        results = (
            CaseResult("TB-BAS-001", CaseStatus.PASS, duration_seconds=0.01),
            CaseResult(
                "TB-BAS-002",
                CaseStatus.FAIL,
                failures=("$.value: expected 1, actual 2",),
            ),
            CaseResult(
                "TB-BAS-003",
                CaseStatus.BLOCKED,
                blocked_by=("TB-BAS-002",),
            ),
        )
        ReportWriter(self.root).write(
            metadata("TB-BAS-001", "TB-BAS-002", "TB-BAS-003"), reversed(results)
        )
        payload = json.loads((self.root / "results.json").read_text(encoding="utf-8"))
        self.assertEqual(payload["counts"]["PASS"], 1)
        self.assertEqual(payload["counts"]["FAIL"], 1)
        self.assertEqual(payload["counts"]["BLOCKED"], 1)
        self.assertEqual(
            [result["case_id"] for result in payload["results"]],
            ["TB-BAS-001", "TB-BAS-002", "TB-BAS-003"],
        )
        summary = (self.root / "summary.md").read_text(encoding="utf-8")
        self.assertIn("| PASS | 1 |", summary)
        failures = (self.root / "failures.log").read_text(encoding="utf-8")
        self.assertNotIn("TB-BAS-001", failures)
        self.assertIn("failure=$.value: expected 1, actual 2", failures)
        self.assertIn("blocked_by=TB-BAS-002", failures)

    def test_rejects_incomplete_or_unexpected_results(self) -> None:
        with self.assertRaisesRegex(ReportError, "coverage mismatch"):
            ReportWriter(self.root).write(
                metadata("TB-BAS-001", "TB-BAS-002"),
                (CaseResult("TB-BAS-001", CaseStatus.PASS),),
            )
        with self.assertRaisesRegex(ReportError, "coverage mismatch"):
            ReportWriter(self.root).write(
                metadata("TB-BAS-001"),
                (
                    CaseResult("TB-BAS-001", CaseStatus.PASS),
                    CaseResult("TB-BAS-002", CaseStatus.PASS),
                ),
            )

    def test_refuses_to_overwrite_any_final_report(self) -> None:
        (self.root / "summary.md").write_text("preserve\n", encoding="utf-8")
        with self.assertRaisesRegex(ReportError, "already exists"):
            ReportWriter(self.root).write(
                metadata("TB-BAS-001"),
                (CaseResult("TB-BAS-001", CaseStatus.PASS),),
            )
        self.assertEqual((self.root / "summary.md").read_text(encoding="utf-8"), "preserve\n")
        self.assertFalse((self.root / "results.json").exists())

    def test_rejects_non_a_drive_root(self) -> None:
        with self.assertRaisesRegex(ReportError, "must be on A"):
            ReportWriter(Path("C:/not-authorized"))


if __name__ == "__main__":
    unittest.main()
