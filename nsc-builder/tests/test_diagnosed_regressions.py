from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


MODULAR_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = MODULAR_ROOT.parent
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder.constants import (
    IDENTITY_COUNT_STATUSES,
    IDENTITY_RESULT_STATUSES,
    RESULT_STATUSES,
)
from nsc_builder.execution import parse_identity_envelope


class DiagnosedRegressionTests(unittest.TestCase):
    def test_parser_lookahead_uses_the_declaring_typesyntax_namespace(self) -> None:
        source = (
            REPOSITORY_ROOT
            / "tests"
            / "compiler-suite"
            / "host"
            / "modules"
            / "ParserLookaheadModule.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn(
            "nativesharp::ast::TypeSyntax* const type = fixture.parser().parseType();",
            source,
        )
        self.assertNotIn(
            "\n    TypeSyntax* const type = fixture.parser().parseType();",
            source,
        )

    def test_parser_clone_compares_observable_source_range_fields(self) -> None:
        source = (
            REPOSITORY_ROOT
            / "tests"
            / "compiler-suite"
            / "host"
            / "modules"
            / "ParserConstructionCloneFinalModule.cpp"
        ).read_text(encoding="utf-8")

        self.assertNotIn("clone->range() == source->range()", source)
        self.assertIn(
            "clone->range().begin() == source->range().begin()",
            source,
        )
        self.assertIn(
            "clone->range().end() == source->range().end()",
            source,
        )
        self.assertNotIn("AstNode* firstChild(", source)

    def test_source_mutation_plans_initialize_terminal_observation_fields(self) -> None:
        source = (
            REPOSITORY_ROOT
            / "tests"
            / "compiler-suite"
            / "host"
            / "modules"
            / "SourceFileReadModule.cpp"
        ).read_text(encoding="utf-8")

        self.assertEqual(source.count(".triggered = false"), 2)
        self.assertEqual(source.count(".error = {}"), 2)
        guard = source.index("#if !defined(NSC_SOURCE_EXTENDED_TESTS_ENABLED)")
        limit = source.index("constexpr std::uintmax_t kMaximumSourceBytes")
        self.assertGreater(limit, guard)

    def test_identity_protocol_excludes_builder_only_not_run_state(self) -> None:
        self.assertIn("NOT_RUN", RESULT_STATUSES)
        self.assertNotIn("NOT_RUN", IDENTITY_RESULT_STATUSES)
        self.assertNotIn("NOT_RUN", IDENTITY_COUNT_STATUSES)

        counts = {name: 0 for name in IDENTITY_COUNT_STATUSES}
        counts["PASS"] = 1
        payload = {
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
                    "status": "PASS",
                    "duration_seconds": 0.25,
                    "failures": [],
                    "blocked_by": [],
                }
            ],
        }
        arguments = {
            "exit_code": 0,
            "run_id": "TB-BAS-020-Debug-NA",
            "revision": "0123456789abcdef",
            "build_mode": "Debug",
            "optimization": "NA",
            "case_id": "TB-BAS-020",
            "expected_tools": (),
        }

        self.assertEqual(
            parse_identity_envelope(content=json.dumps(payload), **arguments),
            ("PASS", ""),
        )

        payload["counts"]["NOT_RUN"] = 0
        with self.assertRaisesRegex(ValueError, "counts"):
            parse_identity_envelope(content=json.dumps(payload), **arguments)

        payload["counts"].pop("NOT_RUN")
        payload["counts"]["PASS"] = 0
        payload["counts"]["NOT_RUN"] = 1
        payload["results"][0]["status"] = "NOT_RUN"
        with self.assertRaisesRegex(ValueError, "unknown status"):
            parse_identity_envelope(content=json.dumps(payload), **arguments)


if __name__ == "__main__":
    unittest.main()
