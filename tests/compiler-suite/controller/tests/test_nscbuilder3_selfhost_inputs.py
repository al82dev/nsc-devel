from __future__ import annotations

import sys
import unittest
from collections import Counter
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


VALID_INPUTS = {
    "fixture_project": "selfhost/modules/SelfHostBasic.nsproj",
    "case_argument": "TSH-00001",
}


class Builder3SelfHostInputTests(unittest.TestCase):
    def parse(self, value: object) -> tuple[str, str, bool]:
        return builder.parse_selfhost_inputs(value, builder.SUITE_ROOT)

    def test_live_inputs_are_closed_and_producer_stems_do_not_collide(self) -> None:
        cases = tuple(
            case
            for case in builder.CanonicalCatalog.load().cases
            if case.executable and case.component.startswith("SelfHost.")
        )
        projects: set[str] = set()
        for case in cases:
            with self.subTest(case_id=case.case_id):
                project, argument, sandbox = self.parse(
                    (case.manifest or {}).get("inputs")
                )
                projects.add(project)
                self.assertTrue(argument)
                self.assertIs(type(sandbox), bool)
        stems = Counter(Path(project).stem for project in projects)
        self.assertFalse([name for name, count in stems.items() if count != 1])

    def test_fixture_must_be_a_local_existing_nsproj(self) -> None:
        for fixture in (
            1,
            "",
            "A:/nsc-devel/CMakeLists.txt",
            "../CMakeLists.txt",
            "selfhost/modules/missing.nsproj",
            "selfhost/modules/SelfHostBasic.ns",
        ):
            with self.subTest(fixture=fixture):
                value = dict(VALID_INPUTS, fixture_project=fixture)
                with self.assertRaisesRegex(ValueError, "fixture_project"):
                    self.parse(value)

    def test_case_argument_must_be_nonempty_text(self) -> None:
        for argument in (None, "", 1, ["TSH-00001"]):
            with self.subTest(argument=argument):
                value = dict(VALID_INPUTS, case_argument=argument)
                with self.assertRaisesRegex(ValueError, "case_argument"):
                    self.parse(value)

    def test_sandbox_is_absent_or_the_declared_literal(self) -> None:
        self.assertEqual(self.parse(VALID_INPUTS)[2], False)
        self.assertEqual(
            self.parse(dict(VALID_INPUTS, sandbox_argument="fresh-case-directory"))[2],
            True,
        )
        for sandbox in (True, "yes", "", 1):
            with self.subTest(sandbox=sandbox):
                with self.assertRaisesRegex(ValueError, "sandbox_argument"):
                    self.parse(dict(VALID_INPUTS, sandbox_argument=sandbox))

    def test_unknown_fields_and_non_objects_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "object"):
            self.parse([])
        with self.assertRaisesRegex(ValueError, "unknown fields"):
            self.parse(dict(VALID_INPUTS, fallback=True))


if __name__ == "__main__":
    unittest.main()
