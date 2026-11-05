import json
import os
import re
import subprocess
from collections import Counter
from pathlib import Path

root = Path(r"A:\al82dev-clang")
evidence = Path(__file__).resolve().parent / "evidencia"
commands = evidence / "clang-commands.txt"
out_json = evidence / "auditoria-objetos.json"
report = evidence / "directivas-objetos.txt"
compiler = re.compile(r'^\s*"?[^\"]*?[\\/]clang-cl(?:\.exe)?"?\s', re.I)
object_option = re.compile(r'(?:^|\s)[/-]Fo(?:"([^"]+)"|(\S+))', re.I)
objects = set()
unparsed = []
for lineno, command in enumerate(commands.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
    if not compiler.match(command) or not re.search(r"(?:^|\s)[/-]c(?:\s|$)", command, re.I):
        continue
    match = object_option.search(command)
    if not match:
        unparsed.append(lineno)
        continue
    path = Path(match.group(1) or match.group(2))
    objects.add(path if path.is_absolute() else root / path)
present = sorted((p for p in objects if p.is_file()), key=str)
missing = sorted((str(p) for p in objects if not p.is_file()))
stats = Counter()
errors = []
special = []
with report.open("w", encoding="utf-8", newline="\n") as log:
    for start in range(0, len(present), 40):
        batch = present[start:start + 40]
        proc = subprocess.run(["llvm-readobj", "--coff-directives", *map(str, batch)], capture_output=True, text=True, encoding="utf-8", errors="replace", cwd=root)
        log.write(proc.stdout)
        if proc.returncode:
            errors.append({"batch_start": start, "exit_code": proc.returncode, "stderr": proc.stderr})
        blocks = re.split(r"(?=^File: )", proc.stdout, flags=re.M)
        for block in blocks:
            if not block.startswith("File: "):
                continue
            stats["readobj_files"] += 1
            match = re.search(r"^Directive\(s\):\s*(.*)$", block, re.M)
            directives = match.group(1).lower() if match else ""
            if 'runtimelibrary=mt_staticrelease' in directives:
                stats["mt_staticrelease"] += 1
            elif 'runtimelibrary=' in directives:
                stats["other_runtime_mismatch"] += 1
                special.append({"file": block.splitlines()[0][6:], "classification": "other_runtime_mismatch", "directives": directives})
            else:
                stats["no_runtime_mismatch"] += 1
                special.append({"file": block.splitlines()[0][6:], "classification": "no_runtime_mismatch", "directives": directives})
            if re.search(r"/defaultlib:(?:\"?)(?:msvcrt|msvcrtd|msvcprt|msvcprtd)(?:\.lib)?(?:\"|\s|$)", directives):
                stats["dynamic_crt_defaultlib"] += 1
                special.append({"file": block.splitlines()[0][6:], "classification": "dynamic_crt_defaultlib", "directives": directives})
result = {
    "scope": "Existing objects identified from generated clang target commands; directives do not prove actual linker selection or runtime loads",
    "commands_unparsed_lines": unparsed,
    "expected_unique_objects": len(objects),
    "present_objects": len(present),
    "missing_objects": missing,
    "statistics": dict(stats),
    "special": special,
    "errors": errors,
    "report": str(report),
}
out_json.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"expected": len(objects), "present": len(present), "missing": len(missing), "unparsed": len(unparsed), "statistics": dict(stats), "errors": len(errors)}))
