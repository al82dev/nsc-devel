from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


def outcome() -> builder.CaseOutcome:
    return builder.CaseOutcome(
        key="CASE|Release|O0",
        case_id="CASE",
        contract_id="CONTRACT",
        component="Host.Basic",
        wave=1,
        build_mode="Release",
        optimization="O0",
        status="PASS",
        duration_seconds=0.1,
        detail="",
        evidence="",
    )


class Builder3EvidencePublicationTests(unittest.TestCase):
    def _store(self, history_root: Path, run_id: str) -> builder.EvidenceStore:
        metadata = {"run_id": run_id, "kind": "tests", "variant_count": 1}
        with patch.object(builder, "HISTORY_ROOT", history_root):
            return builder.EvidenceStore(run_id, metadata)

    def test_terminal_artifacts_precede_the_final_event(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            store = self._store(Path(directory), "successful-run")
            store.finish([outcome()], cancelled=False)
            root = store.root
            events = [
                json.loads(line)
                for line in (root / "events.jsonl").read_text(encoding="utf-8").splitlines()
            ]

            self.assertTrue((root / "metadata.json").is_file())
            self.assertTrue((root / "results.json").is_file())
            self.assertTrue((root / "summary.md").is_file())
            self.assertEqual(events[-1]["kind"], "RUN_FINISHED")
            self.assertEqual(list(root.glob("*.tmp")), [])

    def test_results_publication_failure_never_emits_run_finished(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            store = self._store(Path(directory), "results-failure")
            real_replace = builder.os.replace

            def replace(source: object, destination: object) -> None:
                if Path(destination).name == "results.json":
                    raise OSError("results replace failed")
                real_replace(source, destination)

            try:
                with patch.object(builder.os, "replace", side_effect=replace):
                    with self.assertRaisesRegex(OSError, "results replace failed"):
                        store.finish([outcome()], cancelled=False)
            finally:
                store.close()

            events = (store.root / "events.jsonl").read_text(encoding="utf-8")
            self.assertNotIn("RUN_FINISHED", events)
            self.assertFalse((store.root / "results.json").exists())
            self.assertEqual(list(store.root.glob("*.tmp")), [])

    def test_summary_publication_failure_remains_incomplete(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            store = self._store(Path(directory), "summary-failure")
            real_replace = builder.os.replace

            def replace(source: object, destination: object) -> None:
                if Path(destination).name == "summary.md":
                    raise OSError("summary replace failed")
                real_replace(source, destination)

            try:
                with patch.object(builder.os, "replace", side_effect=replace):
                    with self.assertRaisesRegex(OSError, "summary replace failed"):
                        store.finish([outcome()], cancelled=False)
            finally:
                store.close()

            self.assertTrue((store.root / "results.json").is_file())
            self.assertFalse((store.root / "summary.md").exists())
            self.assertNotIn(
                "RUN_FINISHED",
                (store.root / "events.jsonl").read_text(encoding="utf-8"),
            )
            self.assertEqual(builder.history_record(store.root)[2], "INCOMPLETE")
            self.assertEqual(list(store.root.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
