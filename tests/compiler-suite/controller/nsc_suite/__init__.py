"""Infrastructure for the NativeSharp compiler suite controller."""

from .events import CancellationToken, EventKind, EventWriter, SuiteEvent
from .manifest import CaseManifest, InventoryIndex, ManifestError, load_manifest
from .oracle import ComparisonFailure, compare_structured
from .report import ReportError, ReportWriter, RunMetadata, ToolIdentity
from .runner import RunnerError, SuiteRunner
from .result import CaseResult, CaseStatus, DependencyDecision, ResultLedger
from .selector import Selection, SelectionError, select_cases
from .workspace import CaseWorkspace, WorkspaceError, WorkspaceManager

__all__ = [
    "CaseManifest",
    "ComparisonFailure",
    "CaseResult",
    "CaseStatus",
    "CaseWorkspace",
    "CancellationToken",
    "DependencyDecision",
    "EventKind",
    "EventWriter",
    "InventoryIndex",
    "ManifestError",
    "ReportError",
    "ReportWriter",
    "ResultLedger",
    "RunMetadata",
    "RunnerError",
    "Selection",
    "SelectionError",
    "SuiteEvent",
    "SuiteRunner",
    "ToolIdentity",
    "WorkspaceError",
    "WorkspaceManager",
    "load_manifest",
    "compare_structured",
    "select_cases",
]
