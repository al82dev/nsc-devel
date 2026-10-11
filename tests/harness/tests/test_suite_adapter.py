from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parents[1]
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.suites import PROFILE_MATRIX_GATE, SuiteAdapter, SuiteCatalog, SuiteError  # noqa: E402
from nsc_test.workspace import remove_tree_iterative  # noqa: E402


class SuiteAdapterTests(unittest.TestCase):
    def test_preserves_command_and_rebases_selected_products(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        temporary = Path(tempfile.mkdtemp(dir=repo, prefix="suite-adapter-"))
        try:
            owner = temporary / "owner.cmake"
            owner.write_text("message(STATUS \"O0 only\")\n", encoding="utf-8")
            matrix = temporary / "matrix.json"
            matrix.write_text(
                json.dumps(
                    {
                        "tests": [
                            {
                                "ctest_id": "nsc.test.specialized",
                                "primary_component": "selfhost",
                                "destination": "KEEP_SPECIALIZED",
                                "owner_path": owner.relative_to(repo).as_posix(),
                                "command": [
                                    "cmake",
                                    "-DNSC_EXECUTABLE=A:/native-sharp-3.0-baseline/bin/nsc.exe",
                                    "-DNSLINK_EXECUTABLE=A:/nslink-devel/bin/release/nslink.exe",
                                    "-DSOURCE_ROOT=A:/nsc-devel",
                                    "-DOUTPUT_DIR=A:/nsc-devel/build-test-nslink/selfhost-test",
                                    "-P",
                                    owner.as_posix(),
                                ],
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            nsc = temporary / "nsc.exe"
            nslink = temporary / "nslink.exe"
            nsc.write_bytes(b"nsc")
            nslink.write_bytes(b"nslink")
            build = temporary / "build"
            build.mkdir()
            prefix = temporary / "prefix"
            (prefix / "bin").mkdir(parents=True)
            (prefix / "lib").mkdir()
            evidence = temporary / "evidence"
            suite = SuiteCatalog.load(matrix, repo).require("nsc.test.specialized")
            request = SuiteAdapter(nsc, nslink, repo, build, prefix, evidence).build(suite)
            command = "\n".join(request.command)
            self.assertIn(nsc.resolve().as_posix(), command)
            self.assertIn(nslink.resolve().as_posix(), command)
            self.assertIn(repo.resolve().as_posix(), command)
            self.assertIn((evidence / "selfhost-test").resolve().as_posix(), command)
            self.assertEqual(request.environment["NSC_TEST_SUITE_ID"], suite.identifier)
            self.assertEqual(Path(request.environment["NS_LIB_ROOT"]), prefix / "lib")
            self.assertEqual(request.environment["NSC_SELFHOST_PROFILES"], "O0")
            self.assertEqual(
                request.working_directory,
                (evidence / suite.identifier / "cwd").resolve(),
            )
            self.assertTrue(request.working_directory.is_dir())
        finally:
            remove_tree_iterative(temporary)

    def test_non_o0_owner_is_runnable_but_requires_explicit_gate(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        temporary = Path(tempfile.mkdtemp(dir=repo, prefix="suite-scope-"))
        try:
            owner = temporary / "owner.cmake"
            owner.write_text("execute_process(COMMAND nsc -O2 Main.ns)\n", encoding="utf-8")
            matrix = temporary / "matrix.json"
            matrix.write_text(
                json.dumps(
                    {
                        "tests": [
                            {
                                "ctest_id": "nsc.test.non-o0",
                                "primary_component": "selfhost",
                                "destination": "KEEP_SPECIALIZED",
                                "owner_path": owner.relative_to(repo).as_posix(),
                                "command": ["cmake", "-P", owner.as_posix()],
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            suite = SuiteCatalog.load(matrix, repo).require("nsc.test.non-o0")
            self.assertTrue(suite.runnable)
            self.assertEqual(suite.execution_gates, (PROFILE_MATRIX_GATE,))
            nsc = temporary / "nsc.exe"
            nsc.write_bytes(b"nsc")
            build = temporary / "build"
            build.mkdir()
            prefix = temporary / "prefix"
            (prefix / "bin").mkdir(parents=True)
            (prefix / "lib").mkdir()
            adapter = SuiteAdapter(nsc, None, repo, build, prefix, temporary / "evidence")
            with self.assertRaisesRegex(SuiteError, "profile-matrix"):
                adapter.build(suite)
            request = adapter.build(suite, frozenset({PROFILE_MATRIX_GATE}))
            self.assertEqual(request.command[-2:], ("-P", owner.as_posix()))
        finally:
            remove_tree_iterative(temporary)

    def test_replaces_positional_compiler_and_work_root_options(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        temporary = Path(tempfile.mkdtemp(dir=repo, prefix="suite-options-"))
        try:
            owner = temporary / "owner.py"
            owner.write_text("raise SystemExit(0)\n", encoding="utf-8")
            matrix = temporary / "matrix.json"
            matrix.write_text(
                json.dumps(
                    {
                        "tests": [
                            {
                                "ctest_id": "nsc.test.command-options",
                                "primary_component": "infrastructure",
                                "destination": "KEEP_SPECIALIZED",
                                "owner_path": owner.relative_to(repo).as_posix(),
                                "command": [
                                    sys.executable,
                                    owner.as_posix(),
                                    "--compiler",
                                    "A:/native-sharp-3.0-baseline/bin/nsc.exe",
                                    "--work-root",
                                    "A:/nsc-devel/build-test-nslink/old-work",
                                ],
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            nsc = temporary / "nsc.exe"
            nsc.write_bytes(b"nsc")
            build = temporary / "build"
            build.mkdir()
            prefix = temporary / "prefix"
            (prefix / "bin").mkdir(parents=True)
            (prefix / "lib").mkdir()
            evidence = temporary / "evidence"
            suite = SuiteCatalog.load(matrix, repo).require("nsc.test.command-options")
            request = SuiteAdapter(nsc, None, repo, build, prefix, evidence).build(suite)
            compiler_index = request.command.index("--compiler") + 1
            work_index = request.command.index("--work-root") + 1
            self.assertEqual(request.command[compiler_index], nsc.resolve().as_posix())
            self.assertEqual(
                request.command[work_index],
                (evidence / suite.identifier / "work").resolve().as_posix(),
            )
            self.assertNotIn("native-sharp-3.0-baseline", " ".join(request.command))
            self.assertNotIn("build-test-nslink", " ".join(request.command))
        finally:
            remove_tree_iterative(temporary)

    def test_orders_selfhost_consumers_after_generation(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        catalog = SuiteCatalog.load(
            repo / "docs/compiler-optimization/test-infrastructure/test-destination-matrix.json",
            repo,
        )
        identifiers = [suite.identifier for suite in catalog.runnable()]
        generation = identifiers.index("nsc.selfhost.self-hosted-generation")
        functional = identifiers.index("nsc.selfhost.functional-equivalence")
        repeated = identifiers.index("nsc.selfhost.repeated-fixed-point")
        self.assertLess(generation, functional)
        self.assertLess(functional, repeated)

    def test_legacy_ctest_suite_keeps_a_gated_executable_route(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        catalog = SuiteCatalog.load(
            repo / "docs/compiler-optimization/test-infrastructure/test-destination-matrix.json",
            repo,
        )
        suite = catalog.require("nsc.selfhost.advanced-toolchain")
        self.assertTrue(suite.runnable)
        self.assertIn("legacy-ctest", suite.execution_gates)


if __name__ == "__main__":
    unittest.main()
