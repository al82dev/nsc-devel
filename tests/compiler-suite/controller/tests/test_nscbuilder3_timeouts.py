from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
CONTROLLER_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite" / "controller"
sys.path.insert(0, str(REPOSITORY_ROOT))
sys.path.insert(0, str(CONTROLLER_ROOT))

import nscbuilder3 as builder
from nsc_suite.manifest import TIMEOUT_SECONDS as MANIFEST_TIMEOUT_SECONDS


class Builder3TimeoutTests(unittest.TestCase):
    def test_nightly_dominance_scale_cases_use_the_extended_budget(self) -> None:
        expected = {
            "TF-DOM-024",
            "TF-DOM-032",
            "TF-DOM-040",
            "TF-DOM-072",
            "TF-DOM-080",
            "TF-DOM-104",
            "TF-DOM-128",
            "TF-DOM-216",
            "TF-DOM-224",
            "TF-DOM-232",
            "TF-DOM-256",
        }
        cases = {
            case.case_id: case
            for case in builder.CanonicalCatalog.load().cases
            if case.case_id in expected
        }

        self.assertEqual(set(cases), expected)
        for case_id, case in cases.items():
            with self.subTest(case_id=case_id):
                self.assertEqual(case.profile, "NIGHTLY")
                self.assertEqual(case.manifest["timeout_class"], "extended")
                self.assertIs(
                    case.manifest["oracle"]["timeout_is_contract_failure"], True
                )

    def test_explicit_host_timeout_is_a_contract_failure(self) -> None:
        case = builder.CanonicalCatalog.load().by_id["TF-DOM-024"]
        variant = builder.RunVariant(case, "Debug", "NA")
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(),
            type("Store", (), {"root": Path("A:/synthetic-history")})(),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )

        def timed_out(_variant: builder.RunVariant) -> tuple[str, str, str]:
            engine.process.last_termination_reason = "timeout"
            return "INFRASTRUCTURE_ERROR", "process timed out", "host.log"

        engine._run_host = timed_out
        outcome = engine._run_variant(variant)

        self.assertEqual(outcome.status, "FAIL")
        self.assertEqual(
            outcome.detail, "host exceeded its explicit contractual time budget"
        )
        self.assertEqual(outcome.evidence, "host.log")

    def test_builder_timeout_table_equals_manifest_contract(self) -> None:
        self.assertEqual(builder.TIMEOUT_SECONDS, MANIFEST_TIMEOUT_SECONDS)

    def test_every_live_case_preserves_its_declared_timeout_class(self) -> None:
        catalog = builder.CanonicalCatalog.load()
        executable = tuple(case for case in catalog.cases if case.executable)

        self.assertTrue(executable)
        for case in executable:
            self.assertEqual(
                builder.ExecutionEngine._timeout_class(case),
                case.manifest["timeout_class"],
                case.case_id,
            )

    def test_unknown_timeout_class_fails_closed(self) -> None:
        case = builder.CaseRecord(
            case_id="TI-TIMEOUT-001",
            contract_id="CI-TIMEOUT-001",
            component="Host.Basic",
            wave=1,
            purpose="synthetic timeout guard",
            layer="unit",
            profile="PR",
            build_modes=("Debug",),
            optimizations=("NA",),
            executor="nsc_component_tests",
            disposition="Executable",
            objective="reject an unknown timeout class",
            expected_outcome="PASS",
            manifest={"timeout_class": "invented"},
        )

        with self.assertRaisesRegex(ValueError, "unsupported timeout class"):
            builder.ExecutionEngine._timeout_class(case)


if __name__ == "__main__":
    unittest.main()
