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
            self.parse("[]\n")


if __name__ == "__main__":
    unittest.main()
