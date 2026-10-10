from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


def envelope(**overrides: object) -> str:
    payload: dict[str, object] = {
        "case_id": "TB-BAS-001",
        "component": "Host.Basic",
        "contract_id": "CB-BAS-001",
        "duration_us": 42,
        "status": "PASS",
        "failures": [],
    }
    payload.update(overrides)
    return json.dumps(payload) + "\n"


class Builder3HostEnvelopeTests(unittest.TestCase):
    def abnormal_case(self) -> builder.CaseRecord:
        return builder.CaseRecord(
            case_id="TL-LANG-099",
            contract_id="CL-LANG-008",
            component="Host.Language",
            wave=1,
            purpose="synthetic abnormal termination contract",
            layer="unit",
            profile="NIGHTLY",
            build_modes=("Debug",),
            optimizations=("NA",),
            executor="nsc_component_tests",
            disposition="KnownProductDefect",
            objective="characterize the current unsafe enum boundary",
            expected_outcome="KNOWN_FAILURE",
            manifest={
                "oracle": {
                    "abnormal_termination": {
                        "exit_code": "nonzero",
                        "output_contains": [
                            "Assertion failed: array subscript out of range"
                        ],
                    }
                },
                "tags": ["known-defect", "unsafe-boundary"],
            },
        )

    def test_exact_known_abnormal_termination_is_recognized(self) -> None:
        detail = builder.expected_abnormal_termination_detail(
            self.abnormal_case(),
            3,
            "array(534) : Assertion failed: array subscript out of range\n",
        )
        self.assertIn("known product defect", detail or "")
        self.assertIn("exit code 3", detail or "")

    def test_abnormal_termination_rejects_zero_or_different_output(self) -> None:
        for exit_code, output in (
            (0, "Assertion failed: array subscript out of range"),
            (3, "unrelated crash"),
        ):
            with self.subTest(exit_code=exit_code, output=output):
                with self.assertRaises(ValueError):
                    builder.expected_abnormal_termination_detail(
                        self.abnormal_case(), exit_code, output
                    )

    def test_ordinary_case_never_accepts_an_unstructured_crash(self) -> None:
        ordinary = builder.CaseRecord(
            **{
                **self.abnormal_case().__dict__,
                "disposition": "Executable",
                "expected_outcome": "PASS",
                "manifest": {"oracle": {}, "tags": []},
            }
        )
        self.assertIsNone(
            builder.expected_abnormal_termination_detail(
                ordinary, 3, "Assertion failed: array subscript out of range"
            )
        )

    def parse(self, captured: str, exit_code: int = 0) -> tuple[bool, str]:
        return builder.parse_host_envelope(
            captured=captured,
            exit_code=exit_code,
            case_id="TB-BAS-001",
            contract_id="CB-BAS-001",
            component="Host.Basic",
        )

    def test_pass_and_fail_envelopes_preserve_result_and_detail(self) -> None:
        self.assertEqual(self.parse(envelope()), (True, ""))
        self.assertEqual(
            self.parse(envelope(status="FAIL", failures=["broken"]), exit_code=1),
            (False, "broken"),
        )

    def test_complete_identity_must_match_requested_case(self) -> None:
        for field, value in (
            ("case_id", "OTHER"),
            ("contract_id", "OTHER"),
            ("component", "Other.Component"),
        ):
            with self.subTest(field=field):
                with self.assertRaisesRegex(ValueError, "identity mismatch"):
                    self.parse(envelope(**{field: value}))

    def test_unknown_status_and_invalid_failures_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "status"):
            self.parse(envelope(status="BROKEN"))
        with self.assertRaisesRegex(ValueError, "failures"):
            self.parse(envelope(failures="not-a-list"))
        with self.assertRaisesRegex(ValueError, "failures"):
            self.parse(envelope(failures=[1]))
        with self.assertRaisesRegex(ValueError, "failures"):
            self.parse(envelope(failures=[""]))

    def test_status_and_failure_evidence_must_be_coherent(self) -> None:
        with self.assertRaisesRegex(ValueError, "PASS result"):
            self.parse(envelope(failures=["contradiction"]))
        with self.assertRaisesRegex(ValueError, "requires failure evidence"):
            self.parse(envelope(status="FAIL"), exit_code=1)

    def test_exit_code_must_match_host_status(self) -> None:
        with self.assertRaisesRegex(ValueError, "exit code"):
            self.parse(envelope(), exit_code=1)
        with self.assertRaisesRegex(ValueError, "exit code"):
            self.parse(envelope(status="FAIL", failures=["broken"]), exit_code=0)

    def test_duration_and_json_cardinality_are_validated(self) -> None:
        with self.assertRaisesRegex(ValueError, "duration_us"):
            self.parse(envelope(duration_us=-1))
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.parse(envelope() + envelope())
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.parse("unexpected diagnostic\n" + envelope())
        with self.assertRaisesRegex(ValueError, "JSON object"):
            self.parse("[]\n")

    def test_duplicate_json_keys_are_rejected(self) -> None:
        duplicate = envelope().replace(
            '"status": "PASS"',
            '"status": "PASS", "status": "FAIL"',
        )
        with self.assertRaisesRegex(ValueError, "duplicate JSON object key: status"):
            self.parse(duplicate)


if __name__ == "__main__":
    unittest.main()
