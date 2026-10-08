"""Shared implementation for the consolidated NSC test harness."""

from .catalog import CatalogError, TestCatalog
from .command import NscCommandBuilder
from .model import CaseKind, ExpectedOutcome, ManifestError, TestCase, TextExpectation
from .process import ProcessExecutor, ProcessRequest, ProcessResult
from .report import ReportWriter, TestRunRecord
from .workspace import CaseWorkspace
from .verify import FailureKind, ResultVerifier, TestResult, VerificationFailure

__all__ = [
    "CaseKind",
    "CatalogError",
    "ExpectedOutcome",
    "FailureKind",
    "ManifestError",
    "NscCommandBuilder",
    "ProcessExecutor",
    "ProcessRequest",
    "ProcessResult",
    "ReportWriter",
    "TestRunRecord",
    "ResultVerifier",
    "TestCase",
    "TestCatalog",
    "TestResult",
    "TextExpectation",
    "VerificationFailure",
    "CaseWorkspace",
]
