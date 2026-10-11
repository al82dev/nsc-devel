from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3DispositionCatalogTests(unittest.TestCase):
    def test_live_disposition_catalogues_have_consumed_case_identity(self) -> None:
        dispositions = builder.CanonicalCatalog._load_dispositions()
        design_ids = {
            row["case_id"]
            for row in builder.read_catalog_csv(
                builder.DESIGN_PATH, builder.DESIGN_CATALOG_FIELDS
            )
        }

        self.assertTrue(dispositions)
        self.assertEqual(set(dispositions) - design_ids, set())
        for case_id in (
            "TA-AST-017",
            "TA-AST-223",
            "TA-AST-224",
            "TS-SEMA-077",
            "TS-SEMA-094",
            "TS-SRC-049",
        ):
            with self.subTest(case_id=case_id):
                self.assertEqual(dispositions[case_id], "InfrastructurePending")

    def test_identity_must_be_exactly_one_supported_nonempty_shape(self) -> None:
        path = Path("SyntheticCaseClassifications.jsonl")
        invalid_rows = (
            {},
            {"case_id": "T-001", "case_ids": ["T-001"]},
            {"case_ids": []},
            {"case_ids": ["T-001", "T-001"]},
            {"affected_case_ids": ["T-001", 2]},
        )
        for row in invalid_rows:
            with self.subTest(row=row):
                with self.assertRaisesRegex(ValueError, "identity"):
                    builder.parse_disposition_row(path, row)

    def test_case_range_must_be_canonical_and_ascending(self) -> None:
        path = Path("SyntheticContractCatalog.jsonl")
        invalid_ranges = (
            "T-002..T-001",
            "T-01..X-02",
            "T-01..T-002",
            "not-a-range",
        )
        for value in invalid_ranges:
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "range"):
                    builder.parse_disposition_row(
                        path, {"case_range": value, "disposition": "Executable"}
                    )

    def test_disposition_is_known_or_narrowly_inferred(self) -> None:
        with self.assertRaisesRegex(ValueError, "disposition"):
            builder.parse_disposition_row(
                Path("Synthetic.jsonl"),
                {"case_id": "T-001", "disposition": "ProbablyFine"},
            )
        with self.assertRaisesRegex(ValueError, "disposition"):
            builder.parse_disposition_row(
                Path("Synthetic.jsonl"), {"case_id": "T-001"}
            )

        case_ids, disposition = builder.parse_disposition_row(
            Path("SelfHostBootstrapInfrastructureCatalog.jsonl"),
            {"case_ids": ["T-001"]},
        )
        self.assertEqual(case_ids, ("T-001",))
        self.assertEqual(disposition, "InfrastructurePending")

    def test_only_inventoried_metadata_files_may_omit_case_identity(self) -> None:
        for name in builder.DISPOSITION_METADATA_ONLY_NAMES:
            with self.subTest(name=name):
                self.assertIsNone(
                    builder.parse_disposition_row(Path(name), {"id": "CONFLICT-1"})
                )
        with self.assertRaisesRegex(ValueError, "identity"):
            builder.parse_disposition_row(
                Path("UnexpectedPlanContradictions.jsonl"), {"id": "CONFLICT-1"}
            )

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
