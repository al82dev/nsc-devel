from __future__ import annotations

import unittest
from pathlib import Path


MODULAR_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = MODULAR_ROOT.parent


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


if __name__ == "__main__":
    unittest.main()
