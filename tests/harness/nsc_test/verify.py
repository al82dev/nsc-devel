"""Behavioral verification of compiler and produced-program results."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
import re

from .model import TestCase, TextExpectation
from .process import ProcessResult
from .workspace import CaseWorkspace


class FailureKind(str, Enum):
    COMPILER_EXIT = "compiler-exit"
    STDOUT = "stdout"
    STDERR = "stderr"
    MISSING_ARTIFACT = "missing-artifact"
    PRODUCT_NOT_RUN = "product-not-run"
    PRODUCT_EXIT = "product-exit"


@dataclass(frozen=True)
class VerificationFailure:
    kind: FailureKind
    message: str


@dataclass(frozen=True)
class TestResult:
    case_id: str
    component: str
    passed: bool
    process: ProcessResult
    failures: tuple[VerificationFailure, ...]
    workspace: Path


class ResultVerifier:
    def verify(
        self,
        case: TestCase,
        workspace: CaseWorkspace,
        process: ProcessResult,
        product: ProcessResult | None = None,
    ) -> TestResult:
        failures: list[VerificationFailure] = []
        if process.exit_code != case.expected.exit_code:
            failures.append(
                VerificationFailure(
                    FailureKind.COMPILER_EXIT,
                    f"compiler exit code {process.exit_code}; expected {case.expected.exit_code}",
                )
            )
        self._verify_text("stdout", case.expected.stdout, process.stdout, failures)
        self._verify_text("stderr", case.expected.stderr, process.stderr, failures)
        for artifact in case.expected.artifacts:
            candidate = (workspace.root / artifact).resolve()
            if not candidate.is_relative_to(workspace.root.resolve()):
                failures.append(
                    VerificationFailure(
                        FailureKind.MISSING_ARTIFACT,
                        f"artifact path escapes workspace: {artifact}",
                    )
                )
            elif not candidate.exists():
                failures.append(
                    VerificationFailure(
                        FailureKind.MISSING_ARTIFACT,
                        f"expected artifact does not exist: {artifact}",
                    )
                )
        if case.expected.run_exit_code is not None:
            if product is None:
                failures.append(
                    VerificationFailure(
                        FailureKind.PRODUCT_NOT_RUN,
                        "case requires a product execution result",
                    )
                )
            elif product.exit_code != case.expected.run_exit_code:
                failures.append(
                    VerificationFailure(
                        FailureKind.PRODUCT_EXIT,
                        f"product exit code {product.exit_code}; expected {case.expected.run_exit_code}",
                    )
                )
        return TestResult(
            case_id=case.identifier,
            component=case.component,
            passed=not failures,
            process=process,
            failures=tuple(failures),
            workspace=workspace.root,
        )

    @staticmethod
    def _verify_text(
        stream: str,
        expected: TextExpectation | None,
        actual: str,
        failures: list[VerificationFailure],
    ) -> None:
        if expected is None:
            return
        matched = False
        if expected.mode == "exact":
            matched = actual == expected.text
        elif expected.mode == "contains":
            matched = expected.text in actual
        elif expected.mode == "not-contains":
            matched = expected.text not in actual
        elif expected.mode == "regex":
            matched = re.search(expected.text, actual, flags=re.DOTALL) is not None
        if matched:
            return
        kind = FailureKind.STDOUT if stream == "stdout" else FailureKind.STDERR
        failures.append(
            VerificationFailure(
                kind,
                f"{stream} did not satisfy {expected.mode} expectation",
            )
        )
