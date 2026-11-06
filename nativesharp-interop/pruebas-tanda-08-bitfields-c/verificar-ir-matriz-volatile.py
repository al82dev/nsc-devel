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
failures = []
for suffix, width, c_alignment, n_alignment in [("Small", "8", "8", "1"), ("Medium", "16", "2", "2"),
                                               ("Word", "32", "4", "4"), ("Wide", "64", "8", "8")]:
    prefixes = ("Read", "Increment", "Post", "Pre", "Add") if suffix == "Word" else ("Read", "Increment")
    for prefix in prefixes:
        c_name = "@matrix_" + prefix.lower() + "_" + suffix.lower() + "("
        n_name = "__" + prefix + suffix + "__"
        c_expected = [("load", "", width, c_alignment)]
        n_expected = [("load", "", width, n_alignment)]
        if prefix != "Read":
            c_expected += [("load", "", width, c_alignment), ("store", "", width, c_alignment)]
            n_expected += [("load", "", width, n_alignment), ("store", "", width, n_alignment)]
        c_actual, n_actual = accesses(body(clang, c_name)), accesses(body(nsc, n_name))
        if c_actual != c_expected or n_actual != n_expected:
            failures.append(f"{c_name}: clang={c_actual}; nsc={n_actual}")
if failures:
    raise SystemExit("\n".join(failures))
print("Matriz IR volatile: anchos y secuencias coinciden; alineacion NSC es segura.")
