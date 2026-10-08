"""Shared implementation for the consolidated NSC test harness."""

from .catalog import CatalogError, TestCatalog
from .command import NscCommandBuilder
from .model import CaseKind, ExpectedOutcome, ManifestError, TestCase, TextExpectation
from .process import ProcessExecutor, ProcessRequest, ProcessResult
from .report import ReportWriter, TestRunRecord
from .scheduler import CaseScheduler, ScheduledItem
from .suites import SuiteAdapter, SuiteCatalog, SuiteDefinition, SuiteError
from .workspace import CaseWorkspace
from .verify import FailureKind, ResultVerifier, TestResult, VerificationFailure

__all__ = [
    "CaseKind",
    "CaseScheduler",
    "CatalogError",
    "ExpectedOutcome",
    "FailureKind",
    "ManifestError",
    "NscCommandBuilder",
    "ProcessExecutor",
    "ProcessRequest",
    "ProcessResult",
    "ReportWriter",
    "SuiteAdapter",
    "SuiteCatalog",
    "SuiteDefinition",
    "SuiteError",
    "TestRunRecord",
    "ResultVerifier",
    "ScheduledItem",
    "TestCase",
    "TestCatalog",
    "TestResult",
    "TextExpectation",
    "VerificationFailure",
    "CaseWorkspace",
]
