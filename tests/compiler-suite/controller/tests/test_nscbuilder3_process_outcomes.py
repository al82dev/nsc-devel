from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


def variant() -> SimpleNamespace:
    case = SimpleNamespace(
        case_id="TSH-00001",
        contract_id="CSH-BAS-001",
        component="SelfHost.Basic",
        wave=7,
        executor="nsc_suite_controller+nsc",
    )
    return SimpleNamespace(
        key="TSH-00001|Debug|O0",
        case=case,
        build_mode="Debug",
        optimization="O0",
    )


class Builder3ProcessOutcomeTests(unittest.TestCase):
    def engine(self) -> builder.ExecutionEngine:
        return builder.ExecutionEngine(
            builder.BuilderConfig(),
            SimpleNamespace(root=Path("A:/synthetic-history")),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )

    def test_active_cancellation_overrides_partial_product_result(self) -> None:
        engine = self.engine()
        engine.process.cancelled.set()
        with patch.object(
            engine,
            "_run_selfhost",
            return_value=("KNOWN_FAILURE", "partial mismatch", "fixture.log"),
        ):
            outcome = engine._run_variant(variant())
        self.assertEqual(outcome.status, "CANCELLED")
        self.assertEqual(outcome.detail, "cancelled by user")
        self.assertEqual(outcome.evidence, "fixture.log")

if __name__ == "__main__":
    unittest.main()
