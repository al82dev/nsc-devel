import re
import sys
from pathlib import Path


ir = Path(sys.argv[1]).read_text(encoding="utf-8")
for name, instructions in {
    "bump_volatile": ("load volatile i32", "store volatile i32"),
    "read_const_volatile": ("load volatile i32",),
    "read_volatile_void": ("load volatile i32",),
    "bump_volatile_record": ("load volatile i32", "store volatile i32"),
    "read_const_volatile_record": ("load volatile i32",),
}.items():
    match = re.search(r"^define\b[^\n]*@" + name + r"\([^\n]*\)[^{]*\{(.*?)^\}", ir, re.M | re.S)
    if not match or any(instruction not in match.group(1) for instruction in instructions):
        print("Falta acceso volatile en la funcion C:", name)
        sys.exit(1)
