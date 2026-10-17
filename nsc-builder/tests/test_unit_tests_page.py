from __future__ import annotations

import sys
import unittest
from pathlib import Path


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder.catalog import CanonicalCatalog
from nsc_builder.unit_tests_page import UnitTestsPageMixin


class UnitTestsPageImportTests(unittest.TestCase):
    def test_start_unit_suite_resolves_canonical_catalog(self) -> None:
        """Keep the unit-suite entry point bound to the canonical catalog loader."""

        entry_point_globals = UnitTestsPageMixin._start_unit_suite.__globals__
        self.assertIs(entry_point_globals.get("CanonicalCatalog"), CanonicalCatalog)


if __name__ == "__main__":
    unittest.main()
