from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
CONTROLLER_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite" / "controller"
sys.path.insert(0, str(REPOSITORY_ROOT))
sys.path.insert(0, str(CONTROLLER_ROOT))

import nscbuilder3 as builder


class Builder3DeadlineRemovalTests(unittest.TestCase):
    def test_live_manifest_has_no_automatic_timeout_contract(self) -> None:
        cases = builder.CanonicalCatalog.load().cases
        self.assertTrue(cases)
        for case in cases:
            self.assertNotIn("timeout_class", case.manifest, case.case_id)
            self.assertNotIn(
                "timeout_is_contract_failure",
                case.manifest.get("oracle", {}),
                case.case_id,
            )

    def test_watchdog_cases_are_future_only(self) -> None:
        catalog = builder.CanonicalCatalog.load()
        self.assertNotIn("TA-AST-223", catalog.by_id)
        self.assertNotIn("TA-AST-224", catalog.by_id)

    def test_selfhost_gates_only_budget_fixture_compilation(self) -> None:
        integration = (
            REPOSITORY_ROOT / "tests" / "compiler-suite" / "controller" / "integration"
        )
        for name in ("basic", "diagnostics", "lexer", "source", "types"):
            source = integration.joinpath(f"run_selfhost_{name}_gate.py").read_text(
                encoding="utf-8"
            )
            with self.subTest(name=name):
                self.assertIn("timeout: float | None = None", source)
                self.assertIn('{"timeout": timeout} if timeout is not None else {}', source)
                self.assertNotIn("timeout=timeout", source)
                self.assertRegex(source, r"compiled = run_process\([\s\S]*?900\.0\)")


if __name__ == "__main__":
    unittest.main()
