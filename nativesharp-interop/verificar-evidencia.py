import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent
ev = root / "evidencia"
errors = []
objects = json.loads((ev / "auditoria-objetos.json").read_text(encoding="utf-8"))
stats = objects["statistics"]
if not (objects["expected_unique_objects"] == objects["present_objects"] == stats["readobj_files"] == 2673):
    errors.append("object counts differ")
if objects["missing_objects"] or objects["errors"] or objects["commands_unparsed_lines"]:
    errors.append("object audit has unresolved items")
if stats.get("mt_staticrelease", 0) != 2664 or stats.get("no_runtime_mismatch", 0) != 9 or stats.get("dynamic_crt_defaultlib", 0):
    errors.append("object runtime findings differ")
runs = (ev / "ejecuciones-reconstruidas.txt").read_text(encoding="utf-8")
statuses = re.findall(r"^.+: codigo (\d+)\s*$", runs, re.M)
if len(statuses) != 14 or any(code != "0" for code in statuses):
    errors.append("reconstructed executable statuses incomplete or failed")
inputs = json.loads((ev / "entradas-ejemplo-cpp.json").read_text(encoding="utf-8"))["files"]
if sum(x["kind"] == "header" for x in inputs) != 253 or sum(x["kind"] == "library" for x in inputs) != 7:
    errors.append("sample input counts differ")
if any(x["status"] != "present" for x in inputs):
    errors.append("sample inputs missing")
manifest = json.loads((ev / "huellas-pruebas.json").read_text(encoding="utf-8"))["files"]
for item in manifest:
    path = Path(item["path"])
    if not path.is_file():
        errors.append("missing test artifact: " + str(path))
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != item["sha256"] or path.stat().st_size != item["bytes"]:
        errors.append("changed test artifact: " + str(path))
result = {"checked_test_files": len(manifest), "checked_runs": len(statuses), "object_statistics": stats, "errors": errors, "limits": "Checks captured evidence consistency only; does not verify unloaded dependencies, distribution, complete interoperability, or reconstruct the original build log."}
(ev / "verificacion-evidencia.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"checked_test_files": len(manifest), "checked_runs": len(statuses), "errors": len(errors)}))
raise SystemExit(1 if errors else 0)
