#!/usr/bin/env python3
"""Generate the subordinate-source inventory for the NativeSharp 3 master plan."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
from collections import Counter
from pathlib import Path
from typing import NamedTuple


REGISTRY_PATH = Path("A:/NATIVESHARP-3.0-SOURCE-REGISTRY.md")
INVENTORY_PATH = Path("A:/NATIVESHARP-3.0-SOURCE-INVENTORY.json")

SOURCE_ROOTS = (
    ("NSC", Path("A:/nsc-devel")),
    ("Historia", Path("A:/nsc-historia")),
    ("Reconstrucción", Path("A:/rebuild-context")),
    ("Runtime/STL", Path("A:/nativesharp-libraries")),
    ("Runtime/STL histórico", Path("A:/nativesharp-libraries-2.0-development")),
    ("Bindings", Path("A:/nsc-bindings-devel")),
    ("NSLink", Path("A:/nslink-devel")),
    ("NSASM", Path("A:/nsasm-devel")),
    ("NSDBG", Path("A:/nsc-debugger")),
    ("NSC-LSP", Path("A:/nsc-lsp")),
    ("NSC-VSCode", Path("A:/nsc-vscode")),
    ("Project Manager", Path("A:/nsc-project-manager")),
    ("Documentación", Path("A:/nsc-documentation")),
    ("SaruDX", Path("A:/porting-to-3.0.0/SaruFrameworkNativeSharp")),
    ("SaruGL", Path("A:/porting-to-3.0.0/SaruFrameworkGLNativeSharp")),
)

ROOT_DOCUMENTS = (
    Path("A:/DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md"),
    Path("A:/NATIVESHARP-3.0-TRACKING.md"),
    Path("A:/NATIVESHARP-3.0-LEDGER-LIMITACIONES.md"),
    Path("A:/NATIVESHARP-3.0-PENDIENTES-POR-COMPONENTE.md"),
    Path("A:/NATIVESHARP-3.0-TRACKING - Copy.md"),
    Path("A:/native-sharp-3.0-4.0.md"),
    Path("A:/vsBeef.md"),
    Path("A:/nsc-function-namespace.md"),
    Path("A:/nativesharp-tandas-progreso.md"),
    Path("A:/NSC_3_PDB_DEBUG_AUDIT.md"),
    Path("A:/para_el_nsc.md"),
    Path("A:/para_el_nslink.md"),
    Path("A:/para_el_nsbdg.md"),
    Path("A:/limitaciones-compilador-v1.md"),
    Path("A:/limitaciones-compilador-v2.md"),
    Path("A:/limitaciones-conversion-v3.md"),
    Path("A:/limitaciones-conversion-v4.md"),
)

EXCLUDED_PARTS = {
    ".git",
    ".vs",
    "_nscbuilder",
    "bin",
    "build",
    "build-clang",
    "convision-reference",
    "native-sharp-dev-history",
    "native-sharp-historicos",
    "nsc-docs",
    "node_modules",
    "obj",
    "out",
    "site-packages",
    "third_party",
    "vendor",
}

NON_GIT_ARCHIVE_PARTS = {
    "historicos",
    "native-sharp-dev-history",
    "native-sharp-historicos",
    "nsc-docs",
}

PLANNING_PATTERN = re.compile(
    r"roadmap|plan|tracking|pendient|limitaci|backlog|milestone|progreso|"
    r"tanda|audit|what.?s.?new|changes|contract|design|feature|status",
    re.IGNORECASE,
)
CONTENT_PATTERN = re.compile(
    r"NS3-[A-Z0-9-]+|NativeSharp\s+3(?:\.0)?|Language\s+3|v\.?3\.0",
    re.IGNORECASE,
)


class SourceEntry(NamedTuple):
    owner: str
    path: str
    category: str
    sha256: str
    disposition: str


def normalized_path(path: Path) -> str:
    return path.as_posix().replace("A:/", "A:/", 1)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tracked_markdown(root: Path) -> list[Path]:
    probe = subprocess.run(
        ["git", "rev-parse", "--is-inside-work-tree"],
        cwd=root,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if probe.returncode == 0 and probe.stdout.strip() == "true":
        result = subprocess.run(
            ["git", "ls-files", "--", "*.md", "*.MD"],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        return [root / relative for relative in result.stdout.splitlines() if relative]
    paths: list[Path] = []
    for directory, names, files in os.walk(root):
        names[:] = [
            name
            for name in names
            if name.lower() not in EXCLUDED_PARTS
            and name.lower() not in NON_GIT_ARCHIVE_PARTS
            and not name.lower().startswith("build-")
        ]
        base = Path(directory)
        paths.extend(base / name for name in files if name.lower().endswith(".md"))
    return sorted(paths)


def excluded(path: Path, root: Path) -> bool:
    try:
        parts = path.relative_to(root).parts
    except ValueError:
        return True
    lowered = {part.lower() for part in parts[:-1]}
    if lowered & EXCLUDED_PARTS:
        return True
    return any(part.lower().startswith("build-") for part in parts[:-1])


def category_for(path: Path) -> str:
    name = path.name.lower()
    if path == ROOT_DOCUMENTS[0]:
        return "autoridad"
    if path in ROOT_DOCUMENTS[1:3]:
        return "vista/ledger generado"
    if "roadmap" in name:
        return "roadmap"
    if "tracking" in name or "progreso" in name or "status" in name:
        return "tracking histórico"
    if "limit" in name:
        return "limitaciones"
    if "plan" in name or "pendient" in name or "backlog" in name:
        return "plan"
    if "contract" in name or "design" in name:
        return "contrato/diseño"
    if "tanda" in name or "audit" in name or "changes" in name or "what" in name:
        return "evidencia"
    return "referencia 3.0"


def is_candidate(path: Path, root: Path) -> bool:
    if excluded(path, root) or not path.is_file():
        return False
    relative = path.relative_to(root).as_posix()
    if PLANNING_PATTERN.search(relative):
        return True
    try:
        text = path.read_text(encoding="utf-8-sig", errors="replace")
    except OSError:
        return False
    return CONTENT_PATTERN.search(text) is not None


def collect_sources() -> list[SourceEntry]:
    entries: dict[str, SourceEntry] = {}
    canonical = {ROOT_DOCUMENTS[0], ROOT_DOCUMENTS[1], ROOT_DOCUMENTS[2]}
    for path in ROOT_DOCUMENTS:
        if not path.is_file():
            continue
        key = normalized_path(path)
        entries[key.casefold()] = SourceEntry(
            "Raíz A:",
            key,
            category_for(path),
            file_sha256(path),
            "CANÓNICA" if path in canonical else "SUBORDINADA A §24",
        )
    for owner, root in SOURCE_ROOTS:
        if not root.is_dir():
            continue
        for path in tracked_markdown(root):
            if not is_candidate(path, root):
                continue
            key = normalized_path(path)
            entries.setdefault(
                key.casefold(),
                SourceEntry(owner, key, category_for(path), file_sha256(path), "SUBORDINADA A §24"),
            )
    return sorted(entries.values(), key=lambda item: item.path.casefold())


def render_inventory(entries: list[SourceEntry]) -> str:
    payload = {
        "schema": "NativeSharp3SourceInventory/1",
        "authority": "A:/DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md#24",
        "entries": [entry._asdict() for entry in entries],
    }
    return json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def render_registry(entries: list[SourceEntry], inventory_text: str) -> str:
    owner_counts = Counter(entry.owner for entry in entries)
    category_counts = Counter(entry.category for entry in entries)
    inventory_hash = hashlib.sha256(inventory_text.encode("utf-8")).hexdigest()
    lines = [
        "# Registro generado de fuentes NativeSharp 3.0",
        "",
        "Autoridad de estado: `A:\\DOCUMENTO-MAESTRO-NATIVESHARP-3.0.md`, sección 24.",
        "Este archivo sólo inventaría fuentes de diseño, evidencia e historia; no es un segundo tablero.",
        "",
        f"- Fuentes registradas: **{len(entries)}**",
        f"- SHA-256 del inventario JSON: `{inventory_hash}`",
        "",
        "## Cobertura por dueño",
        "",
        "| Dueño | Documentos |",
        "|---|---:|",
    ]
    lines.extend(f"| {owner} | {count} |" for owner, count in sorted(owner_counts.items()))
    lines.extend(("", "## Cobertura por clase", "", "| Clase | Documentos |", "|---|---:|"))
    lines.extend(f"| {category} | {count} |" for category, count in sorted(category_counts.items()))
    lines.extend(
        (
            "",
            "## Regla de actualización",
            "",
            "`tools/native_sharp_3_source_inventory.py` regenera este resumen y el inventario JSON.",
            "El verificador canónico rechaza diferencias de contenido, rutas o hashes.",
            "",
        )
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--registry", type=Path, default=REGISTRY_PATH)
    parser.add_argument("--inventory", type=Path, default=INVENTORY_PATH)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    entries = collect_sources()
    inventory = render_inventory(entries)
    registry = render_registry(entries, inventory)
    if args.check:
        if not args.inventory.is_file() or args.inventory.read_text(encoding="utf-8-sig") != inventory:
            raise RuntimeError("NativeSharp 3 source inventory differs; regenerate it")
        if not args.registry.is_file() or args.registry.read_text(encoding="utf-8-sig") != registry:
            raise RuntimeError("NativeSharp 3 source registry differs; regenerate it")
    else:
        args.inventory.write_text(inventory, encoding="utf-8", newline="\n")
        args.registry.write_text(registry, encoding="utf-8", newline="\n")
    print(f"native_sharp_3_source_inventory: OK: {len(entries)} sources")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
