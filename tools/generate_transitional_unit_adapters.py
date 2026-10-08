"""Generate one isolated C++ adapter per explicitly registered historical owner."""

from __future__ import annotations

import argparse
import os
import re
from dataclasses import dataclass
from pathlib import Path


IDENTIFIER = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")
COMPONENT = re.compile(r"^[a-z0-9][a-z0-9-]*$")
SYMBOL = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
VOID_MAIN = re.compile(r"^\s*int\s+main\s*\(\s*(?:void\s*)?\)", re.MULTILINE)


class GenerationError(ValueError):
    """Raised when the explicit adapter registry is incomplete or ambiguous."""


@dataclass(frozen=True)
class Entry:
    identifier: str
    component: str
    owner: Path
    symbol: str


def load_registry(path: Path, source_root: Path) -> tuple[Entry, ...]:
    """Load and validate every non-comment TSV row without recursive discovery."""

    resolved_registry = path.resolve()
    resolved_source_root = source_root.resolve()
    entries: list[Entry] = []
    identifiers: set[str] = set()
    owners: set[Path] = set()
    symbols: set[str] = set()
    for line_number, raw_line in enumerate(
        resolved_registry.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4:
            raise GenerationError(
                f"{resolved_registry}:{line_number}: expected four tab-separated fields"
            )
        identifier, component, owner_text, symbol = fields
        if IDENTIFIER.fullmatch(identifier) is None:
            raise GenerationError(f"invalid test identifier: {identifier}")
        if COMPONENT.fullmatch(component) is None:
            raise GenerationError(f"invalid component: {component}")
        if SYMBOL.fullmatch(symbol) is None:
            raise GenerationError(f"invalid entry symbol: {symbol}")
        owner = (resolved_source_root / owner_text).resolve()
        if not owner.is_relative_to(resolved_source_root):
            raise GenerationError(f"owner escapes source root: {owner_text}")
        if not owner.is_file():
            raise GenerationError(f"owner does not exist: {owner}")
        source = owner.read_text(encoding="utf-8")
        if VOID_MAIN.search(source) is None:
            raise GenerationError(f"owner does not expose int main(): {owner}")
        if identifier in identifiers:
            raise GenerationError(f"duplicate test identifier: {identifier}")
        if owner in owners:
            raise GenerationError(f"duplicate owner: {owner}")
        if symbol in symbols:
            raise GenerationError(f"duplicate entry symbol: {symbol}")
        identifiers.add(identifier)
        owners.add(owner)
        symbols.add(symbol)
        entries.append(Entry(identifier, component, owner, symbol))
    if not entries:
        raise GenerationError(f"registry contains no entries: {resolved_registry}")
    return tuple(entries)


def clean_generated_sources(output_root: Path) -> None:
    """Remove only stale generated adapter files from the exact managed directory."""

    if not output_root.exists():
        return
    if not output_root.is_dir():
        raise GenerationError(f"output root is not a directory: {output_root}")
    with os.scandir(output_root) as children:
        for child in children:
            if child.is_file(follow_symlinks=False) and child.name.endswith("Adapter.cpp"):
                Path(child.path).unlink()


def generate(entries: tuple[Entry, ...], output_root: Path) -> tuple[Path, Path]:
    """Write adapters, registration definitions, and the CMake source inventory."""

    resolved_output = output_root.resolve()
    resolved_output.mkdir(parents=True, exist_ok=True)
    clean_generated_sources(resolved_output)
    definitions: list[str] = []
    sources: list[Path] = []
    for index, entry in enumerate(entries, start=1):
        adapter = resolved_output / f"{index:03d}_{entry.owner.stem}Adapter.cpp"
        include_path = entry.owner.as_posix().replace('"', '\\"')
        adapter.write_text(
            f"#define main {entry.symbol}\n"
            f'#include "{include_path}"\n'
            "#undef main\n",
            encoding="utf-8",
        )
        definitions.append(
            f'NSC_TRANSITIONAL_ENTRY("{entry.identifier}", "{entry.component}", '
            f"{entry.symbol})"
        )
        sources.append(adapter)

    definitions_path = resolved_output / "TransitionalLegacyEntries.def"
    definitions_path.write_text("\n".join(definitions) + "\n", encoding="utf-8")
    cmake_path = resolved_output / "TransitionalLegacySources.cmake"
    cmake_lines = ["set(NSC_TRANSITIONAL_ADAPTER_SOURCES"]
    cmake_lines.extend(f'    "{source.as_posix()}"' for source in sources)
    cmake_lines.append(")")
    cmake_path.write_text("\n".join(cmake_lines) + "\n", encoding="utf-8")
    return definitions_path, cmake_path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    options = parse_arguments()
    entries = load_registry(options.registry, options.source_root)
    definitions, cmake = generate(entries, options.output_root)
    print(
        f"generated transitional adapters: entries={len(entries)} "
        f"definitions={definitions} cmake={cmake}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
