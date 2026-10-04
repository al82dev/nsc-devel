#!/usr/bin/env python3
"""Generate finished and pending NativeSharp 3.0 views from canonical tracking."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

import native_sharp_3_catalog as catalog


DEFAULT_TRACKING = Path("A:/NATIVESHARP-3.0-TRACKING.md")
DEFAULT_MASTER = Path("A:/DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md")
DEFAULT_FINISHED = Path("A:/NATIVESHARP-3.0-TERMINADOS.md")
DEFAULT_PENDING = Path("A:/NATIVESHARP-3.0-PENDIENTES.md")

_DEPENDENCIES = re.compile(
    r"\bDependencias?:\s*(.+?)(?=;\s*cierre:|$)", re.IGNORECASE
)
_OPEN_EVIDENCE = re.compile(
    r"\b(?:falta(?:n)?|pendiente(?:s)?|abiert[ao]s?|incomplet[ao]s?|"
    r"no existe|sin soporte|sin baseline|requiere|bloquead[ao]s?)\b",
    re.IGNORECASE,
)
_ITEM_REFERENCE = re.compile(r"\bNS3-[A-Z0-9]+(?:-[A-Z0-9]+)+\b")


def load_tracking(tracking: Path, master: Path) -> list[catalog.CatalogItem]:
    """Read the requested tracking view and reject it when it has drifted."""
    items = catalog.parse_legacy_tracking(tracking)
    canonical = catalog.parse_catalogue(master)
    expected = catalog.render_tracking(canonical)
    observed = tracking.read_text(encoding="utf-8-sig")
    if observed != expected:
        raise RuntimeError(
            "tracking differs from the canonical catalogue; regenerate it before status views"
        )
    if items != canonical:
        raise RuntimeError("tracking parser did not reproduce the canonical 308-item catalogue")
    return items


def partition_items(
    items: list[catalog.CatalogItem],
) -> tuple[list[catalog.CatalogItem], list[catalog.CatalogItem]]:
    finished = [item for item in items if item.state in catalog.TERMINAL_STATES]
    pending = [item for item in items if item.state not in catalog.TERMINAL_STATES]
    if len(finished) + len(pending) != len(items):
        raise RuntimeError("status views do not partition the catalogue")
    if {item.item_id for item in finished} & {item.item_id for item in pending}:
        raise RuntimeError("an item appears in both status views")
    return finished, pending


def explicit_dependency(item: catalog.CatalogItem) -> str:
    match = _DEPENDENCIES.search(item.contract)
    if match:
        return match.group(1).strip()
    references = sorted(
        {
            reference
            for reference in _ITEM_REFERENCE.findall(item.contract + "; " + item.evidence)
            if reference != item.item_id
        }
    )
    if references:
        return "Referencias/dependencias registradas: " + ", ".join(references) + "."
    if item.state == "BLOQUEADA":
        return "Bloqueo explícito registrado; véase la causa en la columna siguiente."
    if re.search(r"\b(?:sin soporte|bloquead[ao]s?)\b", item.evidence, re.IGNORECASE):
        return "Bloqueo técnico registrado en la evidencia; sin dependencia externa identificada."
    return "Sin dependencia ni bloqueo explícito registrado."


def open_evidence(item: catalog.CatalogItem) -> str:
    clauses = [part.strip() for part in item.evidence.split(";") if part.strip()]
    selected = [clause for clause in clauses if _OPEN_EVIDENCE.search(clause)]
    return "; ".join(selected)


def closure_obstacle(item: catalog.CatalogItem) -> str:
    evidence_gap = open_evidence(item)
    if item.state == "BLOQUEADA":
        return evidence_gap or item.evidence or "El registro marca bloqueo pero no describe su causa."
    if evidence_gap:
        return evidence_gap
    reasons = {
        "PENDIENTE": "No hay implementación ni evidencia de cierre registrada.",
        "EN CURSO": "La implementación es parcial y todavía no satisface todo el contrato de cierre.",
        "PRUEBAS DIRIGIDAS": (
            "Sólo hay evidencia dirigida; falta demostrar el contrato completo y promover el estado."
        ),
        "REQUIERE AUDITORÍA": (
            "Falta la auditoría que determine cobertura real, defectos y evidencia de cierre."
        ),
        "ENRUTADA": (
            "El trabajo tiene destino/tanda, pero aún no registra implementación y validación de cierre."
        ),
        "REFERENCIA": (
            "Es inventario de referencia, no una entrega terminal; falta decidir si se implementa "
            "o se cierra mediante una decisión explícita."
        ),
    }
    return reasons.get(
        item.state,
        f"El estado {item.state} no es terminal y no registra evidencia suficiente para cerrarlo.",
    )


def _summary(items: list[catalog.CatalogItem]) -> list[str]:
    by_component = Counter(item.component for item in items)
    by_state = Counter(item.state for item in items)
    lines = [
        f"- Total: **{len(items)}** ítems.",
        "- Por estado: "
        + ", ".join(f"`{state}`={by_state[state]}" for state in sorted(by_state))
        + ".",
        "- Por componente: "
        + ", ".join(
            f"{component}={by_component[component]}"
            for component in sorted(
                by_component,
                key=lambda value: catalog.COMPONENT_ORDER.get(value, 999),
            )
        )
        + ".",
    ]
    return lines


def render_finished(items: list[catalog.CatalogItem], total: int) -> str:
    lines = [
        "# NativeSharp 3.0 — Ítems terminados",
        "",
        "> Vista generada desde `A:/NATIVESHARP-3.0-TRACKING.md`; no editar a mano. "
        "La autoridad de estado sigue siendo el catálogo del documento maestro.",
        "",
        "Incluye únicamente estados terminales: `TERMINADA`, `CERTIFICADA`, `DIFERIDA` y "
        "`NO APLICA`. Los estados con pruebas parciales o trabajo de referencia permanecen en "
        "pendientes para no inflar el avance.",
        "",
        *_summary(items),
        f"- Cobertura del catálogo: **{len(items)}/{total}**.",
        "",
        "| Componente | ID | Entrega | Estado | Contrato satisfecho/decidido | Evidencia |",
        "|---|---|---|---|---|---|",
    ]
    for item in items:
        lines.append(
            f"| {catalog.clean_cell(item.component)} | `{item.item_id}` | "
            f"{catalog.clean_cell(item.title)} | `{item.state}` | "
            f"{catalog.clean_cell(item.contract)} | {catalog.clean_cell(item.evidence)} |"
        )
    return "\n".join(lines) + "\n"


def render_pending(items: list[catalog.CatalogItem], total: int) -> str:
    lines = [
        "# NativeSharp 3.0 — Ítems pendientes",
        "",
        "> Vista generada desde `A:/NATIVESHARP-3.0-TRACKING.md`; no editar a mano. "
        "La autoridad de estado sigue siendo el catálogo del documento maestro.",
        "",
        "Aquí aparece todo estado no terminal. `PRUEBAS DIRIGIDAS` significa que existe avance "
        "comprobado, no que el contrato completo esté cerrado. La ausencia de un bloqueo explícito "
        "no equivale a terminado: normalmente falta implementación, auditoría o evidencia acumulada.",
        "",
        *_summary(items),
        f"- Cobertura del catálogo: **{len(items)}/{total}**.",
        "",
        "| Componente | ID | Entrega | Estado | Dependencia o bloqueo | Qué impide cerrarlo | Condición exacta de cierre | Evidencia/fuente actual |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for item in items:
        lines.append(
            f"| {catalog.clean_cell(item.component)} | `{item.item_id}` | "
            f"{catalog.clean_cell(item.title)} | `{item.state}` | "
            f"{catalog.clean_cell(explicit_dependency(item))} | "
            f"{catalog.clean_cell(closure_obstacle(item))} | "
            f"{catalog.clean_cell(item.contract)} | {catalog.clean_cell(item.evidence)} |"
        )
    return "\n".join(lines) + "\n"


def _write_or_check(path: Path, expected: str, check: bool) -> None:
    if check:
        if not path.is_file() or path.read_text(encoding="utf-8-sig") != expected:
            raise RuntimeError(f"generated status view differs: {path}")
        return
    path.write_text(expected, encoding="utf-8", newline="\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tracking", type=Path, default=DEFAULT_TRACKING)
    parser.add_argument("--master", type=Path, default=DEFAULT_MASTER)
    parser.add_argument("--finished", type=Path, default=DEFAULT_FINISHED)
    parser.add_argument("--pending", type=Path, default=DEFAULT_PENDING)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    try:
        items = load_tracking(args.tracking, args.master)
        finished, pending = partition_items(items)
        _write_or_check(args.finished, render_finished(finished, len(items)), args.check)
        _write_or_check(args.pending, render_pending(pending, len(items)), args.check)
    except (OSError, RuntimeError) as error:
        print(f"generate_native_sharp_3_status_views: error: {error}", file=sys.stderr)
        return 1
    action = "verified" if args.check else "generated"
    print(
        f"generate_native_sharp_3_status_views: {action}: "
        f"finished={len(finished)}, pending={len(pending)}, total={len(items)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
