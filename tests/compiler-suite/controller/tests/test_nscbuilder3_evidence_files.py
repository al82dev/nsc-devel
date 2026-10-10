from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


def outcome(key: str, status: str = "PASS") -> builder.CaseOutcome:
    case_id, mode, optimization = key.split("|")
    return builder.CaseOutcome(
        key=key,
        case_id=case_id,
        contract_id="CI-EVIDENCE-001",
        component="Infrastructure",
        wave=0,
        build_mode=mode,
        optimization=optimization,
        status=status,
    )


class Builder3EvidenceFileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)

    def make_store(
        self, history_root: Path, run_id: str, variant_count: object
    ) -> builder.EvidenceStore:
        metadata = {
            "schema": 1,
            "kind": "phase-2-suite",
            "run_id": run_id,
            "variant_count": variant_count,
        }
        with patch.object(builder, "HISTORY_ROOT", history_root):
            return builder.EvidenceStore(run_id, metadata)

    def test_complete_run_publishes_one_result_and_event_pair_per_variant(self) -> None:
        with tempfile.TemporaryDirectory(dir=builder.STORAGE_ROOT) as temporary:
            root = Path(temporary)
            store = self.make_store(root, "complete", 2)
            values = [outcome("TI-EVIDENCE-001|Debug|NA"), outcome("TI-EVIDENCE-002|Debug|NA")]
            for value in values:
                store.event("CASE_STARTED", {"key": value.key})
                store.event("CASE_FINISHED", {"key": value.key, "status": value.status})
            store.log("synthetic progressive output")
            store.finish(values, False)

            result = json.loads((store.root / "results.json").read_text(encoding="utf-8"))
            events = [
                json.loads(line)
                for line in (store.root / "events.jsonl").read_text(encoding="utf-8").splitlines()
            ]
            self.assertEqual(result["status"], "PASSED")
            self.assertEqual([row["key"] for row in result["results"]], [item.key for item in values])
            self.assertEqual(
                [event["kind"] for event in events],
                [
                    "RUN_STARTED",
                    "CASE_STARTED",
                    "CASE_FINISHED",
                    "CASE_STARTED",
                    "CASE_FINISHED",
                    "RUN_FINISHED",
                ],
            )
            self.assertIn("synthetic progressive output", store.log_path.read_text(encoding="utf-8"))
            self.assertTrue(store._event_stream.closed)
            self.assertTrue(store._log_stream.closed)

    def test_missing_terminal_result_prevents_final_publication(self) -> None:
        with tempfile.TemporaryDirectory(dir=builder.STORAGE_ROOT) as temporary:
            store = self.make_store(Path(temporary), "missing", 2)
            try:
                with self.assertRaisesRegex(ValueError, "terminal result cardinality"):
                    store.finish([outcome("TI-EVIDENCE-001|Debug|NA")], False)
                self.assertFalse((store.root / "results.json").exists())
            finally:
                store.close()

    def test_duplicate_terminal_key_prevents_final_publication(self) -> None:
        with tempfile.TemporaryDirectory(dir=builder.STORAGE_ROOT) as temporary:
            store = self.make_store(Path(temporary), "duplicate", 2)
            duplicate = outcome("TI-EVIDENCE-001|Debug|NA")
            try:
                with self.assertRaisesRegex(ValueError, "duplicate terminal result keys"):
                    store.finish([duplicate, duplicate], False)
                self.assertFalse((store.root / "results.json").exists())
            finally:
                store.close()

    def test_invalid_declared_cardinality_prevents_final_publication(self) -> None:
        with tempfile.TemporaryDirectory(dir=builder.STORAGE_ROOT) as temporary:
            store = self.make_store(Path(temporary), "invalid-count", "two")
            try:
                with self.assertRaisesRegex(ValueError, "variant_count"):
                    store.finish([outcome("TI-EVIDENCE-001|Debug|NA")], False)
                self.assertFalse((store.root / "results.json").exists())
            finally:
                store.close()


if __name__ == "__main__":
    unittest.main()
