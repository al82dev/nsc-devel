import re
import sys
from pathlib import Path


def body(text, marker):
    functions = re.findall(r"^define [^\n]*\{\n.*?^\}", text, re.M | re.S)
    matches = [f for f in functions if marker in f.splitlines()[0]]
    if len(matches) != 1:
        raise ValueError(f"expected one function for {marker}, got {len(matches)}")
    return matches[0]


def accesses(function):
    return re.findall(r"\b(load|store)\s+(atomic\s+)?volatile\s+i(\d+)[^\n]*?, align (\d+)", function)


clang = Path(sys.argv[1]).read_text(encoding="utf-8")
nsc = Path(sys.argv[2]).read_text(encoding="utf-8")
for suffix, width, alignment in [("Small", "8", "1"), ("Medium", "16", "2"),
                                  ("Word", "32", "4"), ("Wide", "64", "8")]:
    for prefix in ("Read", "Increment"):
        c_name = "@matrix_" + prefix.lower() + "_" + suffix.lower() + "("
        n_name = "__" + prefix + suffix + "__"
        expected = [("load", "", width, alignment)]
        if prefix == "Increment":
            expected.append(("store", "", width, alignment))
        c_actual, n_actual = accesses(body(clang, c_name)), accesses(body(nsc, n_name))
        if c_actual != expected or n_actual != expected:
            raise SystemExit(f"{c_name}: clang={c_actual}; nsc={n_actual}; expected={expected}")
print("Matriz IR volatile: anchos, alineaciones y secuencias coinciden con Clang.")
