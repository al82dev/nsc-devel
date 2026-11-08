import re
import sys
from pathlib import Path


def access(path, symbol, operation):
    text = Path(path).read_text(encoding="utf-8")
    matches = re.findall(r"^define\b[^\n]*" + re.escape(symbol) + r"[^\n]*\{\n(.*?)^\}", text, re.M | re.S)
    if len(matches) != 1:
        raise ValueError(f"{path}: expected one definition of {symbol}, found {len(matches)}")
    instructions = [line.strip() for line in matches[0].splitlines()
                    if re.search(r"\b" + operation + r"\s+(?:atomic\s+)?volatile\s+i32\b", line)]
    if len(instructions) != 1:
        raise ValueError(f"{path}: expected one volatile {operation}, found {len(instructions)}")
    line = instructions[0]
    alignment = re.search(r"\balign\s+(\d+)", line)
    if alignment is None:
        raise ValueError(f"{path}: missing alignment: {line}")
    order = "acquire" if re.search(r"\bacquire\b", line) else "release" if re.search(r"\brelease\b", line) else "none"
    return (bool(re.search(r"\b" + operation + r"\s+atomic\b", line)), order, int(alignment.group(1)))


def main():
    if len(sys.argv) != 3:
        raise ValueError("usage: comparar-var-volatile.py clang.ll nsc.ll")
    different = False
    for c_name, ns_name, operation in (("var_volatile_read", "VarVolatileRead", "load"),
                                        ("var_volatile_write", "VarVolatileWrite", "store")):
        reference = access(sys.argv[1], c_name, operation)
        actual = access(sys.argv[2], ns_name, operation)
        print(f"{operation}: Clang={reference} NSC={actual} {'IGUAL' if reference == actual else 'DIFIERE'}")
        different |= reference != actual
    return 1 if different else 0


try:
    sys.exit(main())
except (OSError, ValueError) as error:
    print(error, file=sys.stderr)
    sys.exit(2)
