#!/usr/bin/env python3
"""Maintain the canonical NativeSharp 3.0 catalogue and compact tracking view.

The only state authority is the table between ``CATALOG_BEGIN`` and ``CATALOG_END`` in
``A:/DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md``. The root tracking file is generated from that table;
it is never an independent source of truth.
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path
from typing import NamedTuple


CATALOG_BEGIN = "<!-- NS3-CANONICAL-CATALOG-BEGIN -->"
CATALOG_END = "<!-- NS3-CANONICAL-CATALOG-END -->"

ALLOWED_STATES = {
    "PENDIENTE",
    "EN CURSO",
    "PRUEBAS DIRIGIDAS",
    "TERMINADA",
    "CERTIFICADA",
    "BLOQUEADA",
    "DIFERIDA",
    "NO APLICA",
    "REQUIERE AUDITORÍA",
    "ENRUTADA",
    "REFERENCIA",
}

COMPONENT_ORDER = {
    name: index
    for index, name in enumerate(
        (
            "NSC",
            "NSC-Rendimiento",
            "Runtime/STL",
            "NSLink",
            "NSASM",
            "NSDBG",
            "NSC-LSP",
            "NSC-VSCode",
            "Bindings",
            "Project Manager",
            "Distribución",
            "Saru",
            "Integración",
            "Producto transversal",
            "Limitaciones importadas",
            "Gobernanza documental",
        )
    )
}


class CatalogItem(NamedTuple):
    component: str
    item_id: str
    title: str
    contract: str
    state: str
    evidence: str


def clean_cell(value: str) -> str:
    value = value.replace("\r", " ").replace("\n", " ")
    value = re.sub(r"\s+", " ", value).strip()
    value = re.sub(r"\s+-\s+", "; ", value)
    value = re.sub(r";\s*;", ";", value)
    return value.replace("\\|", "|").replace("|", "\\|")


def plain_cell(value: str) -> str:
    return value.replace("\\|", "|").replace("`", "").replace("**", "").strip()


def split_markdown_row(line: str) -> list[str]:
    if not line.strip().startswith("|"):
        return []
    cells: list[str] = []
    current: list[str] = []
    escaped = False
    for character in line.strip()[1:]:
        if escaped:
            current.extend(("\\", character))
            escaped = False
        elif character == "\\":
            escaped = True
        elif character == "|":
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(character)
    if current:
        cells.append("".join(current).strip())
    return cells


def parse_state(cell: str) -> tuple[str, str]:
    plain = plain_cell(cell)
    for state in sorted(ALLOWED_STATES, key=len, reverse=True):
        if plain == state:
            return state, ""
        if plain.startswith(state + " —") or plain.startswith(state + " -"):
            return state, plain[len(state) :].lstrip(" —-")
    raise RuntimeError(f"invalid state cell: {cell!r}")


def catalogue_section(items: list[CatalogItem]) -> str:
    lines = [
        CATALOG_BEGIN,
        "| Componente | ID | Entrega | Contrato de cierre | Estado | Evidencia / fuente |",
        "|---|---|---|---|---|---|",
    ]
    for item in sorted(
        items,
        key=lambda value: (COMPONENT_ORDER.get(value.component, 999), value.item_id),
    ):
        lines.append(
            "| "
            + " | ".join(
                (
                    clean_cell(item.component),
                    f"`{item.item_id}`",
                    clean_cell(item.title),
                    clean_cell(item.contract),
                    f"`{item.state}`",
                    clean_cell(item.evidence),
                )
            )
            + " |"
        )
    lines.append(CATALOG_END)
    return "\n".join(lines)


def parse_catalogue(master: Path) -> list[CatalogItem]:
    if not master.is_file():
        raise RuntimeError(f"master document not found: {master}")
    text = master.read_text(encoding="utf-8-sig")
    begin = text.find(CATALOG_BEGIN)
    end = text.find(CATALOG_END)
    if begin < 0 or end < 0 or end <= begin:
        raise RuntimeError("canonical catalogue markers are missing from the master document")
    rows: list[CatalogItem] = []
    seen: set[str] = set()
    for line in text[begin:end].splitlines():
        cells = split_markdown_row(line)
        if len(cells) != 6 or plain_cell(cells[0]) in {"Componente", "---"}:
            continue
        item = CatalogItem(
            plain_cell(cells[0]),
            plain_cell(cells[1]),
            plain_cell(cells[2]),
            plain_cell(cells[3]),
            plain_cell(cells[4]),
            plain_cell(cells[5]),
        )
        if item.item_id in seen:
            raise RuntimeError(f"duplicate canonical item: {item.item_id}")
        seen.add(item.item_id)
        rows.append(item)
    if not rows:
        raise RuntimeError("canonical catalogue is empty")
    return rows


def replace_catalogue(master: Path, items: list[CatalogItem], *, allow_create: bool = False) -> None:
    text = master.read_text(encoding="utf-8-sig")
    section = catalogue_section(items)
    begin = text.find(CATALOG_BEGIN)
    end = text.find(CATALOG_END)
    if begin >= 0 and end > begin:
        end += len(CATALOG_END)
        result = text[:begin] + section + text[end:]
    elif allow_create:
        result = text.rstrip() + (
            "\n\n---\n\n## 24. Catálogo operativo canónico y tracking verificable\n\n"
            "Esta tabla es la única autoridad de estado por ID. Las secciones anteriores definen contratos,\n"
            "dependencias y contexto, pero no crean un segundo tablero. `A:\\NATIVESHARP-3.0-TRACKING.md`\n"
            "es una vista compacta generada con `tools/native_sharp_3_catalog.py`.\n\n"
            + section
            + "\n"
        )
    else:
        raise RuntimeError("canonical catalogue markers are missing; run --bootstrap-master once")
    master.write_text(result, encoding="utf-8", newline="\n")


def parse_legacy_tracking(path: Path) -> list[CatalogItem]:
    items: list[CatalogItem] = []
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        cells = split_markdown_row(line)
        if len(cells) != 4:
            continue
        match = re.match(r"(NS3-[A-Z0-9-]+)\s+—\s+(.+)", plain_cell(cells[1]))
        if not match:
            continue
        state, inline_evidence = parse_state(cells[3])
        items.append(
            CatalogItem(
                plain_cell(cells[0]),
                match.group(1),
                match.group(2),
                plain_cell(cells[2]),
                state,
                inline_evidence,
            )
        )
    return items


def _git_feature_commits(repo_root: Path) -> dict[str, str]:
    result = subprocess.run(
        ["git", "log", "--format=%h%x09%s"],
        cwd=repo_root,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    commits: dict[str, str] = {}
    for line in result.stdout.splitlines():
        commit, _, subject = line.partition("\t")
        match = re.search(r"ns3\(t(\d{2})-f(\d{2})\)", subject, re.IGNORECASE)
        if match:
            commits.setdefault(f"NS3-T{match.group(1)}-F{match.group(2)}", commit)
    return commits


def _item_document(repo_root: Path, item_id: str) -> Path | None:
    matches = sorted((repo_root / "docs" / "3.0" / "items").glob(item_id + "-*.md"))
    return matches[0] if len(matches) == 1 else None


def _bullet_contract(text: str, start: int, end_pattern: str) -> str:
    end_match = re.search(end_pattern, text[start:], re.MULTILINE)
    end = start + end_match.start() if end_match else len(text)
    bullets = re.findall(r"^- (.+)$", text[start:end], re.MULTILINE)
    return "; ".join(plain_cell(item) for item in bullets)


def _roadmap_items(repo_root: Path) -> list[CatalogItem]:
    roadmap = (repo_root / "mkdown" / "NativeSharp-Language-3.0-Roadmap.md").read_text(
        encoding="utf-8-sig"
    )
    definitions = (
        ("NSC", r"^## (NS3-T\d{2}-F\d{2}) — (.+)$"),
        ("NSLink", r"^#### (NS3-LNK-\d{2}) — (.+)$"),
        ("NSDBG", r"^#### (NS3-DBG-\d{2}) — (.+)$"),
        ("NSC-LSP", r"^#### (NS3-LSP-\d{2}) — (.+)$"),
        ("NSC-VSCode", r"^#### (NS3-VSC-\d{2}) — (.+)$"),
        ("NSASM", r"^#### (NS3-ASM-\d{2}) — (.+)$"),
    )
    items: list[CatalogItem] = []
    for component, pattern in definitions:
        matches = list(re.finditer(pattern, roadmap, re.MULTILINE))
        for match in matches:
            body_start = match.end()
            next_heading = re.search(r"^#{1,4}\s+", roadmap[body_start:], re.MULTILINE)
            body_end = body_start + next_heading.start() if next_heading else len(roadmap)
            body = roadmap[body_start:body_end]
            implementation = re.search(r"^- \*\*Implementación:\*\*\s*(.+)$", body, re.MULTILINE)
            if implementation:
                contract = implementation.group(1)
            else:
                bullets = re.findall(
                    r"^- (?!\*\*Markdown|\*\*Pruebas)(.+)$", body, re.MULTILINE
                )
                contract = "; ".join(bullets)
            if not contract:
                raise RuntimeError(f"{match.group(1)} has no implementation contract")
            items.append(
                CatalogItem(
                    component,
                    match.group(1),
                    plain_cell(match.group(2)),
                    plain_cell(contract),
                    "PENDIENTE",
                    "",
                )
            )
    return items


def bootstrap_items(master: Path, legacy_tracking: Path, repo_root: Path) -> list[CatalogItem]:
    text = master.read_text(encoding="utf-8-sig")
    try:
        previous = parse_catalogue(master)
    except RuntimeError:
        previous = parse_legacy_tracking(legacy_tracking)
    previous_by_id = {item.item_id: item for item in previous}

    items: list[CatalogItem] = []

    def add(item: CatalogItem) -> None:
        old = previous_by_id.get(item.item_id)
        if old is not None:
            item = item._replace(state=old.state, evidence=old.evidence or item.evidence)
        items.append(item)

    for item in _roadmap_items(repo_root):
        add(item)

    commits = _git_feature_commits(repo_root)
    upgraded: list[CatalogItem] = []
    for item in items:
        evidence = item.evidence
        if item.state in {"TERMINADA", "CERTIFICADA"} and item.item_id.startswith("NS3-T"):
            commit = commits.get(item.item_id)
            document = _item_document(repo_root, item.item_id)
            parts: list[str] = []

            def append_unique(part: str) -> None:
                normalized = part.replace("`", "")
                if all(existing.replace("`", "") != normalized for existing in parts):
                    parts.append(part)

            if commit:
                append_unique(f"commit `{commit}`")
            if document:
                append_unique(str(document.relative_to(repo_root)).replace("\\", "/"))
            if evidence:
                for part in evidence.split("; "):
                    append_unique(part)
            evidence = "; ".join(parts)
        upgraded.append(item._replace(evidence=evidence))
    items = upgraded

    for match in re.finditer(r"^### (P\d{2}) — (.+)$", text, re.MULTILINE):
        short_id, title = match.groups()
        add(
            CatalogItem(
                "NSC-Rendimiento",
                "NS3-" + short_id,
                title,
                _bullet_contract(text, match.end(), r"^### "),
                "PENDIENTE" if short_id == "P11" else "EN CURSO",
                "Documento maestro §7; `docs/compiler-optimization/`",
            )
        )

    # Section 8 owns the two-column Runtime/STL backlog. Later audit tables
    # intentionally repeat some E-numbers with additional state/evidence
    # columns; excluding literal table separators keeps those observations
    # from becoming duplicate canonical work items.
    for number, contract in re.findall(
        r"^\| E(\d{3})(?: / NS3-R\d{3})? \| ([^|\n]+) \|$", text, re.MULTILINE
    ):
        add(
            CatalogItem(
                "Runtime/STL",
                f"NS3-R{number}",
                f"Runtime/STL E{number}",
                plain_cell(contract),
                "REFERENCIA" if number in {"001", "002"} else "REQUIERE AUDITORÍA",
                "Documento maestro §8; `A:/nativesharp-libraries`",
            )
        )

    for gate, dependencies, evidence in re.findall(
        r"^\| (NS3-I(?:\d{2}|035)) \| (.+) \| (.+) \|$", text, re.MULTILINE
    ):
        add(
            CatalogItem(
                "Integración",
                gate,
                f"Gate {gate}",
                f"Dependencias: {plain_cell(dependencies)}; cierre: {plain_cell(evidence)}",
                "PENDIENTE",
                "Documento maestro §9",
            )
        )

    for item_id, contract in re.findall(r"^\| (NS3-X\d{2}) \| (.+) \|$", text, re.MULTILINE):
        add(
            CatalogItem(
                "Producto transversal",
                item_id,
                f"Track transversal {item_id}",
                plain_cell(contract),
                "PENDIENTE",
                "Documento maestro §11.3",
            )
        )

    for item_id, origin, contract, train in re.findall(
        r"^\| (NS3-LIM-[A-Z]+-\d{3}) \| (.+) \| (.+) \| (\d+) \|$",
        text,
        re.MULTILINE,
    ):
        add(
            CatalogItem(
                "Limitaciones importadas",
                item_id,
                f"Limitación importada: {plain_cell(origin)}",
                plain_cell(contract),
                "ENRUTADA",
                f"Tanda {train}; `A:/NATIVESHARP-3.0-LEDGER-LIMITACIONES.md`",
            )
        )

    manual: dict[str, list[tuple[str, str, str]]] = {
        "Bindings": [
            ("NS3-BND-01", "Generación reproducible", "headers/manifests → source, NSMETA, annotations y rejection report deterministas"),
            ("NS3-BND-02", "Contratos de pointers", "nullability, direction, length, lifetime y allocator/deallocator explícitos"),
            ("NS3-BND-03", "Callbacks", "calling convention, thread, capture lifetime y unregister verificables"),
            ("NS3-BND-04", "Errores y handles", "HRESULT/GetLastError/error-code mapping y wrappers move-only/RAII"),
            ("NS3-BND-05", "Paridad precompilada", "surface parity, regeneración estable y consumers sin fuentes"),
            ("NS3-BND-06", "Hosts C/Win32", "exports/imports/layout/callbacks probados desde hosts C/C++"),
            ("NS3-BND-07", "Paquete 3.0", "bindings instalables, manifestados y consumibles desde el prefijo 3.0"),
        ],
        "Project Manager": [
            ("NS3-PM-01", "Schema nsproj", "profiles/configurations y origen efectivo de propiedades"),
            ("NS3-PM-02", "Workspace graph", "ProjectReference, package graph y ciclos con diagnóstico causal"),
            ("NS3-PM-03", "Restore y lock", "lockfile reproducible, offline/no-restore y provenance"),
            ("NS3-PM-04", "Build orchestration", "O0–O3, jobs, cancelación, progreso y logs estructurados"),
            ("NS3-PM-05", "Pack/publish", "contenido, fingerprints, SemVer y verificación antes de publicar"),
            ("NS3-PM-06", "Explicación", "nsc explain build/package/reference con valor, origen y conflicto"),
            ("NS3-PM-07", "GUI 3.0", "crear/abrir/build/test/pack sin duplicar semántica del driver"),
        ],
        "Distribución": [
            ("NS3-DST-01", "Prefijo paralelo", "instalación 3.0 separada y sólo por orden explícita"),
            ("NS3-DST-02", "Manifest canónico", "paths relativos, tamaños, SHA-256 y ownership de cada archivo"),
            ("NS3-DST-03", "Transacciones", "install/upgrade/rollback sin borrar archivos no reclamados"),
            ("NS3-DST-04", "Deploy UI", "selección de destino, verificación y cero recompilación implícita"),
            ("NS3-DST-05", "Archive raíz", "7z sin carpeta contenedora adicional y nombre versión/hash/timestamp"),
            ("NS3-DST-06", "Verificación offline", "extracción limpia funciona sin Visual Studio/.NET"),
            ("NS3-DST-07", "Higiene", "excluir caches, builds, logs y evidencia privada"),
        ],
        "Saru": [
            ("NS3-SARU-01", "SaruGL 3.0", "consumer gráfico externo construido con toolchain 3.0"),
            ("NS3-SARU-02", "SaruDX 3.0", "consumer DirectX externo construido con toolchain 3.0"),
            ("NS3-SARU-03", "Depuración cross-DLL", "breakpoints, stack, símbolos y ownership entre módulos"),
            ("NS3-SARU-04", "Demo insignia", "input/render loop, assets, build incremental, package y cost explorer"),
        ],
        "Gobernanza documental": [
            ("NS3-DOC-01", "Fuente normativa única", "el maestro contiene todos los IDs, estados, contratos y fuentes"),
            ("NS3-DOC-02", "Tracking generado", "la vista raíz se regenera byte a byte desde el catálogo canónico"),
            ("NS3-DOC-03", "Validador", "detecta IDs omitidos/duplicados, estados inválidos y terminales sin evidencia"),
            ("NS3-DOC-04", "Roadmaps subordinados", "cada roadmap queda referenciado como diseño/evidencia, no como segundo tablero"),
        ],
    }
    source_by_component = {
        "Bindings": "Documento maestro §10.8; `A:/nsc-bindings-devel`",
        "Project Manager": "Documento maestro §10.7; `A:/nsc-project-manager`",
        "Distribución": "Documento maestro §10.9",
        "Saru": "Documento maestro §14.7; repositorios Saru en `A:/`",
        "Gobernanza documental": "Documento maestro §0/§24",
    }
    for component, definitions in manual.items():
        for item_id, title, contract in definitions:
            add(
                CatalogItem(
                    component,
                    item_id,
                    title,
                    contract,
                    "EN CURSO" if item_id in {"NS3-DOC-01", "NS3-DOC-02", "NS3-DOC-03"} else "PENDIENTE",
                    source_by_component[component],
                )
            )

    unique: dict[str, CatalogItem] = {}
    for item in items:
        if item.item_id in unique:
            raise RuntimeError(f"bootstrap produced duplicate item: {item.item_id}")
        unique[item.item_id] = item
    return list(unique.values())


def render_tracking(items: list[CatalogItem]) -> str:
    lines = [
        "| Componente / tanda | Ítem | Implementación | Estado |",
        "|---|---|---|---|",
    ]
    for item in sorted(
        items,
        key=lambda value: (COMPONENT_ORDER.get(value.component, 999), value.item_id),
    ):
        state = f"`{item.state}`"
        if item.evidence:
            state += " — " + clean_cell(item.evidence)
        lines.append(
            f"| {clean_cell(item.component)} | `{item.item_id}` — {clean_cell(item.title)} | "
            f"{clean_cell(item.contract)} | {state} |"
        )
    return "\n".join(lines) + "\n"


def update_item(master: Path, item_id: str, state: str, evidence: str | None) -> None:
    if state not in ALLOWED_STATES:
        raise RuntimeError(f"invalid state: {state}")
    items = parse_catalogue(master)
    found = False
    updated: list[CatalogItem] = []
    for item in items:
        if item.item_id != item_id:
            updated.append(item)
            continue
        found = True
        replacement_evidence = item.evidence if evidence is None else evidence
        if state in {"TERMINADA", "CERTIFICADA", "DIFERIDA", "NO APLICA"} and not replacement_evidence:
            raise RuntimeError(f"{state} requires evidence or a recorded decision")
        updated.append(item._replace(state=state, evidence=replacement_evidence))
    if not found:
        raise RuntimeError(f"unknown canonical item: {item_id}")
    replace_catalogue(master, updated)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--master", type=Path, default=Path("A:/DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md"))
    parser.add_argument("--output", type=Path, default=Path("A:/NATIVESHARP-3.0-TRACKING.md"))
    parser.add_argument("--bootstrap-master", action="store_true")
    parser.add_argument("--legacy-tracking", type=Path)
    parser.add_argument("--set-state", metavar="ITEM=STATE")
    parser.add_argument("--evidence")
    parser.add_argument("--stdout", action="store_true")
    args = parser.parse_args()

    if args.bootstrap_master:
        legacy = args.legacy_tracking or args.output
        replace_catalogue(
            args.master,
            bootstrap_items(args.master, legacy, repo_root),
            allow_create=True,
        )
    if args.set_state:
        item_id, separator, state = args.set_state.partition("=")
        if not separator:
            raise RuntimeError("--set-state requires ITEM=STATE")
        update_item(args.master, item_id, state, args.evidence)

    items = parse_catalogue(args.master)
    result = render_tracking(items)
    if args.stdout:
        print(result, end="")
    else:
        args.output.write_text(result, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
