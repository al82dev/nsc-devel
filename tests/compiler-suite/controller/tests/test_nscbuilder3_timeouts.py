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

    def test_selfhost_gates_have_no_automatic_deadline(self) -> None:
        integration = (
            REPOSITORY_ROOT / "tests" / "compiler-suite" / "controller" / "integration"
        )
        for name in ("basic", "diagnostics", "lexer", "source", "types"):
            source = integration.joinpath(f"run_selfhost_{name}_gate.py").read_text(
                encoding="utf-8"
            )
            with self.subTest(name=name):
                self.assertNotIn("timeout", source.casefold())

    def test_process_controller_has_no_automatic_deadline_api(self) -> None:
        source = (REPOSITORY_ROOT / "nscbuilder3.py").read_text(encoding="utf-8")
        controller_source = source.split("class ProcessController:", 1)[1].split(
            "def effective_build_dir", 1
        )[0]
        self.assertNotIn("last_termination_reason = \"timeout\"", controller_source)
        self.assertNotIn("timeout: float | None", controller_source)


if __name__ == "__main__":
    unittest.main()
