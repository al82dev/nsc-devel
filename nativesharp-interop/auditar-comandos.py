import json
import re
from collections import Counter
from pathlib import Path

root = Path(__file__).resolve().parent
commands_path = root / "evidencia" / "clang-commands.txt"
output_path = root / "evidencia" / "auditoria-runtime.json"
switch = re.compile(r"(?:^|\s)[/-](MTd?|MDd?)(?=\s|$)", re.IGNORECASE)
stats = Counter()
exceptions = []
for line_number, command in enumerate(commands_path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
    if not re.match(r'^\s*"?[^\"]*?[\\/]clang-cl(?:\.exe)?"?\s', command, re.IGNORECASE):
        continue
    if not re.search(r"(?:^|\s)(?:/|-)(?:c)(?:\s|$)", command, re.IGNORECASE):
        continue
    matches = [m.upper() for m in switch.findall(command)]
    stats["compile_commands"] += 1
    stats[" ".join(matches) if matches else "NO_RUNTIME_SWITCH"] += 1
    if matches != ["MT"]:
        exceptions.append({"line": line_number, "runtime_switches": matches, "command": command})
result = {
    "source": str(commands_path),
    "scope": "generated Ninja compile commands, not execution logs",
    "counts": dict(stats),
    "exceptions": exceptions,
}
output_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"counts": result["counts"], "exception_count": len(exceptions)}, ensure_ascii=False))
