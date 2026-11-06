import re
import sys
from pathlib import Path


ir = Path(sys.argv[1]).read_text(encoding="utf-8")
for name, instructions in {
    "bump_volatile": (r"\bload (?:atomic )?volatile i32\b", r"\bstore (?:atomic )?volatile i32\b"),
    "read_const_volatile": (r"\bload (?:atomic )?volatile i32\b",),
    "read_volatile_void": (r"\bload (?:atomic )?volatile i32\b",),
    "bump_volatile_record": (r"\bload (?:atomic )?volatile i32\b", r"\bstore (?:atomic )?volatile i32\b"),
    "read_const_volatile_record": (r"\bload (?:atomic )?volatile i32\b",),
}.items():
    match = re.search(r"^define\b[^\n]*@" + name + r"\([^\n]*\)[^{]*\{(.*?)^\}", ir, re.M | re.S)
    if not match or any(re.search(instruction, match.group(1)) is None for instruction in instructions):
        print("Falta acceso volatile en la funcion C:", name)
        sys.exit(1)
