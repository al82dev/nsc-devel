from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

from selfhost_bootstrap_infrastructure_support import parse_catalog_row


class BootstrapInfrastructureSupportTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
