import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent
headers_log = Path(r"A:\clang-tests\archivos-01\compilacion-inventario.txt")
link_log = Path(r"A:\clang-tests\enlace-01\enlace.txt")
output = root / "evidencia" / "entradas-ejemplo-cpp.json"
found = {}
for line in headers_log.read_text(encoding="utf-8", errors="replace").splitlines():
    match = re.match(r"^Note: including file:\s*(.*?)\s*$", line, re.I)
    if match:
        found.setdefault(match.group(1).lower(), {"path": match.group(1), "kind": "header"})
for line in link_log.read_text(encoding="utf-8", errors="replace").splitlines():
    match = re.match(r"^lld-link: Reading\s+([A-Za-z]:\\.*\.lib)\s*$", line, re.I)
    if match:
        found.setdefault(match.group(1).lower(), {"path": match.group(1), "kind": "library"})
for item in found.values():
    path = Path(item["path"])
    if not path.is_file():
        item["status"] = "missing"
        continue
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    item.update(status="present", bytes=path.stat().st_size, sha256=digest.hexdigest())
entries = sorted(found.values(), key=lambda x: x["path"].lower())
result = {"scope": "headers and libraries observed for one C++ example, not all possible toolchain inputs", "headers_log": str(headers_log), "link_log": str(link_log), "files": entries}
output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"headers": sum(x["kind"] == "header" for x in entries), "libraries": sum(x["kind"] == "library" for x in entries), "missing": sum(x["status"] == "missing" for x in entries)}))
