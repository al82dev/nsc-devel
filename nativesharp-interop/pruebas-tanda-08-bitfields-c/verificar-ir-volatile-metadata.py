import re
import sys
from pathlib import Path

expected = [
    ("load", "4"), ("load", "4"), ("load", "1"), ("load", "1"),
    ("load", "4"), ("store", "4"), ("load", "4"), ("store", "4"),
    ("load", "1"), ("store", "1"), ("load", "1"), ("store", "1"),
    ("load", "4"), ("load", "4"),
    ("load", "4"), ("store", "4"), ("load", "4"), ("store", "4"),
]
for filename in sys.argv[1:]:
    text = Path(filename).read_text(encoding="utf-8")
    actual = re.findall(r"\b(load|store)\s+volatile\s+i32[^\n]*?, align (\d+)", text)
    if actual != expected:
        raise SystemExit(f"{filename}: expected {expected}, got {actual}")
print("IR de ambos consumidores conserva las operaciones volatile esperadas.")
