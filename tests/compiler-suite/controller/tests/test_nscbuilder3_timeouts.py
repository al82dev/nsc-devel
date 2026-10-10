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
