from __future__ import annotations

import ast
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.dispatch import ExecutorRouter
from nsc_suite.manifest import CaseManifest
from nsc_suite.process import CompilerIdentityExecutor
from nsc_suite.result import CaseStatus
from nsc_suite.subprocess_policy import HIDDEN_PROCESS_FLAGS
from nsc_suite.workspace import WorkspaceManager


ORACLES = {
    "compiler-version-constants": {
        "compiler_release": "3.0.0",
        "language": "3.0.0",
        "previous_release": "2.0.0",
        "stage": "70",
    },
    "generated-identity-exports": {
        "build_id_is_lower_hex_16": True,
        "build_matches_generated": True,
        "version_matches_generated": True,
        "version_uses_release": True,
    },
    "distinct-build-identities": {
        "build_ids_distinct": True,
        "versions_distinct": True,
    },
    "semantic-version": {
        "release": "3.0.0",
        "release_matches_semver_core": True,
        "semver_valid": True,
    },
}


def case(operation: str, case_id: str = "TB-BAS-020") -> CaseManifest:
    return CaseManifest(
        case_id=case_id,
        contract_id="CB-BAS-006",
        component="Host.Basic",
        case_class="nominal",
        executor="nsc_suite_controller+nsc",
        layer="build-contract",
        inputs={"operation": operation},
        oracle=ORACLES[operation],
        dependencies=(),
        profile="NIGHTLY",
        build_modes=("Debug", "Release"),
        optimizations=("NA",),
        isolation="temporary-directory",
        tags=("build-identity",),
    )


class CompilerIdentityExecutorTests(unittest.TestCase):
    def test_noninteractive_suite_launches_share_the_hidden_window_policy(self) -> None:
        if os.name == "nt":
            self.assertEqual(
                HIDDEN_PROCESS_FLAGS & subprocess.CREATE_NO_WINDOW,
                subprocess.CREATE_NO_WINDOW,
            )
        for filename in ("host.py", "process.py"):
            path = CONTROLLER_ROOT / "nsc_suite" / filename
            tree = ast.parse(path.read_text(encoding="utf-8"))
            calls = [
                node
                for node in ast.walk(tree)
                if isinstance(node, ast.Call)
                and isinstance(node.func, ast.Attribute)
                and isinstance(node.func.value, ast.Name)
                and node.func.value.id == "subprocess"
                and node.func.attr == "run"
            ]
            self.assertEqual(len(calls), 1, filename)
            self.assertIn(
                "creationflags",
                {keyword.arg for keyword in calls[0].keywords},
                filename,
            )

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="nsc-suite-process-", dir=Path.cwd()
        )
        self.root = Path(self.temporary.name)
        self.primary = self.write_compiler(
            "primary.py", "0123456789abcdef", "20261009T010203Z"
        )
        self.comparison = self.write_compiler(
            "comparison.py", "fedcba9876543210", "20261009T020304Z"
        )
        self.header = self.root / "BuildId.h"
        self.header.write_text(
            'inline constexpr auto kGeneratedCompilerBuildId = "0123456789abcdef";\n'
            'inline constexpr auto kGeneratedCompilerVersion = '
            '"3.0.0+build.20261009T010203Z.0123456789abcdef";\n',
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_compiler(self, name: str, build_id: str, timestamp: str) -> Path:
        script = self.root / name
        compiler_version = f"3.0.0+build.{timestamp}.{build_id}"
        script.write_text(
            "import sys\n"
            f"build = {build_id!r}\n"
            f"version = {compiler_version!r}\n"
            "if sys.argv[1] == '--print-compiler-identity':\n"
            "    print('NSCOMPILER 1')\n"
            "    print('compiler_version=' + version)\n"
            "    print('compiler_release=3.0.0')\n"
            "    print('language=3.0.0')\n"
            "    print('stage=70')\n"
            "    print('build=' + build)\n"
            "elif sys.argv[1] == '--print-compatibility-matrix':\n"
            "    print('NSCOMPAT-MATRIX 1')\n"
            "    print('compiler.N=3.0.0')\n"
            "    print('compiler.N-1=2.0.0')\n"
            "    print('row=compiler|consumer:N|producer:N-1|accept')\n"
            "    print('row=compiler|consumer:N|producer:N|accept')\n"
            "else:\n"
            "    raise SystemExit(2)\n",
            encoding="utf-8",
            newline="\n",
        )
        return script

    def executor(self, *, comparison: bool = True, header: bool = True):
        return CompilerIdentityExecutor(
            (sys.executable, str(self.primary)),
            (sys.executable, str(self.comparison)) if comparison else None,
            self.header if header else None,
        )

    def execute(self, operation: str, index: int, executor=None):
        test_case = case(operation, f"TB-BAS-{index:03d}")
        workspace = WorkspaceManager(self.root).prepare(
            f"run-{index:03d}", test_case.case_id
        )
        return (executor or self.executor())(test_case, workspace), workspace

    def test_all_four_identity_operations_pass(self) -> None:
        operations = (
            "compiler-version-constants",
            "generated-identity-exports",
            "distinct-build-identities",
            "semantic-version",
        )
        for index, operation in enumerate(operations, start=20):
            result, workspace = self.execute(operation, index)
            self.assertIs(result.status, CaseStatus.PASS)
            self.assertTrue(any(workspace.logs.glob("*.command.json")))
            self.assertTrue(any(workspace.logs.glob("*.stdout.log")))

    def test_missing_comparison_tool_is_infrastructure_error(self) -> None:
        result, _ = self.execute(
            "distinct-build-identities", 30, self.executor(comparison=False)
        )
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("comparison NSC", result.failures[0])

    def test_product_protocol_mismatch_is_product_failure(self) -> None:
        self.primary.write_text("print('wrong protocol')\n", encoding="utf-8")
        result, _ = self.execute("semantic-version", 31)
        self.assertIs(result.status, CaseStatus.FAIL)
        self.assertIn("protocol header", result.failures[0])

    def test_distinct_identities_reject_missing_primary_fields(self) -> None:
        self.primary.write_text(
            "print('NSCOMPILER 1')\n",
            encoding="utf-8",
            newline="\n",
        )
        result, _ = self.execute("distinct-build-identities", 33)
        self.assertIs(result.status, CaseStatus.FAIL)
        self.assertIn("compiler identity lacks non-empty fields", result.failures[0])
        self.assertIn("compiler_version", result.failures[0])
        self.assertIn("build", result.failures[0])

    def test_router_never_falls_back_to_another_executor(self) -> None:
        test_case = case("semantic-version", "TB-BAS-032")
        workspace = WorkspaceManager(self.root).prepare("run-032", test_case.case_id)
        result = ExecutorRouter({})(test_case, workspace)
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("no executor configured", result.failures[0])


if __name__ == "__main__":
    unittest.main()
