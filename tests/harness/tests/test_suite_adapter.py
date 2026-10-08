from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parents[1]
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.suites import SuiteAdapter, SuiteCatalog  # noqa: E402
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
        finally:
            remove_tree_iterative(temporary)

    def test_blocks_owner_with_non_o0_execution(self) -> None:
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
            self.assertFalse(suite.runnable)
            self.assertIn("O1/O2/O3", suite.blocked_reason or "")
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

    def test_blocks_suite_that_delegates_to_legacy_ctest_tree(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        catalog = SuiteCatalog.load(
            repo / "docs/compiler-optimization/test-infrastructure/test-destination-matrix.json",
            repo,
        )
        suite = catalog.require("nsc.selfhost.advanced-toolchain")
        self.assertFalse(suite.runnable)
        self.assertIn("legacy CTest", suite.blocked_reason or "")


if __name__ == "__main__":
    unittest.main()
