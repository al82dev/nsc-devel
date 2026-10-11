from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.host import HostExecutor
from nsc_suite.manifest import CaseManifest
from nsc_suite.result import CaseStatus
from nsc_suite.workspace import WorkspaceManager


def case(
    *,
    executor: str = "nsc_component_tests",
    expected_outcome: str = "PASS",
    oracle: dict[str, object] | None = None,
    tags: tuple[str, ...] = (),
) -> CaseManifest:
    return CaseManifest(
        case_id="TB-BAS-001",
        contract_id="CB-BAS-001",
        component="Host.Basic",
        case_class="nominal",
        executor=executor,
        layer="unit",
        inputs={},
        oracle=oracle or {"status": "PASS"},
        dependencies=(),
        profile="PR",
        build_modes=("Debug", "Release"),
        optimizations=("NA",),
        isolation="in-process-reset",
        tags=tags,
        expected_outcome=expected_outcome,
    )


class HostExecutorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="nsc-suite-host-", dir=Path.cwd()
        )
        self.root = Path(self.temporary.name)
        self.next_run = 0

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def execute(self, payload: object, exit_code: int = 0):
        self.next_run += 1
        script = self.root / f"fake-host-{self.next_run}.py"
        script.write_text(
            "import json, sys\n"
            f"print({json.dumps(json.dumps(payload, sort_keys=True))})\n"
            f"raise SystemExit({exit_code})\n",
            encoding="utf-8",
            newline="\n",
        )
        workspace = WorkspaceManager(self.root).prepare(
            f"run-{self.next_run:03d}", "TB-BAS-001"
        )
        result = HostExecutor((sys.executable, str(script)))(case(), workspace)
        return result, workspace

    def execute_raw(self, payload: str, exit_code: int = 0):
        self.next_run += 1
        script = self.root / f"fake-host-{self.next_run}.py"
        script.write_text(
            f"print({payload!r})\nraise SystemExit({exit_code})\n",
            encoding="utf-8",
            newline="\n",
        )
        workspace = WorkspaceManager(self.root).prepare(
            f"run-{self.next_run:03d}", "TB-BAS-001"
        )
        result = HostExecutor((sys.executable, str(script)))(case(), workspace)
        return result, workspace

    def valid_payload(self, status: str, failures: list[str]) -> dict[str, object]:
        return {
            "case_id": "TB-BAS-001",
            "contract_id": "CB-BAS-001",
            "component": "Host.Basic",
            "duration_us": 42,
            "status": status,
            "failures": failures,
        }

    def test_accepts_coherent_pass_and_persists_logs(self) -> None:
        result, workspace = self.execute(self.valid_payload("PASS", []))
        self.assertIs(result.status, CaseStatus.PASS)
        self.assertIn("TB-BAS-001", workspace.logs.joinpath("stdout.log").read_text())
        self.assertEqual(workspace.logs.joinpath("stderr.log").read_text(), "")

    def test_routes_isolated_executors_only_through_their_bound_adapters(self) -> None:
        for executor in (
            "nsc_component_tests_destructive",
            "nsc_component_tests_source_extended",
        ):
            with self.subTest(executor=executor):
                self.next_run += 1
                script = self.root / f"fake-host-{self.next_run}.py"
                script.write_text(
                    "import json\n"
                    f"print({json.dumps(json.dumps(self.valid_payload('PASS', []), sort_keys=True))})\n",
                    encoding="utf-8",
                    newline="\n",
                )
                workspace = WorkspaceManager(self.root).prepare(
                    f"run-{self.next_run:03d}", "TB-BAS-001"
                )
                result = HostExecutor((sys.executable, str(script)), executor)(
                    case(executor=executor), workspace
                )

                self.assertIs(result.status, CaseStatus.PASS)

    def test_preserves_a_coherent_product_failure(self) -> None:
        result, _ = self.execute(
            self.valid_payload("FAIL", ["synthetic contract failure"]), exit_code=1
        )
        self.assertIs(result.status, CaseStatus.FAIL)
        self.assertEqual(result.failures, ("synthetic contract failure",))

    def test_classifies_an_expected_failure_without_converting_it_to_pass(self) -> None:
        self.next_run += 1
        script = self.root / f"fake-host-{self.next_run}.py"
        payload = self.valid_payload("FAIL", ["known product defect"])
        script.write_text(
            "import json\n"
            f"print({json.dumps(json.dumps(payload, sort_keys=True))})\n"
            "raise SystemExit(1)\n",
            encoding="utf-8",
            newline="\n",
        )
        workspace = WorkspaceManager(self.root).prepare(
            f"run-{self.next_run:03d}", "TB-BAS-001"
        )
        result = HostExecutor((sys.executable, str(script)))(
            case(expected_outcome="KNOWN_FAILURE"), workspace
        )
        self.assertIs(result.status, CaseStatus.KNOWN_FAILURE)
        self.assertEqual(result.failures, ("known product defect",))

    def test_flags_an_unexpected_pass_of_a_known_defect(self) -> None:
        self.next_run += 1
        script = self.root / f"fake-host-{self.next_run}.py"
        payload = self.valid_payload("PASS", [])
        script.write_text(
            "import json\n"
            f"print({json.dumps(json.dumps(payload, sort_keys=True))})\n",
            encoding="utf-8",
            newline="\n",
        )
        workspace = WorkspaceManager(self.root).prepare(
            f"run-{self.next_run:03d}", "TB-BAS-001"
        )
        result = HostExecutor((sys.executable, str(script)))(
            case(expected_outcome="KNOWN_FAILURE"), workspace
        )
        self.assertIs(result.status, CaseStatus.UNEXPECTED_PASS)

    def test_classifies_only_an_exact_isolated_known_defect_terminal(self) -> None:
        workspace = WorkspaceManager(self.root).prepare(
            "run-exact-terminal", "TB-BAS-001"
        )
        abnormal_case = case(
            expected_outcome="KNOWN_FAILURE",
            oracle={
                "abnormal_termination": {
                    "kind": "windows-status",
                    "exit_code": "0xC00000FD",
                    "output_contains": [],
                }
            },
            tags=("known-defect", "unsafe-boundary"),
        )
        completed = subprocess.CompletedProcess(
            args=("fake-host",), returncode=0xC00000FD, stdout="", stderr=""
        )
        with patch("nsc_suite.host.subprocess.run", return_value=completed):
            result = HostExecutor(("fake-host",))(abnormal_case, workspace)
        self.assertIs(result.status, CaseStatus.KNOWN_FAILURE)
        self.assertIn("windows-status/0xC00000FD", result.failures[0])

        for terminal in (9, 0xC000001D):
            with self.subTest(terminal=terminal):
                mismatch = subprocess.CompletedProcess(
                    args=("fake-host",), returncode=terminal, stdout="", stderr=""
                )
                with patch("nsc_suite.host.subprocess.run", return_value=mismatch):
                    result = HostExecutor(("fake-host",))(abnormal_case, workspace)
                self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)

    def test_never_converts_an_undeclared_kpd_crash(self) -> None:
        workspace = WorkspaceManager(self.root).prepare("run-undeclared", "TB-BAS-001")
        completed = subprocess.CompletedProcess(
            args=("fake-host",), returncode=0xC00000FD, stdout="", stderr=""
        )
        with patch("nsc_suite.host.subprocess.run", return_value=completed):
            result = HostExecutor(("fake-host",))(
                case(expected_outcome="KNOWN_FAILURE"), workspace
            )
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)

    def test_abnormal_kpd_does_not_accept_a_structured_failure(self) -> None:
        self.next_run += 1
        script = self.root / f"fake-host-{self.next_run}.py"
        payload = self.valid_payload("FAIL", ["different product failure"])
        script.write_text(
            "import json\n"
            f"print({json.dumps(json.dumps(payload, sort_keys=True))})\n"
            "raise SystemExit(1)\n",
            encoding="utf-8",
            newline="\n",
        )
        workspace = WorkspaceManager(self.root).prepare(
            f"run-{self.next_run:03d}", "TB-BAS-001"
        )
        result = HostExecutor((sys.executable, str(script)))(
            case(
                expected_outcome="KNOWN_FAILURE",
                oracle={
                    "abnormal_termination": {
                        "kind": "windows-status",
                        "exit_code": "0xC00000FD",
                        "output_contains": [],
                    }
                },
                tags=("known-defect", "unsafe-boundary"),
            ),
            workspace,
        )
        self.assertIs(result.status, CaseStatus.FAIL)

    def test_rejects_wrong_contract_identity(self) -> None:
        payload = self.valid_payload("PASS", [])
        payload["contract_id"] = "CB-SRC-001"
        result, _ = self.execute(payload)
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("contract", result.failures[0])

    def test_rejects_exit_status_contradiction(self) -> None:
        result, _ = self.execute(self.valid_payload("PASS", []), exit_code=1)
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("contradicts", result.failures[0])

    def test_rejects_empty_failure_evidence_without_escaping(self) -> None:
        result, _ = self.execute(self.valid_payload("FAIL", [""]), exit_code=1)
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("schema", result.failures[0])

    def test_rejects_missing_coerced_or_negative_host_duration(self) -> None:
        invalid_values = (None, True, -1, 1.5, "42")
        for invalid_value in invalid_values:
            with self.subTest(duration_us=invalid_value):
                payload = self.valid_payload("PASS", [])
                if invalid_value is None:
                    del payload["duration_us"]
                else:
                    payload["duration_us"] = invalid_value
                result, _ = self.execute(payload)
                self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
                self.assertIn("schema", result.failures[0])

    def test_rejects_duplicate_json_keys_without_last_wins(self) -> None:
        result, _ = self.execute_raw(
            '{"case_id":"TB-BAS-001","contract_id":"CB-BAS-001",'
            '"component":"Host.Basic","status":"PASS","status":"FAIL",'
            '"failures":[]}'
        )
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("duplicate JSON object key: status", result.failures[0])

    def test_rejects_an_executor_owned_by_another_adapter(self) -> None:
        workspace = WorkspaceManager(self.root).prepare("run-other", "TB-BAS-001")
        result = HostExecutor((sys.executable, "unused"))(
            case(executor="nsc_suite_controller+nsc"), workspace
        )
        self.assertIs(result.status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("cannot run", result.failures[0])

if __name__ == "__main__":
    unittest.main()
