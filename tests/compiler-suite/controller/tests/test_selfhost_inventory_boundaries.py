from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
SOURCE_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

from selfhost_inventory import (
    classification_sets,
    load_component_inventory,
    read_jsonl,
    validate_manifest_cases,
)


class SelfHostInventoryBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.diagnostics = load_component_inventory(
            REPOSITORY_ROOT, "SelfHost.Diagnostics"
        )
        manifest = read_jsonl(SOURCE_ROOT / "manifest" / "cases.jsonl")
        cls.manifest_row = next(
            row
            for row in manifest
            if row.get("component") == "SelfHost.Diagnostics"
        )
        cls.types = load_component_inventory(REPOSITORY_ROOT, "SelfHost.Types")
        design = next(iter(cls.types.design_by_id.values()))
        cls.classification_row: dict[str, object] = {
            "case_id": design["case_id"],
            "contract_id": design["contract_id"],
            "component": "SelfHost.Types",
            "disposition": "PlanContradiction",
        }

    def classification_from(self, row: dict[str, object]) -> dict[str, frozenset[str]]:
        with tempfile.TemporaryDirectory(
            prefix="selfhost-classification-", dir=Path.cwd()
        ) as temporary:
            path = Path(temporary) / "classification.jsonl"
            path.write_text(json.dumps(row) + "\n", encoding="utf-8", newline="\n")
            return classification_sets(self.types, path)

    def test_manifest_identity_fields_reject_string_coercions(self) -> None:
        for field in ("case_id", "contract_id", "layer", "profile"):
            for invalid in (7, True, ""):
                with self.subTest(field=field, invalid=invalid):
                    row = dict(self.manifest_row)
                    row[field] = invalid
                    with self.assertRaisesRegex(
                        RuntimeError, rf"{field} must be a non-empty string"
                    ):
                        validate_manifest_cases(self.diagnostics, [row])

    def test_classification_identity_fields_reject_string_coercions(self) -> None:
        for field in ("case_id", "contract_id", "disposition"):
            for invalid in (7, True, ""):
                with self.subTest(field=field, invalid=invalid):
                    row = dict(self.classification_row)
                    row[field] = invalid
                    with self.assertRaisesRegex(
                        RuntimeError, rf"{field} must be a non-empty string"
                    ):
                        self.classification_from(row)

    def test_optional_classification_component_is_an_exact_string(self) -> None:
        for invalid in (7, True, ""):
            with self.subTest(invalid=invalid):
                row = dict(self.classification_row)
                row["component"] = invalid
                with self.assertRaisesRegex(
                    RuntimeError, r"component must be a non-empty string"
                ):
                    self.classification_from(row)

    def test_live_classification_catalogs_satisfy_the_boundary(self) -> None:
        scenarios = (
            (
                "Host.Source",
                SOURCE_ROOT
                / "host"
                / "modules"
                / "HostSourceCaseClassifications.jsonl",
            ),
            (
                "SelfHost.Basic",
                SOURCE_ROOT
                / "selfhost"
                / "modules"
                / "SelfHostBasicPlanContradictions.jsonl",
            ),
            (
                "SelfHost.Source",
                SOURCE_ROOT
                / "selfhost"
                / "modules"
                / "SelfHostSourceCaseClassifications.jsonl",
            ),
            (
                "SelfHost.Types",
                SOURCE_ROOT
                / "selfhost"
                / "modules"
                / "SelfHostTypesCaseClassifications.jsonl",
            ),
        )
        for component, path in scenarios:
            with self.subTest(component=component):
                inventory = load_component_inventory(REPOSITORY_ROOT, component)
                classification_sets(inventory, path)


if __name__ == "__main__":
    unittest.main()
