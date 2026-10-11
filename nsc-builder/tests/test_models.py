from __future__ import annotations

import sys
import unittest
from pathlib import Path


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder._runtime import legacy
from nsc_builder.models import Run, TestCase, TestExecution, TestVariant


def test_case() -> TestCase:
    return TestCase(
        case_id="TI-MODULAR-001",
        contract_id="CI-MODULAR-001",
        component="Infrastructure",
        wave=1,
        purpose="prove canonical model sharing",
        layer="unit",
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        executor="nsc_component_tests",
        disposition="Executable",
        objective="use one definition across projections",
        expected_outcome="PASS",
        manifest={},
    )


class CanonicalModelTests(unittest.TestCase):
    def test_runtime_uses_the_canonical_model_classes(self) -> None:
        runtime = legacy()

        self.assertIs(runtime.CaseRecord, TestCase)
        self.assertIs(runtime.RunVariant, TestVariant)
        self.assertIs(runtime.CaseOutcome, TestExecution)
        self.assertIs(runtime.HistoricalExecution, Run)

    def test_variant_and_execution_share_one_identity(self) -> None:
        case = test_case()
        variant = TestVariant(case, "Debug", "NA")
        execution = TestExecution(
            key=variant.key,
            case_id=case.case_id,
            contract_id=case.contract_id,
            component=case.component,
            wave=case.wave,
            build_mode=variant.build_mode,
            optimization=variant.optimization,
            status="PASS",
            profile=case.profile,
            timeout_pass_count=2,
            timeout_average_seconds=0.25,
            timeout_limit_seconds=5,
        )

        self.assertTrue(case.executable)
        self.assertEqual(variant.key, "TI-MODULAR-001|Debug|NA")
        self.assertEqual(execution.key, variant.key)
        self.assertEqual(execution.profile, case.profile)

    def test_run_aliases_do_not_duplicate_case_or_execution_storage(self) -> None:
        case = test_case()
        variant = TestVariant(case, "Debug", "NA")
        execution = TestExecution(
            key=variant.key,
            case_id=case.case_id,
            contract_id=case.contract_id,
            component=case.component,
            wave=case.wave,
            build_mode=variant.build_mode,
            optimization=variant.optimization,
            status="PASS",
        )
        run = Run(
            root=Path("A:/nsc-devel/_nscbuilder3/history/synthetic"),
            metadata={"run_id": "synthetic"},
            status="PASSED",
            cases=(case,),
            variants=(variant,),
            outcomes=(execution,),
            full_log="",
        )

        self.assertIs(run.tests, run.cases)
        self.assertIs(run.executions, run.outcomes)


if __name__ == "__main__":
    unittest.main()
