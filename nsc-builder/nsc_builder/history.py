"""Extracted history implementation for modular NSC Builder 3."""

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
from .catalog import *


def preparation_failure_from_payload(raw: Any) -> PreparationFailure | None:
    """Validate and restore the optional run-level preparation closure."""
    if raw is None:
        return None
    if not isinstance(raw, dict) or set(raw) != {
        "status",
        "stage",
        "detail",
        "evidence",
        "command",
        "producer_key",
    }:
        raise ValueError("preparation failure has an invalid schema")
    if raw["status"] not in {"PREPARATION_FAILED", "CANCELLED"}:
        raise ValueError("preparation failure has an invalid status")
    for field_name in ("stage", "detail", "evidence", "producer_key"):
        if not isinstance(raw[field_name], str):
            raise ValueError(f"preparation failure {field_name} must be text")
    if not raw["stage"] or not raw["detail"]:
        raise ValueError("preparation failure requires a stage and detail")
    command = raw["command"]
    if not isinstance(command, list) or any(
        not isinstance(argument, str) for argument in command
    ):
        raise ValueError("preparation failure command must be a text list")
    return PreparationFailure(
        status=raw["status"],
        stage=raw["stage"],
        detail=raw["detail"],
        evidence=raw["evidence"],
        command=tuple(command),
        producer_key=raw["producer_key"],
    )


def variant_history_key(variant: RunVariant) -> tuple[str, str, str, str]:
    """Include the manifest profile in addition to the concrete variant identity."""
    return (
        variant.case.case_id,
        variant.build_mode,
        variant.optimization,
        variant.case.profile,
    )


def timeout_limit_for_average(average_seconds: float | None) -> int:
    """Return the PASS-average deadline, always rounded upward to whole seconds.

    No history gets 45 minutes. Averages below one second get five seconds;
    averages through 30 seconds add five, through 120 add fifteen, through ten
    minutes add three minutes, and longer averages add the greater of five
    minutes or twenty percent. Upward rounding never reduces those margins.
    """
    if average_seconds is None:
        return 45 * 60
    if (
        isinstance(average_seconds, bool)
        or not isinstance(average_seconds, (int, float))
        or not math.isfinite(average_seconds)
        or average_seconds < 0
    ):
        raise ValueError("historical PASS average must be a finite nonnegative number")
    average = float(average_seconds)
    if average < 1.0:
        return 5
    if average <= 30.0:
        return math.ceil(average + 5.0)
    if average <= 120.0:
        return math.ceil(average + 15.0)
    if average <= 600.0:
        return math.ceil(average + 180.0)
    return math.ceil(average + max(300.0, average * 0.20))


def load_variant_timeout_budgets(
    variants: Sequence[RunVariant], history_root: Path = HISTORY_ROOT
) -> dict[tuple[str, str, str, str], VariantTimeoutBudget]:
    """Load all trustworthy PASS durations once and calculate every requested budget."""
    requested = {variant_history_key(variant) for variant in variants}
    samples: dict[tuple[str, str, str, str], list[float]] = {
        key: [] for key in requested
    }
    profiles_by_identity = {
        (case_id, build_mode, optimization): profile
        for case_id, build_mode, optimization, profile in requested
    }
    if history_root.is_dir():
        for root in history_root.iterdir():
            if not root.is_dir():
                continue
            record = history_record(root)
            if (
                record[1] != "phase-2-suite"
                or record[2] in {"INCOMPLETE", "MALFORMED"}
            ):
                continue
            try:
                payload = json.loads(
                    (root / "results.json").read_text(encoding="utf-8"),
                    object_pairs_hook=unique_json_object,
                )
            except (OSError, json.JSONDecodeError, TypeError, ValueError):
                continue
            rows = payload.get("results") if isinstance(payload, dict) else None
            if not isinstance(rows, list):
                continue
            for row in rows:
                if not isinstance(row, dict) or row.get("status") != "PASS":
                    continue
                identity = (
                    row.get("case_id"),
                    row.get("build_mode"),
                    row.get("optimization"),
                )
                profile = profiles_by_identity.get(identity)
                recorded_profile = row.get("profile")
                duration = row.get("duration_seconds")
                if (
                    profile is None
                    or (
                        recorded_profile is not None
                        and recorded_profile != profile
                    )
                    or isinstance(duration, bool)
                    or not isinstance(duration, (int, float))
                    or not math.isfinite(duration)
                    or duration < 0
                ):
                    continue
                samples[(*identity, profile)].append(float(duration))
    budgets: dict[tuple[str, str, str, str], VariantTimeoutBudget] = {}
    for key, durations in samples.items():
        average = sum(durations) / len(durations) if durations else None
        budgets[key] = VariantTimeoutBudget(
            pass_count=len(durations),
            average_seconds=average,
            limit_seconds=timeout_limit_for_average(average),
        )
    return budgets


class EvidenceStore:
    """Write crash-surviving run evidence and final human/machine reports."""

    def __init__(self, run_id: str, metadata: dict[str, Any]) -> None:
        self.run_id = run_id
        self.expected_variant_count = metadata.get("variant_count")
        self.root = HISTORY_ROOT / run_id
        self.root.mkdir(parents=True, exist_ok=False)
        self.events_path = self.root / "events.jsonl"
        self.log_path = self.root / "full.log"
        self._event_stream = self.events_path.open("a", encoding="utf-8", newline="\n")
        self._log_stream = self.log_path.open("a", encoding="utf-8", newline="\n")
        write_json_atomic(self.root / "metadata.json", metadata)
        self.event("RUN_STARTED", metadata)

    def event(self, kind: str, payload: dict[str, Any]) -> None:
        """Append and flush one structured event so crashes retain progress."""
        record = {"schema": 1, "timestamp": utc_now(), "kind": kind, "payload": payload}
        self._event_stream.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")
        self._event_stream.flush()

    def log(self, line: str) -> None:
        """Append progressive textual output without delaying visibility."""
        self._log_stream.write(line)
        if not line.endswith("\n"):
            self._log_stream.write("\n")
        self._log_stream.flush()

    def finish(
        self,
        outcomes: Sequence[CaseOutcome],
        cancelled: bool,
        preparation: PreparationFailure | None = None,
    ) -> None:
        """Publish final results and a concise Markdown report."""
        keys = [outcome.key for outcome in outcomes]
        if len(keys) != len(set(keys)):
            raise ValueError("duplicate terminal result keys")
        if preparation is not None:
            if outcomes:
                raise ValueError("preparation failure cannot contain case results")
            if cancelled != (preparation.status == "CANCELLED"):
                raise ValueError("preparation cancellation does not match run cancellation")
        expected = self.expected_variant_count
        if expected is not None:
            if isinstance(expected, bool) or not isinstance(expected, int) or expected < 1:
                raise ValueError(f"invalid metadata variant_count: {expected!r}")
            if preparation is None and len(outcomes) != expected:
                raise ValueError(
                    f"terminal result cardinality {len(outcomes)} does not match "
                    f"variant_count {expected}"
                )
        counts = Counter(outcome.status for outcome in outcomes)
        if preparation is None:
            overall = self._overall_status(counts)
            status = "CANCELLED" if cancelled else overall
        else:
            status = preparation.status
        payload = {
            "run_id": self.run_id,
            "status": status,
            "finished_at": utc_now(),
            "counts": dict(sorted(counts.items())),
            "results": [asdict(outcome) for outcome in outcomes],
        }
        if preparation is not None:
            payload["preparation"] = asdict(preparation)
        write_json_atomic(self.root / "results.json", payload)
        lines = [
            f"# NSC Builder 3 — {self.run_id}",
            "",
            f"Estado: `{status}`",
            "",
            "| Estado | Cantidad |",
            "|---|---:|",
        ]
        lines.extend(f"| `{name}` | {count} |" for name, count in sorted(counts.items()))
        failures = [item for item in outcomes if item.status in TERMINAL_FAILURES]
        lines.extend(["", "## Fallos", ""])
        if preparation is not None:
            lines.extend(
                (
                    f"- Preparación `{preparation.stage}`: {preparation.detail}",
                    f"- Evidencia: `{preparation.evidence or 'no disponible'}`",
                )
            )
        elif failures:
            lines.extend(
                f"- `{item.key}`: {item.detail or item.status}" for item in failures
            )
        else:
            lines.append("No se registraron fallos terminales en los casos ejecutados.")
        write_text_atomic(self.root / "summary.md", "\n".join(lines) + "\n")
        self.event("RUN_FINISHED", {"status": status, "counts": dict(counts)})
        self.close()

    @staticmethod
    def _overall_status(counts: Counter[str]) -> str:
        """Keep known defects visible while distinguishing hard failures."""
        if sum(counts.values()) <= 0:
            raise ValueError("cannot publish evidence with no case results")
        unknown = sorted(set(counts) - RESULT_STATUSES)
        if unknown:
            raise ValueError("unknown result states: " + ", ".join(unknown))
        if any(counts[name] for name in TERMINAL_FAILURES):
            return "FAILED"
        if counts["KNOWN_FAILURE"]:
            return "COMPLETED_WITH_KNOWN_DEFECTS"
        return "PASSED"

    def close(self) -> None:
        """Close evidence streams exactly once."""
        if not self._event_stream.closed:
            self._event_stream.close()
        if not self._log_stream.closed:
            self._log_stream.close()


def history_record(root: Path) -> tuple[str, str, str, str, str]:
    """Validate one durable run before projecting it into the history table."""
    run_id = root.name
    fallback = (run_id, "incomplete", "INCOMPLETE", "", str(root))
    metadata_path = root / "metadata.json"
    results_path = root / "results.json"
    summary_path = root / "summary.md"
    events_path = root / "events.jsonl"
    if not metadata_path.is_file():
        return fallback
    try:
        metadata = json.loads(
            metadata_path.read_text(encoding="utf-8"),
            object_pairs_hook=unique_json_object,
        )
        if not isinstance(metadata, dict):
            raise ValueError("metadata must be an object")
        kind = metadata.get("kind")
        if not isinstance(kind, str) or not kind:
            raise ValueError("metadata kind is missing")
        if metadata.get("run_id") != run_id:
            raise ValueError("metadata run_id does not match its directory")
    except (OSError, json.JSONDecodeError, ValueError, TypeError):
        return (run_id, "malformed", "MALFORMED", "", str(root))
    incomplete = (run_id, kind, "INCOMPLETE", "", str(root))
    if not results_path.is_file() or not summary_path.is_file() or not events_path.is_file():
        return incomplete
    try:
        payload = json.loads(
            results_path.read_text(encoding="utf-8"),
            object_pairs_hook=unique_json_object,
        )
        if not isinstance(payload, dict):
            raise ValueError("results must be an object")
        if payload.get("run_id") != run_id:
            raise ValueError("results run_id does not match its directory")
        finished_at = payload.get("finished_at")
        if not isinstance(finished_at, str) or not finished_at:
            raise ValueError("finished_at is missing")
        results = payload.get("results")
        if not isinstance(results, list):
            raise ValueError("terminal results must be a list")
        preparation = preparation_failure_from_payload(payload.get("preparation"))
        if not results and preparation is None:
            raise ValueError("terminal results must be non-empty without a preparation failure")
        if results and preparation is not None:
            raise ValueError("preparation failure cannot coexist with terminal case rows")
        if any(not isinstance(item, dict) for item in results):
            raise ValueError("each terminal result must be an object")
        keys = [item.get("key") for item in results]
        if any(not isinstance(key, str) or not key for key in keys):
            raise ValueError("each terminal result must have a key")
        if len(keys) != len(set(keys)):
            raise ValueError("terminal result keys must be unique")
        statuses = [item.get("status") for item in results]
        if any(status not in RESULT_STATUSES for status in statuses):
            raise ValueError("terminal result contains an unknown status")
        observed = Counter(statuses)
        raw_counts = payload.get("counts")
        if not isinstance(raw_counts, dict):
            raise ValueError("result counts must be an object")
        if any(
            not isinstance(name, str)
            or name not in RESULT_STATUSES
            or isinstance(count, bool)
            or not isinstance(count, int)
            or count <= 0
            for name, count in raw_counts.items()
        ):
            raise ValueError("result counts contain an invalid entry")
        counts = Counter(raw_counts)
        if counts != observed:
            raise ValueError("result counts do not match terminal rows")
        expected_count = metadata.get("variant_count")
        if expected_count is not None:
            if (
                isinstance(expected_count, bool)
                or not isinstance(expected_count, int)
                or expected_count < 1
                or (preparation is None and expected_count != len(results))
            ):
                raise ValueError("metadata variant_count does not match terminal rows")
        status = payload.get("status")
        if preparation is not None:
            if status != preparation.status:
                raise ValueError("overall status does not match preparation failure")
        else:
            expected_status = EvidenceStore._overall_status(counts)
            if status == "CANCELLED":
                if counts["CANCELLED"] <= 0:
                    raise ValueError("cancelled run has no cancelled result")
            elif status != expected_status:
                raise ValueError("overall status does not match terminal rows")
        events = read_jsonl(events_path)
        if not events:
            raise ValueError("event stream is empty")
        final_event = events[-1]
        final_payload = final_event.get("payload")
        if final_event.get("kind") != "RUN_FINISHED" or not isinstance(final_payload, dict):
            raise ValueError("event stream lacks a final RUN_FINISHED event")
        if final_payload.get("status") != status or Counter(final_payload.get("counts", {})) != counts:
            raise ValueError("final event does not match results")
        if preparation is not None and any(
            event.get("kind") == "CASE_STARTED" for event in events
        ):
            raise ValueError("preparation-failed run dispatched a case")
        if preparation is not None:
            cause_events = [
                event
                for event in events
                if event.get("kind")
                in {"PREPARATION_FAILED", "PREPARATION_CANCELLED"}
            ]
            if len(cause_events) != 1:
                raise ValueError("preparation-failed run lacks one causal event")
            restored_cause = preparation_failure_from_payload(
                cause_events[0].get("payload")
            )
            if restored_cause != preparation:
                raise ValueError("preparation causal event does not match results")
    except (OSError, json.JSONDecodeError, ValueError, TypeError):
        return (run_id, kind, "MALFORMED", "", str(root))
    return (run_id, kind, str(status), finished_at, str(root))


def load_history_execution(
    root: Path, catalog: CanonicalCatalog
) -> HistoricalExecution:
    """Load one completed suite run without mutating or replacing its evidence."""
    record = history_record(root)
    if record[1] != "phase-2-suite":
        raise ValueError("the selected history row is not a Phase 2 suite execution")
    if record[2] in {"INCOMPLETE", "MALFORMED"}:
        raise ValueError(f"the selected execution is {record[2].lower()}")
    metadata = json.loads(
        (root / "metadata.json").read_text(encoding="utf-8"),
        object_pairs_hook=unique_json_object,
    )
    payload = json.loads(
        (root / "results.json").read_text(encoding="utf-8"),
        object_pairs_hook=unique_json_object,
    )
    if not isinstance(metadata, dict) or not isinstance(payload, dict):
        raise ValueError("historical execution envelopes must be objects")
    selection = metadata.get("selection")
    selection_fields = {
        "scope",
        "value",
        "profile",
        "build_mode",
        "optimization",
    }
    if not isinstance(selection, dict) or set(selection) != selection_fields:
        raise ValueError("historical execution has an invalid selection envelope")
    if any(not isinstance(selection[field], str) for field in selection_fields):
        raise ValueError("historical execution selection fields must be text")
    scope = selection["scope"]
    value = selection["value"]
    profile = selection["profile"]
    build_mode = selection["build_mode"]
    optimization = selection["optimization"]
    if scope not in {"Total", "Ola", "Componente", "Contrato", "Caso"}:
        raise ValueError(f"historical execution has an unknown scope: {scope!r}")
    if scope != "Total" and not value:
        raise ValueError("historical non-total scope requires a value")
    if scope == "Total" and value:
        raise ValueError("historical total scope cannot have a value")
    if profile not in {"Todos", *PROFILE_RANK}:
        raise ValueError(f"historical execution has an unknown profile: {profile!r}")
    if build_mode not in {"Debug", "Release", "Todos"}:
        raise ValueError(
            f"historical execution has an unknown build mode: {build_mode!r}"
        )
    if optimization not in {"O0", "O1", "O2", "O3", "Todas"}:
        raise ValueError(
            "historical execution has an unknown optimization: "
            f"{optimization!r}"
        )
    selected_cases = catalog.select(scope, value, profile)
    if not selected_cases:
        raise ValueError(
            "historical case selection is absent from the current catalogue"
        )
    selected_variants = expand_variants(
        selected_cases, build_mode, optimization
    )
    if not selected_variants:
        raise ValueError("historical selection has no executable variants")
    expected_case_count = metadata.get("case_count")
    executable_case_count = len(
        {variant.case.case_id for variant in selected_variants}
    )
    if (
        isinstance(expected_case_count, bool)
        or not isinstance(expected_case_count, int)
        or expected_case_count != executable_case_count
    ):
        raise ValueError(
            "historical case_count does not match its executable selection"
        )
    rows = payload.get("results")
    if not isinstance(rows, list):
        raise ValueError("historical execution has no terminal result rows")
    variants: list[RunVariant] = []
    outcomes: list[CaseOutcome] = []
    preparation = preparation_failure_from_payload(payload.get("preparation"))
    for index, raw in enumerate(rows, start=1):
        if not isinstance(raw, dict):
            raise ValueError(f"historical result {index} must be an object")
        base_fields = {
            "key",
            "case_id",
            "contract_id",
            "component",
            "wave",
            "build_mode",
            "optimization",
            "status",
            "duration_seconds",
            "detail",
            "evidence",
        }
        optional_fields = {
            "profile",
            "timeout_pass_count",
            "timeout_average_seconds",
            "timeout_limit_seconds",
        }
        raw_fields = set(raw)
        if not base_fields <= raw_fields or not raw_fields <= base_fields | optional_fields:
            raise ValueError(f"historical result {index} has an invalid schema")
        text_fields = (
            "key",
            "case_id",
            "contract_id",
            "component",
            "build_mode",
            "optimization",
            "status",
            "detail",
            "evidence",
        )
        if any(not isinstance(raw[field], str) for field in text_fields):
            raise ValueError(f"historical result {index} has a non-text field")
        if any(
            not raw[field]
            for field in (
                "key",
                "case_id",
                "contract_id",
                "component",
                "build_mode",
                "optimization",
                "status",
            )
        ):
            raise ValueError(f"historical result {index} has a blank identity field")
        wave = raw["wave"]
        duration = raw["duration_seconds"]
        if isinstance(wave, bool) or not isinstance(wave, int) or wave < 0:
            raise ValueError(f"historical result {index} has an invalid wave")
        if (
            isinstance(duration, bool)
            or not isinstance(duration, (int, float))
            or not math.isfinite(duration)
            or duration < 0
        ):
            raise ValueError(f"historical result {index} has an invalid duration")
        if raw["status"] not in RESULT_STATUSES:
            raise ValueError(f"historical result {index} has an invalid status")
        recorded_profile = raw.get("profile")
        if recorded_profile is not None and (
            not isinstance(recorded_profile, str) or not recorded_profile
        ):
            raise ValueError(f"historical result {index} has an invalid profile")
        timeout_pass_count = raw.get("timeout_pass_count", 0)
        timeout_average = raw.get("timeout_average_seconds")
        timeout_limit = raw.get("timeout_limit_seconds")
        if (
            isinstance(timeout_pass_count, bool)
            or not isinstance(timeout_pass_count, int)
            or timeout_pass_count < 0
        ):
            raise ValueError(f"historical result {index} has an invalid PASS count")
        if timeout_average is not None and (
            isinstance(timeout_average, bool)
            or not isinstance(timeout_average, (int, float))
            or not math.isfinite(timeout_average)
            or timeout_average < 0
        ):
            raise ValueError(f"historical result {index} has an invalid PASS average")
        if timeout_limit is not None and (
            isinstance(timeout_limit, bool)
            or not isinstance(timeout_limit, int)
            or timeout_limit < 1
        ):
            raise ValueError(f"historical result {index} has an invalid timeout limit")
        case = catalog.by_id.get(raw["case_id"])
        if case is None:
            raise ValueError(
                f"historical case is absent from the current catalogue: {raw['case_id']}"
            )
        if (
            case.contract_id != raw["contract_id"]
            or case.component != raw["component"]
            or case.wave != wave
        ):
            raise ValueError(
                f"historical case identity changed in the current catalogue: {case.case_id}"
            )
        if "profile" in raw and recorded_profile != case.profile:
            raise ValueError(
                f"historical case profile changed in the current catalogue: {case.case_id}"
            )
        variant = RunVariant(case, raw["build_mode"], raw["optimization"])
        if not case.executable or variant.build_mode not in case.build_modes:
            raise ValueError(f"historical variant is no longer executable: {variant.key}")
        if variant.optimization not in case.optimizations or variant.key != raw["key"]:
            raise ValueError(f"historical variant identity is incompatible: {raw['key']}")
        variants.append(variant)
        outcomes.append(
            CaseOutcome(
                key=raw["key"],
                case_id=raw["case_id"],
                contract_id=raw["contract_id"],
                component=raw["component"],
                wave=wave,
                build_mode=raw["build_mode"],
                optimization=raw["optimization"],
                status=raw["status"],
                duration_seconds=float(duration),
                detail=raw["detail"],
                evidence=raw["evidence"],
                profile=case.profile,
                timeout_pass_count=timeout_pass_count,
                timeout_average_seconds=(
                    float(timeout_average) if timeout_average is not None else None
                ),
                timeout_limit_seconds=timeout_limit,
            )
        )
    expected_keys = tuple(variant.key for variant in selected_variants)
    actual_keys = tuple(outcome.key for outcome in outcomes)
    if preparation is None and Counter(actual_keys) != Counter(expected_keys):
        raise ValueError(
            "historical terminal rows do not reproduce the original selection"
        )
    log_path = root / "full.log"
    full_log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""
    return HistoricalExecution(
        root=root.resolve(),
        metadata=metadata,
        status=record[2],
        cases=selected_cases,
        variants=selected_variants,
        outcomes=tuple(outcomes),
        full_log=full_log,
        preparation=preparation,
    )
