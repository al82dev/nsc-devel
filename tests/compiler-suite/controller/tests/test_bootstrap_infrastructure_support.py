from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

from selfhost_bootstrap_infrastructure_support import load_contracts, parse_catalog_row


REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
CATALOG = (
    REPOSITORY_ROOT
    / "tests"
    / "compiler-suite"
    / "selfhost"
    / "bootstrap-infrastructure"
    / "SelfHostBootstrapInfrastructureCatalog.jsonl"
)


class BootstrapInfrastructureSupportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.valid_row: dict[str, object] = {
            "contract_id": "C12B-BSI-001",
            "file_id": "PF-000471",
            "source_path": "selfhost/bootstrap/bootstrap-profile.json",
            "category": "BOOTSTRAP_DATA",
            "language": "JSON",
            "purpose": "Bootstrap profile data",
            "oracle": "typed parsing",
            "case_ids": ["T12B-01081"],
        }

    def test_catalog_row_rejects_duplicate_nested_keys(self) -> None:
        catalog = Path("synthetic-catalog.jsonl")
        with self.assertRaisesRegex(
            RuntimeError,
            r"synthetic-catalog\.jsonl:7:.*duplicate JSON object key: case_ids",
        ):
            parse_catalog_row(
                '{"contract_id":"CSH-001","case_ids":["A"],"case_ids":["B"]}',
                catalog,
                7,
            )

    def test_catalog_row_requires_exact_fields(self) -> None:
        missing = dict(self.valid_row)
        del missing["oracle"]
        unknown = dict(self.valid_row)
        unknown["extra"] = True
        for label, row, message in (
            ("missing", missing, "missing fields: oracle"),
            ("unknown", unknown, "unknown fields: extra"),
        ):
            with self.subTest(label=label):
                with self.assertRaisesRegex(RuntimeError, message):
                    parse_catalog_row(
                        json.dumps(row), Path("synthetic-catalog.jsonl"), 11
                    )

    def test_catalog_row_rejects_string_coercions(self) -> None:
        for field in (
            "contract_id",
            "file_id",
            "source_path",
            "category",
            "language",
            "purpose",
            "oracle",
        ):
            for invalid in (7, True, ""):
                with self.subTest(field=field, invalid=invalid):
                    row = dict(self.valid_row)
                    row[field] = invalid
                    with self.assertRaisesRegex(
                        RuntimeError, rf"{field} must be a non-empty string"
                    ):
                        parse_catalog_row(
                            json.dumps(row), Path("synthetic-catalog.jsonl"), 13
                        )

    def test_catalog_row_requires_unique_non_empty_case_ids(self) -> None:
        scenarios = (
            ("scalar", "T12B-01081", "case_ids must be a non-empty array"),
            ("empty", [], "case_ids must be a non-empty array"),
            ("integer", [1081], "case_ids must contain non-empty strings"),
            ("blank", [""], "case_ids must contain non-empty strings"),
            (
                "duplicate",
                ["T12B-01081", "T12B-01081"],
                "case_ids contains duplicates",
            ),
        )
        for label, invalid, message in scenarios:
            with self.subTest(label=label):
                row = dict(self.valid_row)
                row["case_ids"] = invalid
                with self.assertRaisesRegex(RuntimeError, message):
                    parse_catalog_row(
                        json.dumps(row), Path("synthetic-catalog.jsonl"), 17
                    )

    def test_live_catalog_satisfies_the_exact_schema(self) -> None:
        contracts = load_contracts(REPOSITORY_ROOT, CATALOG)
        self.assertTrue(contracts)
        self.assertTrue(all(contract.case_ids for contract in contracts))


if __name__ == "__main__":
    unittest.main()
