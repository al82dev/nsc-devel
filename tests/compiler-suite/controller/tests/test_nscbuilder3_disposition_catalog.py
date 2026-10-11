from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3DispositionCatalogTests(unittest.TestCase):
    def test_live_design_is_exactly_the_executable_manifest(self) -> None:
        design_ids = {
            row["case_id"]
            for row in builder.read_catalog_csv(
                builder.DESIGN_PATH, builder.DESIGN_CATALOG_FIELDS
            )
        }
        manifest_ids = {
            row["case_id"] for row in builder.read_jsonl(builder.MANIFEST_PATH)
        }
        self.assertEqual(design_ids, manifest_ids)

    def test_jsonl_reader_rejects_duplicate_keys_at_any_depth(self) -> None:
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as temporary:
            path = Path(temporary) / "SyntheticCaseClassifications.jsonl"
            path.write_text(
                '{"case_id":"T-001","nested":{"state":1,"state":2},'
                '"disposition":"Executable"}\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicate JSON object key: state"):
                builder.read_jsonl(path)


if __name__ == "__main__":
    unittest.main()
