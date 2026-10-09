"""Infrastructure for the NativeSharp compiler suite controller."""

from .events import CancellationToken, EventKind, EventWriter, SuiteEvent
from .manifest import CaseManifest, InventoryIndex, ManifestError, load_manifest
from .oracle import ComparisonFailure, compare_structured
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
    "ResultLedger",
    "Selection",
    "SelectionError",
    "SuiteEvent",
    "WorkspaceError",
    "WorkspaceManager",
    "load_manifest",
    "compare_structured",
    "select_cases",
]
