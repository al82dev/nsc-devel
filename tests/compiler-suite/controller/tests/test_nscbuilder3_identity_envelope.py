from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


def envelope(
    *,
    status: str = "PASS",
    failures: list[str] | None = None,
    blocked_by: list[str] | None = None,
    **overrides: object,
) -> str:
    counts = {name: 0 for name in builder.RESULT_STATUSES}
    counts[status] = 1
    payload: dict[str, object] = {
        "schema": 1,
        "run": {
            "run_id": "TB-BAS-020-Debug-NA",
            "revision": "0123456789abcdef",
            "build_mode": "Debug",
            "optimization": "NA",
            "profile": "explicit",
            "scope": "focused",
            "selected": 1,
            "tools": [],
        },
        "counts": counts,
        "results": [
            {
                "case_id": "TB-BAS-020",
                "status": status,
                "duration_seconds": 0.25,
                "failures": failures or [],
                "blocked_by": blocked_by or [],
            }
        ],
    }
    payload.update(overrides)
    return json.dumps(payload)


class Builder3IdentityEnvelopeTests(unittest.TestCase):
    def parse(
        self,
        content: str,
        exit_code: int = 0,
        expected_tools: tuple[tuple[str, str, str], ...] = (),
    ) -> tuple[str, str]:
        return builder.parse_identity_envelope(
            content=content,
            exit_code=exit_code,
            run_id="TB-BAS-020-Debug-NA",
            revision="0123456789abcdef",
            build_mode="Debug",
            optimization="NA",
            case_id="TB-BAS-020",
            expected_tools=expected_tools,
        )

    def test_pass_and_known_failure_preserve_terminal_evidence(self) -> None:
        self.assertEqual(self.parse(envelope()), ("PASS", ""))
        self.assertEqual(
            self.parse(
                envelope(status="KNOWN_FAILURE", failures=["known defect"]),
                exit_code=0,
            ),
            ("KNOWN_FAILURE", "known defect"),
        )

    def test_run_and_case_identity_must_match_request(self) -> None:
        payload = json.loads(envelope())
        for field, value in (
            ("run_id", "another-run"),
            ("revision", "different"),
            ("build_mode", "Release"),
            ("optimization", "O0"),
        ):
            with self.subTest(field=field):
                changed = json.loads(json.dumps(payload))
                changed["run"][field] = value
                with self.assertRaisesRegex(ValueError, "run identity mismatch"):
                    self.parse(json.dumps(changed))
        payload["results"][0]["case_id"] = "TB-BAS-021"
        with self.assertRaisesRegex(ValueError, "case identity mismatch"):
            self.parse(json.dumps(payload))

    def test_schema_cardinality_and_field_types_are_closed(self) -> None:
        with self.assertRaisesRegex(ValueError, "JSON object"):
            self.parse("[]")
        with self.assertRaisesRegex(ValueError, "schema"):
            self.parse(envelope(schema=2))
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.parse(envelope(results=[]))
        with self.assertRaisesRegex(ValueError, "duration_seconds"):
            payload = json.loads(envelope())
            payload["results"][0]["duration_seconds"] = -1
            self.parse(json.dumps(payload))
        for duration in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(duration=duration):
                payload = json.loads(envelope())
                payload["results"][0]["duration_seconds"] = duration
                with self.assertRaisesRegex(ValueError, "duration_seconds"):
                    self.parse(json.dumps(payload))
        with self.assertRaisesRegex(ValueError, "failures"):
            payload = json.loads(envelope())
            payload["results"][0]["failures"] = "broken"
            self.parse(json.dumps(payload))

    def test_status_structure_and_counts_must_be_coherent(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown status"):
            self.parse(envelope(status="BROKEN"))
        with self.assertRaisesRegex(ValueError, "requires failure evidence"):
            self.parse(envelope(status="FAIL"), exit_code=1)
        with self.assertRaisesRegex(ValueError, "PASS cannot contain"):
            self.parse(envelope(failures=["contradiction"]))
        with self.assertRaisesRegex(ValueError, "requires blocked_by"):
            self.parse(envelope(status="BLOCKED"), exit_code=1)
        with self.assertRaisesRegex(ValueError, "only BLOCKED"):
            self.parse(
                envelope(
                    status="FAIL",
                    failures=["broken"],
                    blocked_by=["producer"],
                ),
                exit_code=1,
            )
        payload = json.loads(envelope())
        payload["counts"]["PASS"] = 0
        payload["counts"]["FAIL"] = 1
        with self.assertRaisesRegex(ValueError, "counts"):
            self.parse(json.dumps(payload))

    def test_process_exit_must_match_reported_status(self) -> None:
        with self.assertRaisesRegex(ValueError, "exit code"):
            self.parse(envelope(), exit_code=1)
        with self.assertRaisesRegex(ValueError, "exit code"):
            self.parse(envelope(status="FAIL", failures=["broken"]), exit_code=0)

    def test_tool_receipts_must_match_selected_files_exactly(self) -> None:
        expected = (("nsc", r"A:\\compiler\\nsc.exe", "a" * 64),)
        payload = json.loads(envelope())
        payload["run"]["tools"] = [
            {"name": name, "path": path, "sha256": sha256}
            for name, path, sha256 in expected
        ]
        self.assertEqual(
            self.parse(json.dumps(payload), expected_tools=expected),
            ("PASS", ""),
        )
        for tools in (
            [],
            [{"name": "nsc", "path": expected[0][1], "sha256": "b" * 64}],
            [payload["run"]["tools"][0], payload["run"]["tools"][0]],
            [{"name": "nsc", "path": expected[0][1], "sha256": "a" * 64, "extra": 1}],
        ):
            with self.subTest(tools=tools):
                changed = json.loads(json.dumps(payload))
                changed["run"]["tools"] = tools
                with self.assertRaisesRegex(ValueError, "tool"):
                    self.parse(json.dumps(changed), expected_tools=expected)


if __name__ == "__main__":
    unittest.main()
