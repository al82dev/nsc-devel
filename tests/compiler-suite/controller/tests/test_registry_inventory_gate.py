from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
SUITE_ROOT = CONTROLLER_ROOT.parent
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

from run_registry_inventory_gate import (
    expected_registry_rows,
    parse_registry_output,
)


class RegistryInventoryGateTests(unittest.TestCase):
    def test_expected_registry_is_unique_and_round_trips(self) -> None:
        expected = expected_registry_rows(REPOSITORY_ROOT, SUITE_ROOT)
        self.assertTrue(expected)
        encoded = "\n".join("\t".join(row) for row in expected) + "\n"
        self.assertEqual(parse_registry_output(encoded), expected)

    def test_malformed_duplicate_and_unsorted_rows_are_rejected(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "malformed"):
            parse_registry_output("only-one-field\n")
        with self.assertRaisesRegex(RuntimeError, "duplicate"):
            parse_registry_output("A\tC\tHost.A\nA\tC\tHost.A\n")
        with self.assertRaisesRegex(RuntimeError, "not ordered"):
            parse_registry_output("B\tC\tHost.B\nA\tC\tHost.A\n")


if __name__ == "__main__":
    unittest.main()
