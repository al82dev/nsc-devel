import hashlib
import json
from pathlib import Path

test_root = Path(r"A:\clang-tests")
output = Path(__file__).resolve().parent / "evidencia" / "huellas-pruebas.json"
entries = []
for path in sorted(test_root.rglob("*"), key=lambda p: str(p).lower()):
    if not path.is_file() or "temp" in {part.lower() for part in path.relative_to(test_root).parts}:
        continue
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    entries.append({"path": str(path), "bytes": path.stat().st_size, "sha256": digest.hexdigest()})
output.write_text(json.dumps({"scope": str(test_root), "excludes": ["temp"], "files": entries}, indent=2) + "\n", encoding="utf-8")
print(f"{len(entries)} archivos inventariados en {output}")
