from __future__ import annotations

import argparse
from pathlib import Path

from pypdf import PdfReader


def flatten_outline(reader: PdfReader) -> list[str]:
    entries: list[str] = []

    def visit(items: list[object], depth: int) -> None:
        for item in items:
            if isinstance(item, list):
                visit(item, depth + 1)
                continue

            title = getattr(item, "title", str(item)).strip()
            try:
                page = reader.get_destination_page_number(item) + 1
            except Exception:
                page = 0
            entries.append(f"{'  ' * depth}- p.{page}: {title}")

    visit(reader.outline, 0)
    return entries


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    reader = PdfReader(args.source)
    args.destination.parent.mkdir(parents=True, exist_ok=True)

    with args.destination.open("w", encoding="utf-8", newline="\n") as output:
        output.write(f"# pages: {len(reader.pages)}\n")
        output.write("# outline\n")
        output.write("\n".join(flatten_outline(reader)))
        output.write("\n# pages\n")
        for index, page in enumerate(reader.pages, start=1):
            output.write(f"\n\n===== PDF PAGE {index} =====\n\n")
            output.write(page.extract_text() or "")


if __name__ == "__main__":
    main()
