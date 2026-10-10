from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path
from unittest import mock


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
SOURCE_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

import run_buildsystem_gate as buildsystem_gate
import run_packaging_gate as packaging_gate
from component_gate_support import component_manifest_rows, read_jsonl


class SystemCatalogBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.build_cases = component_manifest_rows(
            SOURCE_ROOT, REPOSITORY_ROOT, "BuildSystem"
        )
        (
            cls.build_design,
            cls.build_contracts,
        ) = buildsystem_gate.validate_lineage(REPOSITORY_ROOT, cls.build_cases)
        build_root = SOURCE_ROOT / "buildsystem"
        cls.build_catalog = read_jsonl(
            build_root / "BuildSystemContractCatalog.jsonl"
        )
        cls.build_classified = read_jsonl(
            build_root / "BuildSystemCaseClassifications.jsonl"
        )

        cls.packaging_cases = component_manifest_rows(
            SOURCE_ROOT, REPOSITORY_ROOT, "Packaging"
        )
        (
            cls.packaging_design,
            cls.packaging_contracts,
            cls.profile_mismatches,
        ) = packaging_gate.validate_lineage(REPOSITORY_ROOT, cls.packaging_cases)
        packaging_root = SOURCE_ROOT / "packaging"
        cls.packaging_catalog = read_jsonl(
            packaging_root / "PackagingContractCatalog.jsonl"
        )
        cls.packaging_classified = read_jsonl(
            packaging_root / "PackagingCaseClassifications.jsonl"
        )
        cls.packaging_contradictions = read_jsonl(
            packaging_root / "PackagingPlanContradictions.jsonl"
        )

    def validate_buildsystem(
        self,
        catalog: list[dict[str, object]],
        classified: list[dict[str, object]],
    ) -> None:
        with mock.patch.object(
            buildsystem_gate,
            "read_jsonl",
            side_effect=(catalog, classified),
        ):
            buildsystem_gate.validate_classifications(
                REPOSITORY_ROOT,
                self.build_cases,
                self.build_design,
                self.build_contracts,
            )

    def validate_packaging(
        self,
        catalog: list[dict[str, object]],
        classified: list[dict[str, object]],
        contradictions: list[dict[str, object]],
    ) -> None:
        with mock.patch.object(
            packaging_gate,
            "read_jsonl",
            side_effect=(catalog, classified, contradictions),
        ):
            packaging_gate.validate_classifications(
                REPOSITORY_ROOT,
                self.packaging_cases,
                self.packaging_design,
                self.packaging_contracts,
                self.profile_mismatches,
            )

    def test_live_system_catalogs_satisfy_exact_boundaries(self) -> None:
        self.validate_buildsystem(
            copy.deepcopy(self.build_catalog),
            copy.deepcopy(self.build_classified),
        )
        self.validate_packaging(
            copy.deepcopy(self.packaging_catalog),
            copy.deepcopy(self.packaging_classified),
            copy.deepcopy(self.packaging_contradictions),
        )

    def test_buildsystem_catalogs_reject_string_coercions(self) -> None:
        scenarios = (
            ("catalog", "case_range", 7, "case range must be a non-empty string"),
            ("catalog", "disposition", 7, "must be a non-empty string"),
            ("classified", "contract_id", 7, "must be a non-empty string"),
            ("classified", "disposition", 7, "must be a non-empty string"),
        )
        for target, field, invalid, message in scenarios:
            with self.subTest(target=target, field=field):
                catalog = copy.deepcopy(self.build_catalog)
                classified = copy.deepcopy(self.build_classified)
                rows = catalog if target == "catalog" else classified
                rows[0][field] = invalid
                with self.assertRaisesRegex(RuntimeError, message):
                    self.validate_buildsystem(catalog, classified)

    def test_packaging_catalogs_reject_string_coercions(self) -> None:
        scenarios = (
            ("catalog", "case_ids", "T12B-00457", "must be an array"),
            ("catalog", "case_ids", [457], "must contain non-empty strings"),
            ("catalog", "disposition", 7, "must be a non-empty string"),
            ("classified", "contract_id", 7, "must be a non-empty string"),
            ("classified", "disposition", 7, "must be a non-empty string"),
            ("contradiction", "scope", 7, "must be a non-empty string"),
            ("contradiction", "field", 7, "must be a non-empty string"),
        )
        for target, field, invalid, message in scenarios:
            with self.subTest(target=target, field=field):
                catalog = copy.deepcopy(self.packaging_catalog)
                classified = copy.deepcopy(self.packaging_classified)
                contradictions = copy.deepcopy(self.packaging_contradictions)
                if target == "catalog":
                    rows = catalog
                elif target == "classified":
                    rows = classified
                else:
                    rows = contradictions
                rows[0][field] = invalid
                with self.assertRaisesRegex(RuntimeError, message):
                    self.validate_packaging(catalog, classified, contradictions)


if __name__ == "__main__":
    unittest.main()
