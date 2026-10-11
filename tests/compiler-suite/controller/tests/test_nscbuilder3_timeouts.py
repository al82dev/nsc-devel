from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
CONTROLLER_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite" / "controller"
sys.path.insert(0, str(REPOSITORY_ROOT))
sys.path.insert(0, str(CONTROLLER_ROOT))

import nscbuilder3 as builder


def case(case_id: str = "TI-TIMEOUT-001") -> builder.CaseRecord:
    return builder.CaseRecord(
        case_id=case_id,
        contract_id="CI-TIMEOUT-001",
        component="Host.Basic",
        wave=1,
        purpose="exercise per-variant timeout control",
        layer="unit",
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        executor="nsc_component_tests",
        disposition="Executable",
        objective="keep timeout state scoped to one variant",
        expected_outcome="PASS",
        manifest={},
    )


class FakeStore:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.events: list[tuple[str, dict[str, object]]] = []
        self.lines: list[str] = []

    def event(self, kind: str, payload: dict[str, object]) -> None:
        self.events.append((kind, payload))

    def log(self, line: str) -> None:
        self.lines.append(line)


class ImmediateTimer:
    def __init__(self, _seconds: float, callback: object) -> None:
        self.callback = callback
        self.daemon = False

    def start(self) -> None:
        self.callback()  # type: ignore[operator]

    def cancel(self) -> None:
        pass


class Builder3VariantTimeoutTests(unittest.TestCase):
    def test_policy_boundaries_round_up_without_reducing_margin(self) -> None:
        expectations = {
            None: 2700,
            0.0: 5,
            0.999: 5,
            1.0: 6,
            30.0: 35,
            30.01: 46,
            120.0: 135,
            120.01: 301,
            600.0: 780,
            600.01: 901,
            1800.0: 2160,
        }
        for average, expected in expectations.items():
            with self.subTest(average=average):
                self.assertEqual(builder.timeout_limit_for_average(average), expected)

    def test_history_uses_only_valid_pass_for_exact_variant_profile(self) -> None:
        selected = builder.RunVariant(case(), "Debug", "NA")
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            history_root = Path(directory)
            for run_id, rows in (
                (
                    "pass-a",
                    [
                        {
                            "case_id": selected.case.case_id,
                            "build_mode": "Debug",
                            "optimization": "NA",
                            "status": "PASS",
                            "duration_seconds": 2.0,
                        },
                        {
                            "case_id": selected.case.case_id,
                            "build_mode": "Debug",
                            "optimization": "NA",
                            "status": "FAIL",
                            "duration_seconds": 1000.0,
                        },
                    ],
                ),
                (
                    "pass-b",
                    [
                        {
                            "case_id": selected.case.case_id,
                            "build_mode": "Debug",
                            "optimization": "NA",
                            "status": "PASS",
                            "duration_seconds": 4.0,
                        },
                        {
                            "case_id": selected.case.case_id,
                            "build_mode": "Release",
                            "optimization": "NA",
                            "status": "PASS",
                            "duration_seconds": 2000.0,
                        },
                    ],
                ),
            ):
                root = history_root / run_id
                root.mkdir()
                (root / "results.json").write_text(
                    json.dumps({"results": rows}), encoding="utf-8"
                )
            with patch.object(
                builder,
                "history_record",
                return_value=("run", "phase-2-suite", "PASSED", "", ""),
            ) as record:
                budgets = builder.load_variant_timeout_budgets(
                    (selected,), history_root
                )

            budget = budgets[builder.variant_history_key(selected)]
            self.assertEqual(record.call_count, 2)
            self.assertEqual(budget.pass_count, 2)
            self.assertEqual(budget.average_seconds, 3.0)
            self.assertEqual(budget.limit_seconds, 8)

    def test_variant_cancellation_scope_is_not_sticky(self) -> None:
        controller = builder.ProcessController(lambda _line: None)
        first = controller.begin_variant()
        self.assertTrue(controller.cancel_current("case-cancelled", first))
        self.assertEqual(controller.end_variant(first), "case-cancelled")
        self.assertFalse(controller.cancelled.is_set())

        second = controller.begin_variant()
        self.assertIsNone(controller.end_variant(second))

    def test_timeout_writes_timing_evidence_and_terminal_state(self) -> None:
        selected = builder.RunVariant(case(), "Debug", "NA")
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            store = FakeStore(Path(directory))
            budget = builder.VariantTimeoutBudget(2, 3.0, 8)
            engine = builder.ExecutionEngine(
                builder.BuilderConfig(),
                store,  # type: ignore[arg-type]
                lambda _line: None,
                lambda *_args: None,
                lambda *_args: None,
                {builder.variant_history_key(selected): budget},
            )
            engine.active_key = selected.key
            with (
                patch.object(builder.threading, "Timer", ImmediateTimer),
                patch.object(
                    engine,
                    "_run_variant",
                    return_value=builder.CaseOutcome(
                        key=selected.key,
                        case_id=selected.case.case_id,
                        contract_id=selected.case.contract_id,
                        component=selected.case.component,
                        wave=selected.case.wave,
                        build_mode=selected.build_mode,
                        optimization=selected.optimization,
                        status="PASS",
                    ),
                ),
            ):
                outcome = engine._run_controlled_variant(selected)

            self.assertEqual(outcome.status, "TIMEOUT")
            self.assertEqual(outcome.timeout_pass_count, 2)
            self.assertEqual(outcome.timeout_average_seconds, 3.0)
            self.assertEqual(outcome.timeout_limit_seconds, 8)
            self.assertTrue(Path(outcome.evidence).is_file())
            payload = json.loads(Path(outcome.evidence).read_text(encoding="utf-8"))
            self.assertEqual(payload["status"], "TIMEOUT")
            self.assertEqual(payload["historical_pass_average_seconds"], 3.0)
            self.assertEqual(payload["timeout_limit_seconds"], 8)
            self.assertIn("elapsed_seconds", payload)

    def test_manual_cancel_finishes_one_variant_and_run_continues(self) -> None:
        first = builder.RunVariant(case("TI-TIMEOUT-001"), "Debug", "NA")
        second = builder.RunVariant(case("TI-TIMEOUT-002"), "Debug", "NA")
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            store = FakeStore(Path(directory))
            engine = builder.ExecutionEngine(
                builder.BuilderConfig(),
                store,  # type: ignore[arg-type]
                lambda _line: None,
                lambda *_args: None,
                lambda *_args: None,
            )

            def dispatch(variant: builder.RunVariant) -> builder.CaseOutcome:
                if variant.key == first.key:
                    engine.process.cancel_current("case-cancelled")
                return builder.CaseOutcome(
                    key=variant.key,
                    case_id=variant.case.case_id,
                    contract_id=variant.case.contract_id,
                    component=variant.case.component,
                    wave=variant.case.wave,
                    build_mode=variant.build_mode,
                    optimization=variant.optimization,
                    status="PASS",
                )

            with patch.object(engine, "_run_variant", side_effect=dispatch):
                outcomes, cancelled = engine.run((first, second))

            self.assertFalse(cancelled)
            self.assertEqual([item.status for item in outcomes], ["CANCELLED", "PASS"])
            self.assertFalse(engine.process.cancelled.is_set())


if __name__ == "__main__":
    unittest.main()
