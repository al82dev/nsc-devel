from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[2] / "tools" / "verify_native_sharp_304_plan.py"
SPEC = importlib.util.spec_from_file_location("verify_native_sharp_304_plan", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class NativeSharp304HarnessTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.plan = self.root / "plan.md"
        self.tracking = self.root / "tracking.md"
        self.contracts = self.root / "contracts"
        self.contracts.mkdir()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_contract(self, payload: object) -> None:
        (self.contracts / "contract.json").write_text(json.dumps(payload), encoding="utf-8")

    def write_items(self, plan_ids: list[str], tracking_ids: list[str]) -> None:
        self.plan.write_text("\n".join(f"- `{item}` — plan" for item in plan_ids), encoding="utf-8")
        self.tracking.write_text(
            "\n".join(f"- [{'x' if index == 0 else ' '}] `{item}` — tracking"
                       for index, item in enumerate(tracking_ids)),
            encoding="utf-8",
        )

    def test_discovers_denominator_from_matching_sources(self) -> None:
        items = ["NS304-T00-F01", "NS304-T01-F01", "NS304-T01-F02"]
        self.write_items(items, items)
        self.write_contract({"schema": "NS304_TEST", "schema_version": 1, "release": "3.0.4"})

        result = MODULE.verify(self.plan, self.tracking, self.contracts)

        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["discovered"]["plan_items"], len(items))
        self.assertEqual(result["discovered"]["closed_items"], 1)
        self.assertEqual(result["issues"], [])

    def test_new_plan_item_requires_tracking_without_fixed_count(self) -> None:
        self.write_items(["NS304-T00-F01", "NS304-T00-F02"], ["NS304-T00-F01"])
        self.write_contract({"schema": "NS304_TEST", "schema_version": 1, "release": "3.0.4"})

        result = MODULE.verify(self.plan, self.tracking, self.contracts)

        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["issues"], [{
            "code": "TRACKING_MISSING_ITEM",
            "path": str(self.tracking),
            "item_id": "NS304-T00-F02",
        }])

    def test_contract_failures_use_structured_codes(self) -> None:
        self.write_items(["NS304-T00-F01"], ["NS304-T00-F01"])
        self.write_contract({"schema": "foreign", "schema_version": 2, "release": "future"})

        result = MODULE.verify(self.plan, self.tracking, self.contracts)

        self.assertEqual(
            [issue["code"] for issue in result["issues"]],
            ["CONTRACT_SCHEMA_INVALID", "CONTRACT_SCHEMA_VERSION_INVALID", "CONTRACT_RELEASE_INVALID"],
        )

    def test_invalid_json_is_terminal_without_matching_exception_text(self) -> None:
        self.write_items(["NS304-T00-F01"], ["NS304-T00-F01"])
        (self.contracts / "contract.json").write_text("{", encoding="utf-8")

        result = MODULE.verify(self.plan, self.tracking, self.contracts)

        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["issues"][0]["code"], "CONTRACT_JSON_INVALID")


if __name__ == "__main__":
    unittest.main()
