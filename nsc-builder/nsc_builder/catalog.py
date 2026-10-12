"""Extracted catalog implementation for modular NSC Builder 3."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import uuid
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

from .constants import *
from .paths import *
from .models import *
from .utils import *


def read_csv(path: Path) -> list[dict[str, str]]:
    """Read one extensible CSV using the canonical structural validation."""
    return read_catalog_csv(path, ())


def read_catalog_csv(
    path: Path, required_fields: Sequence[str]
) -> list[dict[str, str]]:
    """Read a catalogue CSV while preserving extensible named columns."""
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        headers = reader.fieldnames
        if headers is None:
            raise ValueError(f"{path}: missing CSV header")
        if any(header is None or not header.strip() for header in headers):
            raise ValueError(f"{path}: CSV header contains an empty name")
        if len(headers) != len(set(headers)):
            raise ValueError(f"{path}: CSV header contains duplicate names")
        missing = [field for field in required_fields if field not in headers]
        if missing:
            raise ValueError(
                f"{path}: CSV header is missing required fields: {', '.join(missing)}"
            )
        rows: list[dict[str, str]] = []
        for row_number, row in enumerate(reader, start=2):
            if None in row:
                raise ValueError(f"{path}: row {row_number} contains extra cells")
            blank = [
                field
                for field in required_fields
                if row.get(field) is None or not row[field].strip()
            ]
            if blank:
                raise ValueError(
                    f"{path}: row {row_number} has blank required fields: "
                    + ", ".join(blank)
                )
            rows.append(row)
        return rows


def parse_wave_number(value: str) -> int:
    """Parse one canonical positive decimal implementation-wave number."""
    if re.fullmatch(r"[1-9][0-9]*", value) is None:
        raise ValueError(f"invalid implementation wave: {value!r}")
    return int(value)


def validate_allocation_domains(rows: Sequence[dict[str, str]]) -> None:
    """Reject allocation values that selection or expansion cannot represent."""
    scalar_domains = {
        "layer": ALLOCATION_LAYERS,
        "planned_executor": ALLOCATION_EXECUTORS,
        "default_profile": frozenset(PROFILE_RANK),
        "isolation": ALLOCATION_ISOLATIONS,
    }
    list_domains = {
        "supported_build_modes": (ALLOCATION_BUILD_MODES, {"FROZEN_CONTRACT"}),
        "supported_optimizations": (
            ALLOCATION_OPTIMIZATIONS,
            {"N/A", "FROZEN_CONTRACT"},
        ),
    }
    for row_number, row in enumerate(rows, start=2):
        case_id = row.get("case_id", f"row {row_number}")
        for field_name, allowed in scalar_domains.items():
            value = row[field_name]
            if value not in allowed:
                raise ValueError(
                    f"allocation {case_id} has unsupported {field_name}: {value!r}"
                )
        for field_name, (allowed, exclusive) in list_domains.items():
            values = split_semicolon(row[field_name])
            if not values or len(values) != len(set(values)):
                raise ValueError(
                    f"allocation {case_id} has invalid {field_name}: {row[field_name]!r}"
                )
            unknown = sorted(set(values) - allowed)
            if unknown:
                raise ValueError(
                    f"allocation {case_id} has unsupported {field_name}: "
                    + ", ".join(unknown)
                )
            if len(values) > 1 and set(values) & exclusive:
                raise ValueError(
                    f"allocation {case_id} mixes sentinel in {field_name}: "
                    f"{row[field_name]!r}"
                )


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    """Read non-empty JSON Lines objects and retain their source order."""
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as stream:
        for number, line in enumerate(stream, start=1):
            if not line.strip():
                continue
            try:
                value = json.loads(line, object_pairs_hook=unique_json_object)
            except ValueError as error:
                raise ValueError(f"{path}:{number}: {error}") from error
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{number}: expected a JSON object")
            rows.append(value)
    return rows


def validate_executable_manifest(
    manifest_path: Path = MANIFEST_PATH,
    components_path: Path = COMPONENTS_PATH,
    contracts_path: Path = CONTRACTS_PATH,
) -> tuple[Any, ...]:
    """Validate the GUI catalogue with the controller's canonical schema.

    Builder 3 intentionally delegates this contract instead of maintaining a
    second permissive parser.  The controller directory is exposed only while
    importing its public package; no repository or process state is changed.
    """
    controller_path = str(CONTROLLER_ROOT)
    inserted = controller_path not in sys.path
    if inserted:
        sys.path.insert(0, controller_path)
    try:
        from nsc_suite.manifest import InventoryIndex, load_manifest

        inventory = InventoryIndex.from_csv(components_path, contracts_path)
        return load_manifest(manifest_path, inventory)
    finally:
        if inserted:
            sys.path.remove(controller_path)


def split_semicolon(value: str) -> tuple[str, ...]:
    """Normalize semicolon-separated allocation fields into stable tuples."""
    return tuple(part.strip() for part in value.split(";") if part.strip())


def load_expected_outcome_overrides(
    path: Path = EXPECTED_OUTCOME_OVERRIDES_PATH,
) -> dict[str, str]:
    """Load explicit Builder-only corrections missing from the suite schema."""
    value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_json_object)
    if not isinstance(value, dict) or value.get("schema") != 1:
        raise ValueError(f"{path}: expected schema 1 object")
    rows = value.get("cases")
    if not isinstance(rows, list):
        raise ValueError(f"{path}: cases must be an array")
    overrides: dict[str, str] = {}
    for index, row in enumerate(rows, start=1):
        if not isinstance(row, dict) or set(row) != {"case_id", "expected_outcome"}:
            raise ValueError(f"{path}: invalid case row {index}")
        case_id = row["case_id"]
        expected = row["expected_outcome"]
        if not isinstance(case_id, str) or not case_id or expected != "KNOWN_FAILURE":
            raise ValueError(f"{path}: invalid expected outcome row {index}")
        if case_id in overrides:
            raise ValueError(f"{path}: duplicate case {case_id}")
        overrides[case_id] = expected
    return overrides


class CanonicalCatalog:
    """Join the executable Phase 2 design, allocation, and manifest inventories."""

    def __init__(self, cases: Sequence[CaseRecord]) -> None:
        self.cases = tuple(cases)
        self.by_id = {case.case_id: case for case in self.cases}
        self.components = tuple(sorted({case.component for case in self.cases}))
        self.contracts = tuple(sorted({case.contract_id for case in self.cases}))
        self.waves = tuple(sorted({case.wave for case in self.cases}))

    @classmethod
    def load(cls) -> "CanonicalCatalog":
        """Load and validate the complete compiler-only denominator."""
        validated_manifests = validate_executable_manifest()
        validated_by_id = {case.case_id: case for case in validated_manifests}
        waves = read_catalog_csv(WAVES_PATH, WAVE_CATALOG_FIELDS)
        wave_by_component = {
            row["component"]: parse_wave_number(row["wave"]) for row in waves
        }
        purpose_by_component = {row["component"]: row["purpose"] for row in waves}
        design = [
            row
            for row in read_catalog_csv(DESIGN_PATH, DESIGN_CATALOG_FIELDS)
            if row["component"] in wave_by_component
        ]
        allocation_rows = [
            row
            for row in read_catalog_csv(ALLOCATION_PATH, ALLOCATION_CATALOG_FIELDS)
            if row["component"] in wave_by_component
        ]
        validate_allocation_domains(allocation_rows)
        manifest_rows = read_jsonl(MANIFEST_PATH)
        expected_overrides = load_expected_outcome_overrides()
        cls._validate_sources(design, allocation_rows, manifest_rows, waves)
        allocation = {row["case_id"]: row for row in allocation_rows}
        manifest = {row["case_id"]: row for row in manifest_rows}
        unknown_overrides = sorted(set(expected_overrides) - set(manifest))
        if unknown_overrides:
            raise ValueError(
                "expected-outcome overrides contain foreign cases: "
                + ", ".join(unknown_overrides)
            )
        cases: list[CaseRecord] = []
        for row in design:
            case_id = row["case_id"]
            assigned = allocation[case_id]
            executable = manifest.get(case_id)
            canonical = validated_by_id.get(case_id)
            disposition = "Executable"
            expected = ""
            if canonical is not None:
                expected = canonical.expected_outcome
            expected = expected_overrides.get(case_id, expected)
            if expected == "KNOWN_FAILURE":
                disposition = "KnownProductDefect"
            cases.append(
                CaseRecord(
                    case_id=case_id,
                    contract_id=row["contract_id"],
                    component=row["component"],
                    wave=wave_by_component[row["component"]],
                    purpose=purpose_by_component[row["component"]],
                    layer=canonical.layer if canonical is not None else assigned["layer"],
                    profile=canonical.profile
                    if canonical is not None
                    else assigned["default_profile"],
                    build_modes=canonical.build_modes
                    if canonical is not None
                    else split_semicolon(assigned["supported_build_modes"]),
                    optimizations=canonical.optimizations
                    if canonical is not None
                    else cls._optimizations(assigned),
                    executor=canonical.executor
                    if canonical is not None
                    else assigned["planned_executor"],
                    disposition=disposition,
                    objective=row["objective"],
                    expected_outcome=expected,
                    manifest=executable,
                )
            )
        return cls(cases)

    @staticmethod
    def _optimizations(allocation: dict[str, str]) -> tuple[str, ...]:
        """Normalize allocation optimizations for display."""
        values = split_semicolon(allocation["supported_optimizations"])
        normalized = tuple("NA" if value in {"N/A", ""} else value for value in values)
        return normalized or ("NA",)

    @staticmethod
    def _validate_sources(
        design: Sequence[dict[str, str]],
        allocation: Sequence[dict[str, str]],
        manifest: Sequence[dict[str, Any]],
        waves: Sequence[dict[str, str]],
    ) -> None:
        """Reject relational omissions without freezing revision-specific totals."""
        wave_components = [row["component"] for row in waves]
        design_ids = [row["case_id"] for row in design]
        allocation_ids = [row["case_id"] for row in allocation]
        manifest_ids = [row["case_id"] for row in manifest]
        if not wave_components:
            raise ValueError("implementation waves do not contain compiler components")
        if len(wave_components) != len(set(wave_components)):
            raise ValueError("implementation-waves.csv contains duplicate compiler components")
        if not design_ids:
            raise ValueError("test-case-design.csv does not contain compiler cases")
        if len(design_ids) != len(set(design_ids)):
            raise ValueError("test-case-design.csv contains duplicate compiler-suite IDs")
        if len(allocation_ids) != len(set(allocation_ids)):
            raise ValueError("suite-case-allocation.csv contains duplicate compiler-suite IDs")
        if len(manifest_ids) != len(set(manifest_ids)):
            raise ValueError("manifest contains duplicate executable compiler-suite IDs")
        design_set = set(design_ids)
        allocation_set = set(allocation_ids)
        missing_allocation = sorted(design_set - allocation_set)
        if missing_allocation:
            raise ValueError(f"allocation is missing {len(missing_allocation)} compiler cases")
        foreign_allocation = sorted(allocation_set - design_set)
        if foreign_allocation:
            raise ValueError(f"allocation contains {len(foreign_allocation)} foreign compiler cases")
        design_component_by_id = {row["case_id"]: row["component"] for row in design}
        mismatched_components = [
            row["case_id"]
            for row in allocation
            if design_component_by_id.get(row["case_id"]) != row["component"]
        ]
        if mismatched_components:
            raise ValueError(
                f"allocation disagrees with design for {len(mismatched_components)} compiler cases"
            )
        foreign_manifest = sorted(set(manifest_ids) - design_set)
        if foreign_manifest:
            raise ValueError(f"manifest contains {len(foreign_manifest)} non-canonical cases")
        missing_manifest = sorted(design_set - set(manifest_ids))
        if missing_manifest:
            raise ValueError(f"manifest is missing {len(missing_manifest)} canonical cases")

    def summary(self) -> dict[str, Any]:
        """Return deterministic catalogue counts for the audit mode and GUI."""
        dispositions = Counter(case.disposition for case in self.cases)
        return {
            "cases": len(self.cases),
            "components": len(self.components),
            "contracts": len(self.contracts),
            "waves": list(self.waves),
            "executable": sum(case.executable for case in self.cases),
            "dispositions": dict(sorted(dispositions.items())),
        }

    def select(
        self,
        scope: str,
        value: str,
        profile: str,
    ) -> tuple[CaseRecord, ...]:
        """Select canonical rows without converting pending rows into executable cases."""
        selected: list[CaseRecord] = []
        for case in self.cases:
            if scope == "Ola" and str(case.wave) != value:
                continue
            if scope == "Componente" and case.component != value:
                continue
            if scope == "Contrato" and case.contract_id != value:
                continue
            if scope == "Caso" and case.case_id != value:
                continue
            if profile != "Todos" and PROFILE_RANK.get(case.profile, 99) > PROFILE_RANK[profile]:
                continue
            selected.append(case)
        return tuple(selected)


def refresh_run_variants(
    catalog: CanonicalCatalog, variants: Sequence[RunVariant]
) -> tuple[RunVariant, ...]:
    """Rebind an existing run selection to freshly loaded case definitions."""
    refreshed: list[RunVariant] = []
    for variant in variants:
        case = catalog.by_id.get(variant.case.case_id)
        if case is None or not case.executable:
            raise ValueError(
                f"case is absent from the current executable catalogue: {variant.case.case_id}"
            )
        if variant.build_mode not in case.build_modes:
            raise ValueError(f"build mode changed for current case: {variant.key}")
        if variant.optimization not in case.optimizations:
            raise ValueError(f"optimization changed for current case: {variant.key}")
        refreshed.append(RunVariant(case, variant.build_mode, variant.optimization))
    return tuple(refreshed)


def expand_variants(
    cases: Iterable[CaseRecord], build_mode: str, optimization: str
) -> tuple[RunVariant, ...]:
    """Expand selected executable cases into their supported configuration matrix."""
    variants: list[RunVariant] = []
    requested_modes = ("Debug", "Release") if build_mode == "Todos" else (build_mode,)
    requested_opts = ("O0", "O1", "O2", "O3") if optimization == "Todas" else (optimization,)
    for case in cases:
        if not case.executable:
            continue
        for mode in requested_modes:
            if mode not in case.build_modes:
                continue
            supported = case.optimizations
            effective = ("NA",) if supported == ("NA",) else requested_opts
            for opt in effective:
                if opt in supported:
                    variants.append(RunVariant(case, mode, opt))
    return tuple(variants)
