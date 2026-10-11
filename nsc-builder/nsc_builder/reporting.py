"""Extracted reporting implementation for modular NSC Builder 3."""

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


def case_diagnostic_summary(
    case: CaseRecord,
    variant: RunVariant | None = None,
    outcome: CaseOutcome | None = None,
) -> str:
    """Render the stable test contract and its observed result for a human reader."""
    manifest = case.manifest or {}
    dependencies = manifest.get("dependencies", [])
    tags = manifest.get("tags", [])
    lines = [
        "RESUMEN DE LA PRUEBA",
        "=" * 80,
        f"Caso              : {case.case_id}",
        f"Contrato          : {case.contract_id}",
        f"Componente        : {case.component}",
        f"Ola               : {case.wave}",
        f"Objetivo          : {case.objective}",
        f"Propósito         : {case.purpose}",
        f"Capa              : {case.layer}",
        f"Clase de escenario: {manifest.get('class', 'no declarada')}",
        f"Etiquetas         : {', '.join(str(tag) for tag in tags) or 'ninguna'}",
        f"Dependencias      : {', '.join(str(item) for item in dependencies) or 'ninguna'}",
        f"Aislamiento       : {manifest.get('isolation', 'no declarado')}",
        f"Ejecutor          : {case.executor}",
        f"Perfil            : {case.profile}",
        f"Disposición       : {case.disposition}",
        f"Resultado esperado: {case.expected_outcome or 'definido por el oráculo'}",
    ]
    if variant is not None:
        lines.extend(
            (
                "",
                "CONFIGURACIÓN EJECUTADA",
                "-" * 80,
                f"Build             : {variant.build_mode}",
                f"Optimización      : {variant.optimization}",
                f"Identidad exacta  : {variant.key}",
            )
        )
    if outcome is not None:
        lines.extend(
            (
                "",
                "RESULTADO OBSERVADO",
                "-" * 80,
                f"Estado            : {outcome.status}",
                f"Duración          : {outcome.duration_seconds:.6f} segundos",
                f"Diagnóstico       : {outcome.detail or 'sin diagnóstico textual'}",
                f"Evidencia         : {outcome.evidence or 'sin archivo de evidencia'}",
            )
        )
    else:
        lines.extend(
            (
                "",
                "RESULTADO OBSERVADO",
                "-" * 80,
                "Estado            : la prueba todavía no tiene un resultado guardado",
            )
        )
    return "\n".join(lines) + "\n"


def case_contract_payload(case: CaseRecord) -> dict[str, Any]:
    """Project the exact declared scenario without interpreting its oracle."""
    manifest = case.manifest or {}
    return {
        "case_id": case.case_id,
        "contract_id": case.contract_id,
        "component": case.component,
        "objective": case.objective,
        "dependencies": manifest.get("dependencies", []),
        "inputs": manifest.get("inputs"),
        "oracle": manifest.get("oracle"),
    }


def case_evidence_output(
    outcome: CaseOutcome | None, run_root: Path | None = None
) -> str:
    """Read the complete per-case textual evidence without changing the saved run."""
    if outcome is None or not outcome.evidence:
        return "La prueba no tiene un archivo de evidencia registrado.\n"
    evidence_path = Path(outcome.evidence)
    if not evidence_path.is_absolute() and run_root is not None:
        evidence_path = run_root / evidence_path
    candidates: list[Path] = []
    if evidence_path.is_file():
        candidates.append(evidence_path)
        candidates.extend(
            path
            for path in sorted(evidence_path.parent.glob("*.log"))
            if path != evidence_path
        )
    elif evidence_path.is_dir():
        candidates.extend(sorted(evidence_path.glob("*.log")))
    if not candidates:
        return f"La evidencia registrada no está disponible: {evidence_path}\n"
    sections: list[str] = []
    for path in candidates:
        try:
            contents = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            contents = f"No se pudo leer el archivo: {error}\n"
        separator = "" if contents.endswith("\n") else "\n"
        sections.append(
            f"ARCHIVO: {path}\n{'-' * 80}\n{contents}{separator}"
        )
    return "\n".join(sections)


def retry_variants(
    visible: Sequence[RunVariant], outcomes: Sequence[CaseOutcome]
) -> tuple[RunVariant, ...]:
    """Project terminal failures back to the exact visible plan deterministically."""
    by_key = {variant.key: variant for variant in visible}
    if len(by_key) != len(visible):
        raise ValueError("visible retry plan contains duplicate variant keys")
    status_by_key: dict[str, str] = {}
    for outcome in outcomes:
        if outcome.key in status_by_key:
            raise ValueError(f"duplicate outcome key in retry projection: {outcome.key}")
        status_by_key[outcome.key] = outcome.status
    terminal_keys = {
        key for key, status in status_by_key.items() if status in TERMINAL_FAILURES
    }
    unknown = sorted(terminal_keys - set(by_key))
    if unknown:
        raise ValueError("terminal outcomes are outside the visible plan: " + ", ".join(unknown))
    return tuple(variant for variant in visible if variant.key in terminal_keys)


def active_retry_variants(
    failed: Sequence[RunVariant], disabled_keys: Iterable[str]
) -> tuple[RunVariant, ...]:
    """Return exact failed variants not temporarily disabled by the operator."""
    by_key = {variant.key: variant for variant in failed}
    if len(by_key) != len(failed):
        raise ValueError("failed retry plan contains duplicate variant keys")
    disabled = set(disabled_keys)
    unknown = sorted(disabled - set(by_key))
    if unknown:
        raise ValueError("disabled retry variants are outside the failed plan: " + ", ".join(unknown))
    return tuple(variant for variant in failed if variant.key not in disabled)


def row_progress_text(status: str) -> str:
    """Project one execution state into a compact per-row progress value."""
    if status == "LISTO":
        return "0%"
    if status in {"RUNNING", "COMPILING"}:
        return "EN CURSO"
    if status in RESULT_STATUSES or status == "DESACTIVADA":
        return "100%"
    if status == "PENDIENTE":
        return "NO EJECUTABLE"
    if status == "FILTRADO":
        return "FUERA"
    return ""


def row_action_text(status: str, executable: bool) -> str:
    """Describe the exact-row action without implying a broader selection."""
    if not executable:
        return ""
    if status == "RUNNING":
        return "Cancelar prueba"
    if status == "COMPILING":
        return "En ejecución"
    if status in RESULT_STATUSES:
        return "Volver a correr"
    return "Ejecutar"


def status_filter_matches(filter_name: str, status: str) -> bool:
    """Match one row status without changing the underlying execution plan."""
    if filter_name == "ALL":
        return True
    if filter_name == "FAILURES":
        return status in TERMINAL_FAILURES
    if filter_name == "ACTIVE":
        return status in {"RUNNING", "COMPILING"}
    if filter_name == "WAITING":
        return status in {"LISTO", "PENDIENTE"}
    return status == filter_name


def condensed_error_report(
    outcomes: Sequence[CaseOutcome], cancelled: bool, evidence: str
) -> str:
    """Render one causal report shared by infrastructure and suite closures."""
    failures = [item for item in outcomes if item.status in TERMINAL_FAILURES]
    known = [item for item in outcomes if item.status == "KNOWN_FAILURE"]
    counts = Counter(item.status for item in outcomes)
    controller_failure = evidence.startswith("ERROR:")
    lines = [
        "REPORTE CONDENSADO — NSC BUILDER 3",
        "=" * 88,
        f"Evidencia: {evidence}",
        f"Cancelado: {'sí' if cancelled else 'no'}",
        "Resultados: "
        + ", ".join(f"{key}={value}" for key, value in sorted(counts.items())),
        "",
    ]
    if failures:
        grouped: dict[str, list[CaseOutcome]] = defaultdict(list)
        for item in failures:
            grouped[item.detail or item.status].append(item)
        lines.append(f"CAUSAS TERMINALES ÚNICAS: {len(grouped)}")
        for index, (cause, items) in enumerate(grouped.items(), start=1):
            lines.extend(
                [
                    "-" * 88,
                    f"CAUSA {index}/{len(grouped)} · {len(items)} ocurrencia(s)",
                    cause,
                ]
            )
            lines.extend(f"  - {item.key}" for item in items)
    elif controller_failure:
        lines.extend(
            [
                "CAUSAS TERMINALES ÚNICAS: 1",
                "-" * 88,
                "CAUSA 1/1 · fallo del controlador",
                evidence,
            ]
        )
    else:
        lines.append("No hubo fallos terminales.")
    if known:
        lines.extend(["", f"DEFECTOS CONOCIDOS OBSERVADOS: {len(known)}"])
        lines.extend(f"  - {item.key}: {item.detail}" for item in known)
    return "\n".join(lines)
