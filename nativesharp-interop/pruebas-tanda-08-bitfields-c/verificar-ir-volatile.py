import re
import sys
from pathlib import Path


def body(text, marker):
    matches = [m.group(0) for m in re.finditer(r"^define [^\n]*\{\n.*?^\}", text, re.M | re.S)
               if marker in m.group(0).splitlines()[0]]
    if len(matches) != 1:
        raise ValueError(f"expected one function for {marker}, found {len(matches)}")
    return matches[0]


def accesses(function):
    return re.findall(r"\b(load|store)\s+(atomic\s+)?volatile\s+i(\d+)[^\n]*?, align (\d+)", function)


clang = Path(sys.argv[1]).read_text(encoding="utf-8")
nsc = Path(sys.argv[2]).read_text(encoding="utf-8")
cases = [
    ("@read_volatile_first(", "__ReadFirst__", [("load", "", "32", "4")]),
    ("@write_volatile_first(", "__WriteFirst__", [("load", "", "32", "4"), ("store", "", "32", "4")]),
    ("@read_packed_volatile_first(", "__ReadPackedFirst__", [("load", "", "32", "1")]),
    ("@write_packed_volatile_first(", "__WritePackedFirst__", [("load", "", "32", "1"), ("store", "", "32", "1")]),
    ("@read_union_volatile_first(", "__ReadUnionFirst__", [("load", "", "32", "4")]),
    ("@write_union_volatile_first(", "__WriteUnionFirst__", [("load", "", "32", "4"), ("store", "", "32", "4")]),
]
for c_name, n_name, expected in cases:
    c_actual = accesses(body(clang, c_name))
    n_actual = accesses(body(nsc, n_name))
    if c_actual != expected or n_actual != expected:
        raise SystemExit(f"{c_name}: clang={c_actual}, nsc={n_actual}, expected={expected}")
print("IR volatile de campos de bits: ancho, alineacion, orden y numero de accesos coinciden.")
