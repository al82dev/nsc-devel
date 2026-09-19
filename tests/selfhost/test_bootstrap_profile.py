from __future__ import annotations

import json
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from selfhost.bootstrap import verify_profile


PROFILE = ROOT / "selfhost" / "bootstrap" / "bootstrap-profile.json"
FIXTURES = ROOT / "selfhost" / "tests" / "profile"
TEST_TEMP_ROOT = ROOT / "_nscbuilder" / "temp" / "selfhost-profile-unit-tests"


class BootstrapProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.profile = verify_profile.load_profile(PROFILE)

    def setUp(self) -> None:
        TEST_TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=TEST_TEMP_ROOT)
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_contract_is_versioned_and_closed(self) -> None:
        self.assertEqual(self.profile["profile"], "NativeSharp.BootstrapProfile/2.0")
        self.assertEqual(self.profile["target"], "windows-x64-msvc")
        self.assertNotIn("stackalloc", self.profile["allowed_reserved_keywords"])
        self.assertIn("NativeSharp.Core.nsproj", self.profile["allowed_project_references"])
        known = set(self.profile["known_reserved_keywords"])
        allowed = set(self.profile["allowed_reserved_keywords"])
        forbidden = {rule["token"] for rule in self.profile["forbidden_tokens"]}
        self.assertEqual(known - allowed, known & forbidden)

    def test_positive_fixture_is_compliant(self) -> None:
        report = verify_profile.verify(PROFILE, [FIXTURES / "positive"])
        self.assertEqual(report["status"], "passed")
        self.assertGreaterEqual(report["files_checked"], 2)
        self.assertEqual(report["violations"], [])

    def test_each_negative_fixture_has_its_declared_diagnostic(self) -> None:
        manifest_path = FIXTURES / "negative" / "cases.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema"], "nativesharp-bootstrap-profile-fixtures")
        for case in manifest["cases"]:
            with self.subTest(case=case["file"]):
                report = verify_profile.verify(
                    PROFILE, [FIXTURES / "negative" / case["file"]]
                )
                actual = {item["code"] for item in report["violations"]}
                self.assertEqual(report["status"], "failed")
                self.assertTrue(set(case["expected"]).issubset(actual), (case, actual))

    def test_comments_and_literals_do_not_trigger_rules(self) -> None:
        source = self.root / "IgnoredText.ns"
        source.write_text(
            "// stackalloc event asm {\n"
            "namespace Safe { public class C { "
            "public static string Text() { return \"comptime vectorcall\"; } } }\n",
            encoding="utf-8",
        )
        self.assertEqual(verify_profile.scan_source(source, self.profile), [])

    def test_diagnostics_are_stable_and_source_ordered(self) -> None:
        source = self.root / "Order.ns"
        source.write_text(
            "namespace N { public class C { public static void F() { "
            "stackalloc byte[1]; event; } } }\n",
            encoding="utf-8",
        )
        first = verify_profile.scan_source(source, self.profile)
        second = verify_profile.scan_source(source, self.profile)
        self.assertEqual(first, second)
        self.assertEqual([item.code for item in first], ["NSBP2005", "NSBP2008"])

    def test_unapproved_project_reference_is_rejected(self) -> None:
        project = self.root / "Bad.nsproj"
        project.write_text(
            "<Project><ItemGroup><ProjectReference Include=\"Other.nsproj\" />"
            "</ItemGroup></Project>",
            encoding="utf-8",
        )
        violations = verify_profile.scan_project(project, self.profile)
        self.assertEqual([item.code for item in violations], ["NSBP3001"])

    def test_external_package_reference_is_rejected(self) -> None:
        project = self.root / "BadPackage.nsproj"
        project.write_text(
            "<Project><ItemGroup><PackageReference Include=\"Anything\" />"
            "</ItemGroup></Project>",
            encoding="utf-8",
        )
        violations = verify_profile.scan_project(project, self.profile)
        self.assertEqual([item.code for item in violations], ["NSBP3002"])

    def test_fully_qualified_forbidden_namespace_is_rejected(self) -> None:
        source = self.root / "Qualified.ns"
        source.write_text(
            "namespace N { public class C { "
            "private NativeSharp.Core.Threading.Thread worker; } }\n",
            encoding="utf-8",
        )
        violations = verify_profile.scan_source(source, self.profile)
        self.assertEqual([item.code for item in violations], ["NSBP2201"])

    def test_unknown_contract_version_fails_closed(self) -> None:
        profile = json.loads(PROFILE.read_text(encoding="utf-8"))
        profile["version"] = 2
        path = self.root / "future-profile.json"
        path.write_text(json.dumps(profile), encoding="utf-8")
        with self.assertRaises(verify_profile.ProfileError):
            verify_profile.load_profile(path)

    def test_cli_exit_codes_distinguish_violation_and_invalid_input(self) -> None:
        bad_source = FIXTURES / "negative" / "stackalloc.ns"
        self.assertEqual(
            verify_profile.main(
                ["--profile", str(PROFILE), "--source-root", str(bad_source), "--quiet"]
            ),
            verify_profile.EXIT_VIOLATION,
        )
        diagnostics = io.StringIO()
        with contextlib.redirect_stderr(diagnostics):
            result = verify_profile.main(
                [
                    "--profile",
                    str(PROFILE),
                    "--source-root",
                    str(self.root / "missing"),
                    "--quiet",
                ]
            )
        self.assertEqual(result, verify_profile.EXIT_INVALID_INPUT)
        self.assertIn("source root does not exist", diagnostics.getvalue())


if __name__ == "__main__":
    unittest.main()
