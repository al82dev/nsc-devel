from __future__ import annotations

import sys
import unittest
from pathlib import Path


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder.catalog import (
    CanonicalCatalog,
    read_catalog_csv,
    validate_allocation_domains,
)
from nsc_builder.constants import ALLOCATION_CATALOG_FIELDS
from nsc_builder.paths import ALLOCATION_PATH


class CatalogAllocationTests(unittest.TestCase):
    def test_real_tl_lang_302_uses_canonical_not_applicable_optimization(self) -> None:
        rows = read_catalog_csv(ALLOCATION_PATH, ALLOCATION_CATALOG_FIELDS)
        allocation = next(row for row in rows if row["case_id"] == "TL-LANG-302")

        self.assertEqual(allocation["planned_executor"], "nsc_component_tests")
        self.assertEqual(allocation["supported_optimizations"], "NA")
        validate_allocation_domains([allocation])
        self.assertEqual(CanonicalCatalog._optimizations(allocation), ("NA",))

    def test_complete_live_catalog_loads_without_gui(self) -> None:
        catalog = CanonicalCatalog.load()
        case = catalog.by_id["TL-LANG-302"]

        self.assertEqual(case.executor, "nsc_component_tests")
        self.assertEqual(case.optimizations, ("NA",))

    def test_not_applicable_optimization_is_an_exclusive_sentinel(self) -> None:
        base = {
            "case_id": "SYNTHETIC-NA",
            "component": "Host.Language",
            "layer": "component",
            "planned_executor": "nsc_component_tests",
            "default_profile": "PR",
            "supported_build_modes": "Debug",
            "supported_optimizations": "NA;O0",
            "isolation": "RESET_PER_CASE",
        }

        with self.assertRaisesRegex(ValueError, "mixes sentinel"):
            validate_allocation_domains([base])

        for invalid in ("Ofast", "NA;NA"):
            with self.subTest(invalid=invalid):
                row = dict(base, supported_optimizations=invalid)
                with self.assertRaises(ValueError):
                    validate_allocation_domains([row])


if __name__ == "__main__":
    unittest.main()
