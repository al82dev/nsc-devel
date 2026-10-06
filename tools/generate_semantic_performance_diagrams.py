"""Generate the versioned diagrams for semantic performance reengineering."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
    """Parse command-line options without deriving environment-specific paths."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dot",
        default="dot",
        help="Graphviz dot executable or name resolved by PATH (default: dot)",
    )
    return parser.parse_args()


def resolve_dot(executable: str) -> str:
    """Resolve Graphviz and fail with a direct diagnostic when it is unavailable."""
    candidate = Path(executable)
    if candidate.is_file():
        return str(candidate.resolve())

    resolved = shutil.which(executable)
    if resolved is None:
        raise SystemExit(f"Graphviz executable not found: {executable}")
    return resolved


def generate_diagram(dot: str, source: Path, destination: Path) -> None:
    """Render one SVG atomically enough that a failed invocation cannot look current."""
    temporary = destination.with_suffix(".svg.tmp")
    temporary.unlink(missing_ok=True)
    destination.parent.mkdir(parents=True, exist_ok=True)

    result = subprocess.run(
        [dot, "-Tsvg", str(source), "-o", str(temporary)],
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        temporary.unlink(missing_ok=True)
        diagnostic = result.stderr.strip() or result.stdout.strip() or "unknown Graphviz error"
        raise SystemExit(f"Graphviz failed for {source}: {diagnostic}")

    temporary.replace(destination)


def main() -> int:
    """Generate every semantic-performance diagram in a deterministic order."""
    arguments = parse_arguments()
    dot = resolve_dot(arguments.dot)
    repository = Path(__file__).resolve().parents[1]
    diagrams = repository / "docs" / "compiler-optimization" / "semantic-performance" / "diagrams"
    sources = diagrams / "src"
    generated = diagrams / "generated"

    source_files = sorted(sources.glob("*.dot"), key=lambda item: item.name)
    if not source_files:
        raise SystemExit(f"No Graphviz sources found in {sources}")

    for source in source_files:
        generate_diagram(dot, source, generated / f"{source.stem}.svg")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
