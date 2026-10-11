"""Canonical records shared by catalogue, execution, history, and GUI views."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class TestCase:
    """Canonical test definition and executable configuration identity."""

    case_id: str
    contract_id: str
    component: str
    wave: int
    purpose: str
    layer: str
    profile: str
    build_modes: tuple[str, ...]
    optimizations: tuple[str, ...]
    executor: str
    disposition: str
    objective: str
    expected_outcome: str
    manifest: dict[str, Any] | None = field(compare=False, repr=False)

    @property
    def executable(self) -> bool:
        """Report whether the executable manifest owns this definition."""
        return self.manifest is not None


@dataclass(frozen=True)
class TestVariant:
    """One exact test, build-mode, and optimization execution identity."""

    case: TestCase
    build_mode: str
    optimization: str

    @property
    def key(self) -> str:
        """Return the stable identity used by GUI rows and durable evidence."""
        return f"{self.case.case_id}|{self.build_mode}|{self.optimization}"


@dataclass
class TestExecution:
    """Canonical terminal or live projection for one exact test variant."""

    key: str
    case_id: str
    contract_id: str
    component: str
    wave: int
    build_mode: str
    optimization: str
    status: str
    duration_seconds: float = 0.0
    detail: str = ""
    evidence: str = ""
    profile: str = ""
    timeout_pass_count: int = 0
    timeout_average_seconds: float | None = None
    timeout_limit_seconds: int | None = None


@dataclass(frozen=True)
class VariantTimeoutBudget:
    """One per-variant deadline derived exclusively from trusted PASS history."""

    pass_count: int
    average_seconds: float | None
    limit_seconds: int


@dataclass(frozen=True)
class Run:
    """Canonical aggregate restored from one complete durable suite run."""

    root: Path
    metadata: dict[str, Any]
    status: str
    cases: tuple[TestCase, ...]
    variants: tuple[TestVariant, ...]
    outcomes: tuple[TestExecution, ...]
    full_log: str

    @property
    def tests(self) -> tuple[TestCase, ...]:
        """Expose canonical test definitions without duplicating storage."""
        return self.cases

    @property
    def executions(self) -> tuple[TestExecution, ...]:
        """Expose canonical executions without duplicating storage."""
        return self.outcomes


@dataclass(frozen=True)
class TestTreeCleanupResult:
    """Summarize one bounded cleanup of generated test-build directories."""

    targets: tuple[Path, ...]
    missing_targets: tuple[Path, ...]
    removed_files: int
    removed_directories: int
    failures: tuple[str, ...]


def bind_runtime_models(runtime: Any) -> None:
    """Keep the compatibility bridge coherent during staged extraction commits."""
    runtime.CaseRecord = TestCase
    runtime.RunVariant = TestVariant
    runtime.CaseOutcome = TestExecution
    runtime.HistoricalExecution = Run


# Compatibility names make the extraction transparent to existing callers.
CaseRecord = TestCase
RunVariant = TestVariant
CaseOutcome = TestExecution
HistoricalExecution = Run

__all__ = (
    "CaseOutcome",
    "CaseRecord",
    "HistoricalExecution",
    "Run",
    "RunVariant",
    "TestCase",
    "TestExecution",
    "TestTreeCleanupResult",
    "TestVariant",
    "VariantTimeoutBudget",
    "bind_runtime_models",
)
