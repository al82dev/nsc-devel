"""A-drive-only process isolation for test execution and its evidence."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Mapping

from .paths import STORAGE_ROOT, require_a_path


def require_storage_descendant(path: Path, label: str) -> Path:
    """Require an A-drive path owned by Builder 3's versioned storage root."""
    resolved = require_a_path(path, label)
    storage = require_a_path(STORAGE_ROOT, "Builder 3 storage root")
    if resolved != storage and storage not in resolved.parents:
        raise ValueError(f"{label} must remain under {storage}, got {resolved}")
    return resolved


def isolated_test_environment(
    owner_root: Path,
    inherited: Mapping[str, str] | None = None,
) -> dict[str, str]:
    """Create one private A-drive TEMP/staging/evidence namespace for a child."""
    owner = require_storage_descendant(owner_root, "test process root")
    temporary = owner / "temp"
    staging = owner / "staging"
    evidence = owner / "evidence"
    for path in (temporary, staging, evidence):
        path.mkdir(parents=True, exist_ok=True)

    environment = dict(os.environ if inherited is None else inherited)
    environment.update(
        {
            "TEMP": str(temporary),
            "TMP": str(temporary),
            "TMPDIR": str(temporary),
            "NSC_TEST_TEMP_ROOT": str(temporary),
            "NSC_TEST_STAGING_ROOT": str(staging),
            "NSC_TEST_EVIDENCE_ROOT": str(evidence),
        }
    )
    return environment


__all__ = ("isolated_test_environment", "require_storage_descendant")
