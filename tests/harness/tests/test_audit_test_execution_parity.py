"""Directed tests for the one-to-one historical execution audit."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "audit_test_execution_parity",
    ROOT / "tools" / "audit_test_execution_parity.py",
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class AuditTestExecutionParityTests(unittest.TestCase):
    """Prove that execution and final migration remain independent dimensions."""

    def test_parse_unit_catalog_requires_exact_declared_count(self) -> None:
        catalog = MODULE.parse_unit_catalog("a\tparser\tone\nb\tsema\ttwo\ncases=2\n")
        self.assertEqual(catalog, {"a": "parser", "b": "sema"})
        with self.assertRaises(MODULE.ParityError):
            MODULE.parse_unit_catalog("a\tparser\tone\ncases=2\n")

    def test_manifest_target_is_runnable_and_complete(self) -> None:
        row = MODULE.classify_row(
            self._legacy("a", "MIGRATE_NS_FIXTURE"),
            {},
            {"a": Path("tests/cases/a/case.json")},
            {},
        )
        self.assertEqual(row.execution_state, "RUNNABLE")
        self.assertEqual(row.migration_state, "TARGET_RUNNABLE")

    def test_unit_adapter_does_not_complete_fixture_migration(self) -> None:
        row = MODULE.classify_row(
            self._legacy("a", "MIGRATE_NS_FIXTURE"),
            {"a": "sema"},
            {},
            {},
        )
        self.assertEqual(row.execution_state, "RUNNABLE")
        self.assertEqual(row.migration_state, "TRANSITIONAL_WRONG_LAYER")

    def test_multiple_paths_never_count_as_final_migration(self) -> None:
        row = MODULE.classify_row(
            self._legacy("a", "MIGRATE_NS_FIXTURE"),
            {"a": "sema"},
            {"a": Path("tests/cases/a/case.json")},
            {},
        )
        self.assertEqual(row.execution_state, "AMBIGUOUS_MULTIPLE_PATHS")
        self.assertEqual(row.migration_state, "TARGET_AMBIGUOUS_MULTIPLE_PATHS")

    def test_blocked_specialized_suite_is_not_runnable(self) -> None:
        row = MODULE.classify_row(
            self._legacy("a", "KEEP_SPECIALIZED"),
            {},
            {},
            {"a": (False, "non-O0 owner")},
        )
        self.assertEqual(row.execution_state, "REGISTERED_BUT_BLOCKED")
        self.assertEqual(row.migration_state, "TARGET_REGISTERED_BUT_BLOCKED")

    def test_missing_identifier_remains_visible(self) -> None:
        row = MODULE.classify_row(
            self._legacy("a", "KEEP_CPP_UNIT"),
            {},
            {},
            {},
        )
        self.assertEqual(row.execution_state, "UNMAPPED")
        self.assertEqual(row.migration_state, "TARGET_UNMAPPED")

    @staticmethod
    def _legacy(identifier: str, destination: str) -> dict[str, object]:
        """Create the smallest valid historical row for a pure classification test."""

        return {
            "ctest_id": identifier,
            "destination": destination,
            "primary_component": "test",
            "owner_path": "tests/unit/Test.cpp",
            "expectation_contract": "contract",
            "expectation_signals": ["exit"],
        }


if __name__ == "__main__":
    unittest.main()
