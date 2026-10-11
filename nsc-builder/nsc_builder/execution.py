"""Extracted execution implementation for modular NSC Builder 3."""

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
from .configuration import *
from .catalog import *
from .history import *
from .reporting import *
from .process_control import *
from .build import *


def infrastructure_command_outcome(
    *,
    index: int,
    label: str,
    exit_code: int,
    duration_seconds: float,
    cancelled: bool,
    evidence: str,
    captured_output: str = "",
) -> CaseOutcome:
    """Project one infrastructure command into the shared terminal protocol."""
    status = "CANCELLED" if cancelled else ("PASS" if exit_code == 0 else "FAIL")
    if cancelled:
        detail = "operation cancelled"
    elif exit_code != 0:
        detail = infrastructure_failure_detail(exit_code, captured_output)
    else:
        detail = ""
    return CaseOutcome(
        key=f"INFRA|{index}",
        case_id=label,
        contract_id="NSCBUILDER3-INFRASTRUCTURE",
        component="TestInfrastructure",
        wave=0,
        build_mode="NA",
        optimization="NA",
        status=status,
        duration_seconds=duration_seconds,
        detail=detail,
        evidence=evidence,
    )


def infrastructure_failure_detail(exit_code: int, captured_output: str) -> str:
    """Keep one bounded causal diagnostic while full.log retains all output."""
    prefix = f"exit code {exit_code}"
    lines = [line.strip() for line in captured_output.splitlines() if line.strip()]
    for index, line in enumerate(lines):
        folded = line.casefold()
        if not (
            folded.startswith("cmake error")
            or ": error:" in folded
            or ": error " in folded
            or "fatal error" in folded
            or folded.startswith("ninja: error:")
        ):
            continue
        diagnostic = line
        if folded.startswith("cmake error") and index + 1 < len(lines):
            continuation = lines[index + 1]
            if not continuation.casefold().startswith("call stack"):
                diagnostic = f"{diagnostic} — {continuation}"
        return f"{prefix}: {diagnostic}"[:800]
    return prefix


def isolate_missing_llvm_discovery(
    case_id: str,
    command: Sequence[str],
    environment: dict[str, str],
) -> tuple[list[str], dict[str, str]]:
    """Make the missing-LLVM BuildSystem oracle independent of host discovery."""
    controlled_command = list(command)
    controlled_environment = dict(environment)
    if case_id != "T12B-00453":
        return controlled_command, controlled_environment
    blocked = {
        "LLVM_DIR",
        "LLVM_ROOT",
        "LLVM_HOME",
        "CMAKE_PREFIX_PATH",
        "CMAKE_MODULE_PATH",
    }
    controlled_environment = {
        key: value
        for key, value in controlled_environment.items()
        if key.upper() not in blocked
    }
    if not any(argument.startswith("-DLLVM_DIR=") for argument in controlled_command):
        controlled_command.append("-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE")
    return controlled_command, controlled_environment


def cancelled_infrastructure_outcomes(
    commands: Sequence[tuple[str, Sequence[str], Path]],
    first_unstarted_index: int,
    evidence: str,
) -> tuple[CaseOutcome, ...]:
    """Close every unstarted command after an infrastructure cancellation."""
    if not 1 <= first_unstarted_index <= len(commands) + 1:
        raise ValueError("first unstarted command index is outside the command plan")
    outcomes: list[CaseOutcome] = []
    for index in range(first_unstarted_index, len(commands) + 1):
        label = commands[index - 1][0]
        outcome = infrastructure_command_outcome(
            index=index,
            label=label,
            exit_code=130,
            duration_seconds=0.0,
            cancelled=True,
            evidence=evidence,
        )
        outcome.detail = "not started because operation was cancelled"
        outcomes.append(outcome)
    return tuple(outcomes)


def blocked_infrastructure_outcomes(
    commands: Sequence[tuple[str, Sequence[str], Path]],
    first_unstarted_index: int,
    evidence: str,
    failed_label: str,
) -> tuple[CaseOutcome, ...]:
    """Close every command whose prerequisite infrastructure command failed."""
    if not 1 <= first_unstarted_index <= len(commands) + 1:
        raise ValueError("first unstarted command index is outside the command plan")
    outcomes: list[CaseOutcome] = []
    for index in range(first_unstarted_index, len(commands) + 1):
        label = commands[index - 1][0]
        outcomes.append(
            CaseOutcome(
                key=f"INFRA|{index}",
                case_id=label,
                contract_id="NSCBUILDER3-INFRASTRUCTURE",
                component="TestInfrastructure",
                wave=0,
                build_mode="NA",
                optimization="NA",
                status="BLOCKED",
                detail=f"not started because prerequisite failed: {failed_label}",
                evidence=evidence,
            )
        )
    return tuple(outcomes)


def expected_abnormal_termination_detail(
    case: CaseRecord, exit_code: int, captured: str
) -> str | None:
    """Recognize only a manifest-authorized known process termination."""
    manifest = case.manifest or {}
    oracle = manifest.get("oracle")
    if not isinstance(oracle, dict):
        return None
    abnormal = oracle.get("abnormal_termination")
    if abnormal is None:
        return None
    if case.expected_outcome != "KNOWN_FAILURE" or case.disposition != "KnownProductDefect":
        raise ValueError("abnormal termination contract belongs to a non-defect case")
    tags = manifest.get("tags")
    if not isinstance(tags, list) or not {"known-defect", "unsafe-boundary"} <= set(tags):
        raise ValueError("abnormal termination contract lacks required defect tags")
    if not isinstance(abnormal, dict) or set(abnormal) != {
        "exit_code",
        "output_contains",
    }:
        raise ValueError("invalid abnormal termination contract")
    if abnormal.get("exit_code") != "nonzero" or exit_code == 0:
        raise ValueError(
            f"abnormal termination exit code {exit_code} contradicts nonzero contract"
        )
    fragments = abnormal.get("output_contains")
    if (
        not isinstance(fragments, list)
        or not fragments
        or not all(isinstance(value, str) and value for value in fragments)
    ):
        raise ValueError("invalid abnormal termination output contract")
    missing = [value for value in fragments if value not in captured]
    if missing:
        raise ValueError(
            "abnormal termination output omits: " + ", ".join(repr(value) for value in missing)
        )
    return (
        f"known product defect terminated the isolated host process with exit code {exit_code}; "
        + "; ".join(f"observed {value!r}" for value in fragments)
    )


def parse_host_envelope(
    *,
    captured: str,
    exit_code: int,
    case_id: str,
    contract_id: str,
    component: str,
) -> tuple[bool, str]:
    """Validate the exact single-case protocol emitted by the consolidated host."""
    lines = [line for line in captured.splitlines() if line.strip()]
    if len(lines) != 1:
        raise ValueError(f"host must emit exactly one JSON object, got {len(lines)}")
    payload = json.loads(lines[0], object_pairs_hook=unique_json_object)
    if not isinstance(payload, dict):
        raise ValueError("host result must be a JSON object")
    expected_identity = (case_id, contract_id, component)
    actual_identity = (
        payload.get("case_id"),
        payload.get("contract_id"),
        payload.get("component"),
    )
    if actual_identity != expected_identity:
        raise ValueError("host result identity mismatch")
    duration = payload.get("duration_us")
    if isinstance(duration, bool) or not isinstance(duration, int) or duration < 0:
        raise ValueError("host duration_us must be a non-negative integer")
    failures = payload.get("failures")
    if not isinstance(failures, list) or any(
        not isinstance(item, str) or not item for item in failures
    ):
        raise ValueError("host failures must be a list of strings")
    status = payload.get("status")
    if status not in {"PASS", "FAIL"}:
        raise ValueError(f"host status is invalid: {status!r}")
    if status == "PASS" and failures:
        raise ValueError("host PASS result cannot contain failures")
    if status == "FAIL" and not failures:
        raise ValueError("host FAIL result requires failure evidence")
    expected_exit = 0 if status == "PASS" else 1
    if exit_code != expected_exit:
        raise ValueError(
            f"host exit code {exit_code} contradicts status {status}; expected {expected_exit}"
        )
    return status == "PASS", "; ".join(failures)


def parse_identity_envelope(
    *,
    content: str,
    exit_code: int,
    run_id: str,
    revision: str,
    build_mode: str,
    optimization: str,
    case_id: str,
    expected_tools: Sequence[tuple[str, str, str]],
) -> tuple[str, str]:
    """Validate one canonical focused report produced by ``run_suite.py``."""
    payload = json.loads(content, object_pairs_hook=unique_json_object)
    if not isinstance(payload, dict):
        raise ValueError("identity report must be a JSON object")
    if type(payload.get("schema")) is not int or payload.get("schema") != 1:
        raise ValueError("identity report schema must be 1")

    run = payload.get("run")
    if not isinstance(run, dict):
        raise ValueError("identity report run must be a JSON object")
    expected_identity = (run_id, revision, build_mode, optimization)
    actual_identity = (
        run.get("run_id"),
        run.get("revision"),
        run.get("build_mode"),
        run.get("optimization"),
    )
    if actual_identity != expected_identity:
        raise ValueError("identity report run identity mismatch")
    if (
        run.get("profile") != "explicit"
        or run.get("scope") != "focused"
        or type(run.get("selected")) is not int
        or run.get("selected") != 1
    ):
        raise ValueError("identity report is not a focused single-case run")
    tools = run.get("tools")
    if not isinstance(tools, list):
        raise ValueError("identity report tools must be a list")
    actual_tools: list[tuple[str, str, str]] = []
    for tool in tools:
        if not isinstance(tool, dict) or set(tool) != {"name", "path", "sha256"}:
            raise ValueError("identity report tool receipt schema is invalid")
        identity = (tool.get("name"), tool.get("path"), tool.get("sha256"))
        if any(not isinstance(value, str) or not value for value in identity):
            raise ValueError("identity report tool receipt fields must be non-empty strings")
        actual_tools.append(identity)
    if len(actual_tools) != len(set(actual_tools)):
        raise ValueError("identity report contains duplicate tool receipts")
    if sorted(actual_tools) != sorted(expected_tools):
        raise ValueError("identity report tool receipts do not match selected tools")

    results = payload.get("results")
    if not isinstance(results, list) or len(results) != 1:
        raise ValueError("identity report must contain exactly one result")
    result = results[0]
    if not isinstance(result, dict):
        raise ValueError("identity result must be a JSON object")
    if result.get("case_id") != case_id:
        raise ValueError("identity report case identity mismatch")

    status = result.get("status")
    if not isinstance(status, str) or status not in IDENTITY_RESULT_STATUSES:
        raise ValueError(f"identity report has unknown status: {status!r}")
    duration = result.get("duration_seconds")
    if (
        isinstance(duration, bool)
        or not isinstance(duration, (int, float))
        or not math.isfinite(duration)
        or duration < 0
    ):
        raise ValueError("identity result duration_seconds must be non-negative")
    failures = result.get("failures")
    if not isinstance(failures, list) or any(
        not isinstance(item, str) or not item for item in failures
    ):
        raise ValueError("identity result failures must be a list of strings")
    blocked_by = result.get("blocked_by")
    if not isinstance(blocked_by, list) or any(
        not isinstance(item, str) or not item for item in blocked_by
    ):
        raise ValueError("identity result blocked_by must be a list of strings")

    if status == "PASS" and (failures or blocked_by):
        raise ValueError("PASS cannot contain failures or blockers")
    if status == "BLOCKED" and not blocked_by:
        raise ValueError("BLOCKED requires blocked_by evidence")
    if status != "BLOCKED" and blocked_by:
        raise ValueError("only BLOCKED can contain blocked_by evidence")
    if status in {
        "FAIL",
        "KNOWN_FAILURE",
        "UNEXPECTED_PASS",
        "TIMEOUT",
        "INFRASTRUCTURE_ERROR",
    } and not failures:
        raise ValueError(f"{status} requires failure evidence")

    counts = payload.get("counts")
    expected_counts = {name: 0 for name in IDENTITY_COUNT_STATUSES}
    expected_counts[status] = 1
    if (
        not isinstance(counts, dict)
        or any(
            not isinstance(name, str)
            or isinstance(count, bool)
            or not isinstance(count, int)
            or count < 0
            for name, count in counts.items()
        )
        or counts != expected_counts
    ):
        raise ValueError("identity report counts do not match its result")

    expected_exit = 0 if status in {"PASS", "KNOWN_FAILURE"} else 1
    if exit_code != expected_exit:
        raise ValueError(
            f"identity controller exit code {exit_code} contradicts status {status}; "
            f"expected {expected_exit}"
        )
    return status, "; ".join(failures)


def parse_python_gate_result(value: Any) -> tuple[bool, str]:
    """Accept only the exact result protocol declared by imported Python gates."""
    if type(value) is not tuple or len(value) != 2:
        raise ValueError("Python gate must return a two-item tuple")
    satisfied, detail = value
    if type(satisfied) is not bool:
        raise ValueError("Python gate satisfaction must be a boolean")
    if not isinstance(detail, str):
        raise ValueError("Python gate detail must be a string")
    if not satisfied and not detail:
        raise ValueError("failed Python gate must provide non-empty detail")
    return satisfied, detail


def parse_selfhost_oracle(value: Any) -> tuple[int, dict[str, Any] | None]:
    """Validate the closed oracle schema used by executable self-host cases."""
    if not isinstance(value, dict):
        raise ValueError("self-host oracle must be a JSON object")
    unknown = sorted(set(value) - {"exit_code", "description", "json"})
    if unknown:
        raise ValueError("self-host oracle contains unknown fields: " + ", ".join(unknown))
    exit_code = value.get("exit_code")
    if isinstance(exit_code, bool) or not isinstance(exit_code, int):
        raise ValueError("self-host oracle exit_code must be an integer")
    description = value.get("description")
    if description is not None and not isinstance(description, str):
        raise ValueError("self-host oracle description must be a string")
    expected_json = value.get("json")
    if "json" in value and not isinstance(expected_json, dict):
        raise ValueError("self-host oracle json must be an object")
    return exit_code, expected_json


def equal_json_exact(expected: Any, actual: Any) -> bool:
    """Compare JSON values iteratively without Python scalar coercions."""
    pending: list[tuple[Any, Any]] = [(expected, actual)]
    while pending:
        expected_value, actual_value = pending.pop()
        if type(expected_value) is not type(actual_value):
            return False
        if isinstance(expected_value, dict):
            if expected_value.keys() != actual_value.keys():
                return False
            pending.extend(
                (expected_value[key], actual_value[key]) for key in expected_value
            )
            continue
        if isinstance(expected_value, list):
            if len(expected_value) != len(actual_value):
                return False
            pending.extend(zip(expected_value, actual_value, strict=True))
            continue
        if expected_value != actual_value:
            return False
    return True


def parse_selfhost_json_output(content: str) -> Any:
    """Parse fixture JSON without allowing duplicate object definitions."""
    return json.loads(content, object_pairs_hook=unique_json_object)


def parse_selfhost_inputs(
    value: Any, suite_root: Path, expected_case_id: str
) -> tuple[str, str, bool]:
    """Validate the closed producer and invocation contract of self-host cases."""
    if not isinstance(value, dict):
        raise ValueError("self-host inputs must be a JSON object")
    unknown = sorted(
        set(value) - {"fixture_project", "case_argument", "sandbox_argument"}
    )
    if unknown:
        raise ValueError("self-host inputs contain unknown fields: " + ", ".join(unknown))

    fixture = value.get("fixture_project")
    if not isinstance(fixture, str) or not fixture:
        raise ValueError("self-host fixture_project must be non-empty text")
    relative = Path(fixture)
    root = suite_root.resolve()
    resolved = (root / relative).resolve()
    if (
        relative.is_absolute()
        or root not in resolved.parents
        or resolved.suffix.casefold() != ".nsproj"
        or not resolved.is_file()
    ):
        raise ValueError("self-host fixture_project must be a local existing .nsproj")

    argument = value.get("case_argument")
    if not isinstance(argument, str) or not argument:
        raise ValueError("self-host case_argument must be non-empty text")
    if argument != expected_case_id:
        raise ValueError("self-host case_argument does not match selected case")
    sandbox = value.get("sandbox_argument")
    if sandbox is not None and sandbox != "fresh-case-directory":
        raise ValueError(
            "self-host sandbox_argument must be absent or fresh-case-directory"
        )
    return fixture, argument, sandbox == "fresh-case-directory"


class ExecutionEngine:
    """Execute concrete variants while preserving producer/consumer boundaries."""

    def __init__(
        self,
        config: BuilderConfig,
        store: EvidenceStore,
        output: Callable[[str], None],
        status: Callable[[str, str, str, float, str], None],
        progress: Callable[[int, int], None],
        timeout_budgets: dict[
            tuple[str, str, str, str], VariantTimeoutBudget
        ] | None = None,
        preparation: Callable[[str, int, int, str], None] | None = None,
    ) -> None:
        self.config = config
        self.store = store
        self.output = output
        self.status = status
        self.progress = progress
        self.process = ProcessController(self._emit_output, self._emit_activity)
        self.timeout_budgets = dict(timeout_budgets or {})
        self.preparation = preparation or (lambda _phase, _done, _total, _detail: None)
        self.fixture_cache: dict[tuple[str, str, str], Path] = {}
        self.fixture_failures: dict[tuple[str, str, str], tuple[str, str]] = {}
        self.module_cache: dict[str, Any] = {}
        self.active_key = ""
        self.active_phase = ""
        self.active_detail = ""

    def run(self, variants: Sequence[RunVariant]) -> tuple[list[CaseOutcome], bool]:
        """Run every planned variant in deterministic dependency-oriented order."""
        ordered = sorted(
            variants,
            key=lambda item: (
                item.case.wave,
                item.case.component,
                item.case.contract_id,
                item.case.case_id,
                item.build_mode,
                item.optimization,
            ),
        )
        outcomes: list[CaseOutcome] = []
        total = len(ordered)
        preparations = self._prepare_producers(ordered)
        blocked_by_key = {
            consumer: result
            for result in preparations
            if result.status != "PASS"
            for consumer in result.consumer_keys
        }
        for index, variant in enumerate(ordered, start=1):
            if self.process.cancelled.is_set():
                outcomes.extend(self._cancelled_outcomes(ordered[index - 1 :]))
                break
            preparation = blocked_by_key.get(variant.key)
            if preparation is not None:
                outcome = self._producer_blocked_outcome(variant, preparation)
            else:
                self.store.event("CASE_STARTED", self._variant_payload(variant))
                outcome = self._run_controlled_variant(variant)
            outcomes.append(outcome)
            self.status(
                variant.key,
                outcome.status,
                outcome.detail,
                outcome.duration_seconds,
                outcome.evidence,
            )
            self.store.event("CASE_FINISHED", asdict(outcome))
            self.progress(index, total)
            self.active_key = ""
            self.active_phase = ""
            self.active_detail = ""
        return outcomes, self.process.cancelled.is_set()

    def _prepare_producers(
        self, variants: Sequence[RunVariant]
    ) -> tuple[ProducerPreparation, ...]:
        """Build each selected producer once and validate its concrete artifact."""
        plans = plan_test_producers(self.config, variants)
        if not plans:
            return ()
        total = len(plans)
        self.preparation(
            "Preparar pruebas",
            0,
            total,
            ", ".join(plan.key for plan in plans),
        )
        failures: dict[str, tuple[str, str]] = {}
        try:
            commands = producer_preparation_commands(self.config, plans)
        except Exception as error:
            detail = f"{type(error).__name__}: {error}"
            commands = []
            for plan in plans:
                failures[plan.key] = (detail, "")
        preparation_root = self.store.root / "preparation"
        preparation_root.mkdir(parents=True, exist_ok=True)
        for command_index, (label, command, cwd, producer_keys) in enumerate(
            commands, start=1
        ):
            if all(key in failures for key in producer_keys):
                continue
            if self.process.cancelled.is_set():
                break
            self.preparation("Preparar pruebas", 0, total, label)
            self.store.event(
                "PRODUCER_COMMAND_STARTED",
                {"label": label, "producer_keys": list(producer_keys), "command": command},
            )
            code, captured = self.process.run(command, cwd)
            log_path = preparation_root / f"command-{command_index:02d}.log"
            write_text_atomic(log_path, captured)
            self.store.event(
                "PRODUCER_COMMAND_FINISHED",
                {
                    "label": label,
                    "producer_keys": list(producer_keys),
                    "exit_code": code,
                    "evidence": str(log_path),
                },
            )
            if code != 0:
                detail = infrastructure_failure_detail(code, captured)
                for key in producer_keys:
                    failures.setdefault(key, (detail, str(log_path)))
        results: list[ProducerPreparation] = []
        for index, plan in enumerate(plans, start=1):
            executable = component_test_executable(
                self.config, plan.build_mode, plan.producer.target
            )
            if self.process.cancelled.is_set():
                detail = "preparación cancelada por el usuario"
                status = "CANCELLED"
                evidence = ""
            elif plan.key in failures:
                detail, evidence = failures[plan.key]
                status = "FAIL"
            elif not executable.is_file() or executable.stat().st_size <= 0:
                detail = f"el productor terminó sin ejecutable válido: {executable}"
                evidence = ""
                status = "FAIL"
            else:
                stat = executable.stat()
                detail = (
                    "grafo incremental verificado; "
                    f"ejecutable {stat.st_size} bytes, mtime_ns={stat.st_mtime_ns}"
                )
                evidence = str(executable)
                status = "PASS"
            receipt_path = preparation_root / f"{plan.producer.key}-{plan.build_mode}.json"
            payload = {
                "schema": 1,
                "producer": plan.producer.key,
                "target": plan.producer.target,
                "build_mode": plan.build_mode,
                "status": status,
                "detail": detail,
                "executable": str(executable),
                "consumers": list(plan.consumer_keys),
                "command_evidence": evidence,
            }
            write_json_atomic(receipt_path, payload)
            result = ProducerPreparation(
                key=plan.key,
                status=status,
                detail=detail,
                executable=executable,
                consumer_keys=plan.consumer_keys,
                evidence=str(receipt_path),
            )
            results.append(result)
            self.store.event("PRODUCER_FINISHED", payload | {"evidence": str(receipt_path)})
            self.preparation("Preparar pruebas", index, total, f"{plan.key}: {status}")
        return tuple(results)

    @staticmethod
    def _producer_blocked_outcome(
        variant: RunVariant, preparation: ProducerPreparation
    ) -> CaseOutcome:
        """Close one consumer without dispatch after its producer failed."""
        return CaseOutcome(
            key=variant.key,
            case_id=variant.case.case_id,
            contract_id=variant.case.contract_id,
            component=variant.case.component,
            wave=variant.case.wave,
            build_mode=variant.build_mode,
            optimization=variant.optimization,
            status="NOT_RUN",
            detail=f"BLOQUEADO_POR productor {preparation.key}: {preparation.detail}",
            evidence=preparation.evidence,
            profile=variant.case.profile,
        )

    def cancel(self) -> None:
        """Request cancellation of the active child and all remaining variants."""
        self.process.cancel()

    def cancel_current(self, key: str) -> bool:
        """Cancel exactly the active row without setting the run-wide event."""
        if not key or key != self.active_key:
            return False
        return self.process.cancel_current("case-cancelled")

    def _run_controlled_variant(self, variant: RunVariant) -> CaseOutcome:
        """Apply one isolated timeout/cancellation scope around a variant dispatch."""
        budget = self.timeout_budgets.get(
            variant_history_key(variant),
            VariantTimeoutBudget(0, None, timeout_limit_for_average(None)),
        )
        generation = self.process.begin_variant()
        started = time.monotonic()
        average_detail = (
            f"promedio PASS {budget.average_seconds:.3f} s"
            if budget.average_seconds is not None
            else "sin PASS histórico"
        )
        self._set_active_phase(
            variant,
            "RUNNING",
            f"iniciando variante; límite {budget.limit_seconds} s; {average_detail}",
        )
        heartbeat_stop = threading.Event()

        def heartbeat() -> None:
            """Keep elapsed duration visible even while no child emits output."""
            while not heartbeat_stop.wait(0.25):
                self._emit_activity(time.monotonic() - started, "")

        heartbeat_thread = threading.Thread(
            target=heartbeat,
            name="nscbuilder3-variant-heartbeat",
            daemon=True,
        )
        timeout_timer = threading.Timer(
            budget.limit_seconds,
            lambda: self.process.cancel_current("timeout", generation),
        )
        timeout_timer.daemon = True
        heartbeat_thread.start()
        timeout_timer.start()
        try:
            outcome = self._run_variant(variant)
        finally:
            timeout_timer.cancel()
            heartbeat_stop.set()
            heartbeat_thread.join(timeout=1.0)
        elapsed = time.monotonic() - started
        stop_reason = self.process.end_variant(generation)
        outcome.duration_seconds = elapsed
        outcome.profile = variant.case.profile
        outcome.timeout_pass_count = budget.pass_count
        outcome.timeout_average_seconds = budget.average_seconds
        outcome.timeout_limit_seconds = budget.limit_seconds
        if self.process.cancelled.is_set():
            outcome.status = "CANCELLED"
            outcome.detail = "corrida cancelada por el usuario"
        elif stop_reason == "case-cancelled":
            outcome.status = "CANCELLED"
            outcome.detail = "prueba cancelada por el usuario; la corrida continúa"
            outcome.evidence = self._write_variant_stop_evidence(
                variant, outcome, budget, "CANCELLED", outcome.evidence
            )
        elif stop_reason == "timeout":
            outcome.status = "TIMEOUT"
            average = (
                f"{budget.average_seconds:.3f} s en {budget.pass_count} PASS"
                if budget.average_seconds is not None
                else "sin PASS histórico confiable"
            )
            outcome.detail = (
                f"tiempo límite individual agotado a {budget.limit_seconds} s; "
                f"{average}; duración real {elapsed:.3f} s"
            )
            outcome.evidence = self._write_variant_stop_evidence(
                variant, outcome, budget, "TIMEOUT", outcome.evidence
            )
        return outcome

    def _write_variant_stop_evidence(
        self,
        variant: RunVariant,
        outcome: CaseOutcome,
        budget: VariantTimeoutBudget,
        status: str,
        process_evidence: str,
    ) -> str:
        """Persist the causal timing record for a manual cancel or timeout."""
        case_root = self._case_root(variant)
        path = case_root / ("timeout.json" if status == "TIMEOUT" else "cancellation.json")
        payload = {
            "schema": 1,
            "timestamp": utc_now(),
            "key": variant.key,
            "status": status,
            "profile": variant.case.profile,
            "historical_pass_count": budget.pass_count,
            "historical_pass_average_seconds": budget.average_seconds,
            "timeout_limit_seconds": budget.limit_seconds,
            "elapsed_seconds": outcome.duration_seconds,
            "process_evidence": process_evidence,
        }
        write_json_atomic(path, payload)
        self.store.event(f"CASE_{status}", payload)
        self.store.log(
            f"[{status}] {variant.key}: {outcome.duration_seconds:.3f} s; "
            f"límite {budget.limit_seconds} s\n"
        )
        return str(path)

    def _run_variant(self, variant: RunVariant) -> CaseOutcome:
        """Dispatch one variant by its manifest-owned executor contract."""
        started = time.monotonic()
        self.process.last_termination_reason = None
        try:
            if variant.case.executor in {
                "nsc_component_tests",
                "nsc_component_tests_destructive",
                "nsc_component_tests_source_extended",
            }:
                status, detail, evidence = self._run_host(variant)
            elif variant.case.executor != "nsc_suite_controller+nsc":
                raise ValueError(
                    f"unsupported executor in NSC Builder 3: {variant.case.executor}"
                )
            elif variant.case.component.startswith("SelfHost."):
                status, detail, evidence = self._run_selfhost(variant)
            elif variant.case.component == "BuildSystem":
                status, detail, evidence = self._run_buildsystem(variant)
            elif variant.case.component == "Packaging":
                status, detail, evidence = self._run_packaging(variant)
            else:
                status, detail, evidence = self._run_identity(variant)
        except Exception as error:
            status = "INFRASTRUCTURE_ERROR"
            detail = f"{type(error).__name__}: {error}"
            evidence = ""
        duration = time.monotonic() - started
        if self.process.cancelled.is_set():
            status = "CANCELLED"
            detail = "cancelled by user"
        return CaseOutcome(
            key=variant.key,
            case_id=variant.case.case_id,
            contract_id=variant.case.contract_id,
            component=variant.case.component,
            wave=variant.case.wave,
            build_mode=variant.build_mode,
            optimization=variant.optimization,
            status=status,
            duration_seconds=duration,
            detail=detail,
            evidence=evidence,
        )

    def _run_host(self, variant: RunVariant) -> tuple[str, str, str]:
        """Run one consolidated C++ host case and validate its JSON envelope."""
        executable = component_test_executable(
            self.config, variant.build_mode, variant.case.executor
        )
        if not executable.is_file():
            return "INFRASTRUCTURE_ERROR", f"host executable does not exist: {executable}", ""
        case_root = self._case_root(variant)
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando {variant.case.executor} --case {variant.case.case_id}",
        )
        code, captured = self.process.run(
            [str(executable), "--case", variant.case.case_id],
            case_root,
        )
        log_path = case_root / "host.log"
        log_path.write_text(captured, encoding="utf-8", newline="\n")
        try:
            passed, detail = parse_host_envelope(
                captured=captured,
                exit_code=code,
                case_id=variant.case.case_id,
                contract_id=variant.case.contract_id,
                component=variant.case.component,
            )
        except (json.JSONDecodeError, ValueError) as error:
            try:
                abnormal_detail = expected_abnormal_termination_detail(
                    variant.case, code, captured
                )
            except ValueError as contract_error:
                return (
                    "INFRASTRUCTURE_ERROR",
                    f"{error}; {contract_error}",
                    str(log_path),
                )
            if abnormal_detail is not None:
                return self._expected_status(
                    variant.case, False, abnormal_detail, str(log_path)
                )
            return (
                "INFRASTRUCTURE_ERROR",
                f"{error}; host exit code {code}",
                str(log_path),
            )
        return self._expected_status(variant.case, passed, detail, str(log_path))

    def _run_identity(self, variant: RunVariant) -> tuple[str, str, str]:
        """Delegate one compiler-identity case to the existing manifest controller."""
        case_root = self._case_root(variant)
        run_id = variant.key.replace("|", "-")
        revision = current_revision()
        nsc = Path(self.config.nsc_executable).resolve()
        expected_tools = [("nsc", str(nsc), sha256_file(nsc))]
        command = [
            sys.executable,
            str(RUN_SUITE_PATH),
            "--manifest",
            str(MANIFEST_PATH),
            "--components",
            str(COMPONENTS_PATH),
            "--contracts",
            str(CONTRACTS_PATH),
            "--nsc-executable",
            str(nsc),
            "--work-root",
            str(case_root),
            "--run-id",
            run_id,
            "--revision",
            revision,
            "--build-mode",
            variant.build_mode,
            "--optimization",
            variant.optimization,
            "--case",
            variant.case.case_id,
        ]
        if self.config.comparison_nsc:
            comparison = Path(self.config.comparison_nsc).resolve()
            command.extend(["--comparison-nsc-executable", str(comparison)])
            expected_tools.append(
                ("comparison-nsc", str(comparison), sha256_file(comparison))
            )
        if self.config.build_id_header:
            header = Path(self.config.build_id_header).resolve()
            command.extend(["--build-id-header", str(header)])
            expected_tools.append(
                ("build-id-header", str(header), sha256_file(header))
            )
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando controlador de identidad {variant.case.case_id}",
        )
        # This command runs the compiler-identity test controller; it does not build NSC.
        code, captured = self.process.run(command, SOURCE_ROOT)
        log_path = case_root / "controller.log"
        log_path.write_text(captured, encoding="utf-8", newline="\n")
        result_path = case_root / run_id / "results.json"
        if not result_path.is_file():
            return "INFRASTRUCTURE_ERROR", f"controller exited with {code} without results", str(log_path)
        try:
            status, detail = parse_identity_envelope(
                content=result_path.read_text(encoding="utf-8"),
                exit_code=code,
                run_id=run_id,
                revision=revision,
                build_mode=variant.build_mode,
                optimization=variant.optimization,
                case_id=variant.case.case_id,
                expected_tools=expected_tools,
            )
        except (OSError, json.JSONDecodeError, TypeError, ValueError) as error:
            return "INFRASTRUCTURE_ERROR", str(error), str(result_path)
        return status, detail, str(result_path)

    def _run_selfhost(self, variant: RunVariant) -> tuple[str, str, str]:
        """Compile a real NativeSharp fixture once, then run one selected case."""
        manifest = variant.case.manifest or {}
        try:
            project_relative, case_argument, use_sandbox = parse_selfhost_inputs(
                manifest.get("inputs"), SUITE_ROOT, variant.case.case_id
            )
            expected, expected_json = parse_selfhost_oracle(manifest.get("oracle"))
        except ValueError as error:
            return "INFRASTRUCTURE_ERROR", str(error), ""
        fixture_key = (project_relative, variant.build_mode, variant.optimization)
        producer_failure = self.fixture_failures.get(fixture_key)
        if producer_failure is not None:
            detail, evidence = producer_failure
            return "BLOCKED", f"BLOQUEADO_POR fixture {project_relative}: {detail}", evidence
        try:
            executable = self._prepare_selfhost_fixture(variant, project_relative)
        except RuntimeError as error:
            project = SUITE_ROOT / project_relative
            fixture_root = self.store.root / "fixtures" / (
                f"{project.stem}-{variant.build_mode.casefold()}-{variant.optimization.casefold()}"
            )
            evidence = str(fixture_root / "compile.log")
            detail = str(error)
            self.fixture_failures[fixture_key] = (detail, evidence)
            return "INFRASTRUCTURE_ERROR", detail, evidence
        case_root = self._case_root(variant)
        if variant.case.component == "SelfHost.Source":
            self._prepare_source_files(case_root)
        command = [str(executable), case_argument]
        if use_sandbox:
            command.append(str(case_root))
        environment = self._nsc_environment()
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando fixture {Path(project_relative).name}: {case_argument}",
        )
        code, captured = self.process.run(
            command,
            case_root,
            environment,
        )
        log_path = case_root / "fixture.log"
        log_path.write_text(captured, encoding="utf-8", newline="\n")
        passed = code == expected
        detail = "" if passed else f"exit code {code}; expected {expected}"
        if passed and expected_json is not None:
            try:
                passed = equal_json_exact(
                    expected_json,
                    parse_selfhost_json_output(captured.strip()),
                )
            except json.JSONDecodeError as error:
                passed = False
                detail = f"invalid JSON output: {error.msg}"
            if not passed and not detail:
                detail = "JSON output differs from oracle"
        return self._expected_status(variant.case, passed, detail, str(log_path))

    def _prepare_selfhost_fixture(self, variant: RunVariant, relative: str) -> Path:
        """Build one fixture producer per project, mode and optimization."""
        key = (relative, variant.build_mode, variant.optimization)
        cached = self.fixture_cache.get(key)
        if cached is not None and cached.is_file():
            return cached
        source_project = SUITE_ROOT / relative
        fixture_root = self.store.root / "fixtures" / (
            f"{source_project.stem}-{variant.build_mode.casefold()}-{variant.optimization.casefold()}"
        )
        fixture_root.mkdir(parents=True, exist_ok=True)
        inventory_module = self._load_gate("selfhost_inventory.py")
        materializer = getattr(inventory_module, "materialize_contained_project", None)
        if not callable(materializer):
            raise RuntimeError("self-host inventory does not expose project materialization")
        project = Path(
            materializer(
                SOURCE_ROOT,
                source_project,
                fixture_root / "project",
            )
        )
        executable = fixture_root / f"{source_project.stem}.exe"
        command = [
            str(Path(self.config.nsc_executable).resolve()),
            "--emit-project-oop-exe",
            str(project),
            f"-{variant.optimization}",
            f"--profile={variant.build_mode.casefold()}",
            "-g0",
            "--library-reference-root",
            str((Path(self.config.install_prefix).resolve() / "lib")),
            "-o",
            str(executable),
        ]
        self._set_active_phase(
            variant,
            "COMPILING",
            f"compilando fixture {project.name}",
            str(fixture_root),
        )
        write_json(fixture_root / "compile.command.json", command)
        code, captured = self.process.run(command, fixture_root, self._nsc_environment())
        (fixture_root / "compile.log").write_text(captured, encoding="utf-8", newline="\n")
        if self.process.last_termination_reason == "cancelled":
            raise RuntimeError(f"fixture {source_project.name} compilation was cancelled")
        if code != 0:
            raise RuntimeError(f"fixture {source_project.name} compiler exited with code {code}")
        if not executable.is_file():
            raise RuntimeError(
                f"fixture {source_project.name} completed without executable output"
            )
        self.fixture_cache[key] = executable
        return executable

    def _run_buildsystem(self, variant: RunVariant) -> tuple[str, str, str]:
        """Execute one BuildSystem row through its existing case-level oracle."""
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando gate BuildSystem {variant.case.case_id}",
        )
        module = self._load_gate("run_buildsystem_gate.py")
        tools = discover_build_tools(str(self.config.llvm_root_path()))
        cmake = Path(tools["cmake"]).resolve()
        installation = Path(self.config.install_prefix).resolve()
        arguments = argparse.Namespace(
            cmake_executable=cmake,
            source_root=SUITE_ROOT,
            repository_root=SOURCE_ROOT,
            installation_root=installation,
            work_parent=self.store.root,
            build_mode=variant.build_mode,
            root_config_argument=[
                "-G",
                "Ninja",
                f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).resolve().as_posix()}",
                f"-DCMAKE_BUILD_TYPE={variant.build_mode}",
                f"-DCMAKE_C_COMPILER={Path(tools['clang_cl']).resolve().as_posix()}",
                f"-DCMAKE_CXX_COMPILER={Path(tools['clang_cl']).resolve().as_posix()}",
                f"-DCMAKE_LINKER={Path(tools['lld_link']).resolve().as_posix()}",
                f"-DCMAKE_MT={Path(tools['llvm_mt']).resolve().as_posix()}",
                f"-DCMAKE_RC_COMPILER={Path(tools['llvm_rc']).resolve().as_posix()}",
                f"-DLLVM_DIR={Path(tools['llvm_dir']).resolve().as_posix()}",
                f"-DNSC_NSLINK_EXECUTABLE="
                f"{Path(self.config.nslink_executable).resolve().as_posix()}",
            ],
        )
        return self._run_python_gate_case(module, variant, arguments)

    def _run_packaging(self, variant: RunVariant) -> tuple[str, str, str]:
        """Execute one Packaging row through its existing case-level oracle."""
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando gate Packaging {variant.case.case_id}",
        )
        archive = Path(self.config.archive_executable).resolve()
        if not archive.is_file():
            return "INFRASTRUCTURE_ERROR", "Packaging requires an archive executable", ""
        module = self._load_gate("run_packaging_gate.py")
        arguments = argparse.Namespace(
            python_executable=Path(sys.executable).resolve(),
            archive_executable=archive,
            source_root=SUITE_ROOT,
            repository_root=SOURCE_ROOT,
            installation_root=Path(self.config.nsc_executable).resolve().parent.parent,
            work_parent=self.store.root,
            build_mode=variant.build_mode,
        )
        return self._run_python_gate_case(module, variant, arguments)

    def _run_python_gate_case(
        self, module: Any, variant: RunVariant, options: argparse.Namespace
    ) -> tuple[str, str, str]:
        """Run one already-defined Python oracle while retaining per-case evidence."""
        case_root = self._case_root(variant)
        manifest = variant.case.manifest or {}
        original_runner = getattr(module, "run_process", None)
        if not callable(original_runner):
            return (
                "INFRASTRUCTURE_ERROR",
                "Python gate does not expose a callable run_process boundary",
                str(case_root),
            )

        def controlled_runner(
            command: Sequence[str],
            cwd: Path,
            environment: dict[str, str],
        ) -> subprocess.CompletedProcess[str]:
            controlled_command, controlled_environment = isolate_missing_llvm_discovery(
                variant.case.case_id, command, environment
            )
            return self.process.run_completed(
                controlled_command, cwd, controlled_environment
            )

        raw_result: Any = None
        gate_error = ""
        module.run_process = controlled_runner
        try:
            raw_result = module.run_case(manifest, options, case_root, dict(os.environ))
        except Exception as error:
            gate_error = f"{type(error).__name__}: {error}"
        finally:
            module.run_process = original_runner
        self._emit_case_logs(case_root)
        if gate_error:
            return (
                "INFRASTRUCTURE_ERROR",
                gate_error,
                str(case_root),
            )
        evidence = str(case_root)
        try:
            satisfied, detail = parse_python_gate_result(raw_result)
        except ValueError as error:
            return "INFRASTRUCTURE_ERROR", str(error), evidence
        return self._expected_status(variant.case, satisfied, detail, evidence)

    def _emit_case_logs(self, case_root: Path) -> None:
        """Expose logs captured by imported gates in the unified progressive console."""
        for path in sorted(case_root.rglob("*.log")):
            try:
                content = path.read_text(encoding="utf-8", errors="replace")
            except OSError as error:
                self._emit_output(f"[{path.name}] unreadable: {error}\n")
                continue
            self._emit_output(f"\n===== {path.relative_to(case_root)} =====\n")
            self._emit_output(content)

    def _load_gate(self, name: str) -> Any:
        """Load one existing gate by path without adding new Builder modules."""
        cached = self.module_cache.get(name)
        if cached is not None:
            return cached
        path = SUITE_ROOT / "controller" / "integration" / name
        # Integration gates intentionally share ``component_gate_support`` as a
        # sibling module, and that support module imports the public ``nsc_suite``
        # package from the controller root. Loading a gate from an absolute file
        # path exposes neither location, so publish both canonical roots once
        # before executing any gate. The entries remain stable for lazy imports.
        controller_root = str(SUITE_ROOT / "controller")
        integration_root = str(path.parent)
        for import_root in (controller_root, integration_root):
            if import_root not in sys.path:
                sys.path.insert(0, import_root)
        module_name = f"nscbuilder3_{path.stem}"
        spec = importlib.util.spec_from_file_location(module_name, path)
        if spec is None or spec.loader is None:
            raise RuntimeError(f"cannot load gate: {path}")
        module = importlib.util.module_from_spec(spec)
        # ``dataclasses`` and other runtime introspection resolve annotations
        # through ``sys.modules`` while the module body is executing.  A module
        # loaded directly from a file therefore must be published before
        # ``exec_module`` and removed again if initialization fails.
        sys.modules[module_name] = module
        try:
            spec.loader.exec_module(module)
        except Exception:
            sys.modules.pop(module_name, None)
            raise
        self.module_cache[name] = module
        return module

    def _expected_status(
        self, case: CaseRecord, passed: bool, detail: str, evidence: str
    ) -> tuple[str, str, str]:
        """Preserve known defects as visible red outcomes rather than skips."""
        if case.expected_outcome == "KNOWN_FAILURE" or case.disposition == "KnownProductDefect":
            status = "UNEXPECTED_PASS" if passed else "KNOWN_FAILURE"
        else:
            status = "PASS" if passed else "FAIL"
        return status, detail, evidence

    def _case_root(self, variant: RunVariant) -> Path:
        """Create an isolated evidence directory for one concrete variant."""
        root = self.store.root / "cases" / variant.key.replace("|", "-")
        root.mkdir(parents=True, exist_ok=True)
        return root

    def _nsc_environment(self) -> dict[str, str]:
        """Build the explicit NSLink environment used by NativeSharp products."""
        nslink = Path(self.config.nslink_executable).resolve()
        environment = dict(os.environ)
        environment["NSC_LINKER"] = str(nslink)
        environment["PATH"] = str(nslink.parent) + os.pathsep + environment.get("PATH", "")
        return environment

    @staticmethod
    def _prepare_source_files(case_root: Path) -> None:
        """Create the exact isolated source corpus expected by SelfHost.Source."""
        (case_root / "source-empty.ns").write_bytes(b"")
        (case_root / "source-input.ns").write_bytes(b"alpha\r\nbeta\rgamma\n")
        (case_root / "source-unicode.ns").write_bytes("á\r\nβ".encode("utf-8"))
        (case_root / "source-frontend.ns").write_bytes(b"comptime int value = #run(2 + 3);\n")
        (case_root / "source-large.ns").write_bytes(b"x\r\n" * 32768)

    @staticmethod
    def _variant_payload(variant: RunVariant) -> dict[str, Any]:
        """Serialize one variant identity for progressive events."""
        return {
            "key": variant.key,
            "case_id": variant.case.case_id,
            "contract_id": variant.case.contract_id,
            "component": variant.case.component,
            "wave": variant.case.wave,
            "build_mode": variant.build_mode,
            "optimization": variant.optimization,
            "profile": variant.case.profile,
        }

    @staticmethod
    def _cancelled_outcomes(variants: Sequence[RunVariant]) -> list[CaseOutcome]:
        """Materialize every unstarted variant as cancelled instead of dropping it."""
        return [
            CaseOutcome(
                key=item.key,
                case_id=item.case.case_id,
                contract_id=item.case.contract_id,
                component=item.case.component,
                wave=item.case.wave,
                build_mode=item.build_mode,
                optimization=item.optimization,
                status="CANCELLED",
                detail="not started because the run was cancelled",
                profile=item.case.profile,
            )
            for item in variants
        ]

    def _emit_output(self, line: str) -> None:
        """Mirror subprocess output to both the GUI and progressive evidence."""
        self.store.log(line)
        self.output(line)

    def _set_active_phase(
        self,
        variant: RunVariant,
        phase: str,
        detail: str,
        evidence: str = "",
    ) -> None:
        """Publish the active row and retain its phase for process heartbeats."""
        self.active_key = variant.key
        self.active_phase = phase
        self.active_detail = detail
        self.status(variant.key, phase, detail, 0.0, evidence)

    def _emit_activity(self, elapsed: float, detail: str) -> None:
        """Refresh elapsed time and last child output for the active exact row."""
        if not self.active_key:
            return
        visible_detail = detail[-300:] if detail else self.active_detail
        self.status(
            self.active_key,
            self.active_phase,
            visible_detail,
            elapsed,
            "",
        )
