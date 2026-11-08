"""Compares every volatile C field access emitted by NSC with the access emitted by Clang.

Usage: verificar_volatile_clang.py <clang -O1 reference .ll> <nsc project .ll> <report .txt> <table>

The table has one access per line: <key> <clang getter> <clang setter> <NSC method suffix> <unused>.
The Clang reference defines the getter and setter; the NativeSharp project defines VolatileAccess.Get<Name>
and VolatileAccess.Set<Name>. The field access is the last load of the getter and the last store of the setter.

Each access is reduced to (atomic ordering, volatile, IR type, alignment). Verdicts:
  IGUAL        same ordering, same volatility and same IR type; atomic accesses have the same alignment and
               plain accesses have NSC alignment == min(Clang alignment, natural alignment of the IR type)
  SUBALINEADO  plain access whose NSC alignment is lower than that: safe, recorded as evidence
  DEFECTO      different ordering, volatility or IR type, or a higher NSC alignment
  FALTA        the function or the access is missing
Exit code 1 when any access is FALTA or DEFECTO.
"""
import pathlib
import re
import sys

NATURAL = {"i8": 1, "i16": 2, "i32": 4, "i64": 8, "float": 4, "double": 8, "ptr": 8}
ORDERINGS = "unordered|monotonic|acquire|release|acq_rel|seq_cst"
DEFINE = re.compile(r'^define [^@]*@("(?:[^"]+)"|[\w.$]+)\(', re.MULTILINE)
LOAD = re.compile(r"= load (atomic )?(volatile )?(\w+), ptr [^,\n]+?(?: (" + ORDERINGS + r"))?, align (\d+)")
STORE = re.compile(
    r"store (atomic )?(volatile )?(\w+) [^,\n]+, ptr [^,\n]+?(?: (" + ORDERINGS + r"))?, align (\d+)")


def functions(text):
    """Returns a map from function name (quotes removed) to its body text."""
    result = {}
    matches = list(DEFINE.finditer(text))
    for match in matches:
        end = text.find("\n}", match.end())
        result[match.group(1).strip('"')] = text[match.end():end if end >= 0 else len(text)]
    return result


def find_nsc(table, name):
    """Returns the body of the single NSC method VolatileAccess.<name>, or None."""
    pattern = re.compile(r"VolatileAccess__" + re.escape(name) + r"__")
    found = [body for key, body in table.items() if pattern.search(key)]
    if len(found) == 1:
        return found[0]
    return None


def last_access(pattern, body):
    """Returns (ordering, volatile, IR type, alignment) of the last match of pattern in body, or None."""
    if body is None:
        return None
    matches = pattern.findall(body)
    if not matches:
        return None
    atomic, volatile, ir_type, ordering, alignment = matches[-1]
    if not atomic:
        ordering = "none"
    return (ordering, bool(volatile), ir_type, int(alignment))


def verdict(clang, nsc):
    """Classifies one access comparison."""
    if clang is None or nsc is None:
        return "FALTA"
    if clang[0] != nsc[0] or clang[1] != nsc[1] or clang[2] != nsc[2]:
        return "DEFECTO"
    if clang[0] != "none":
        if nsc[3] == clang[3]:
            return "IGUAL"
        return "DEFECTO"
    expected = min(clang[3], NATURAL.get(clang[2], clang[3]))
    if nsc[3] > clang[3]:
        return "DEFECTO"
    if nsc[3] == expected:
        return "IGUAL"
    return "SUBALINEADO"


def access_table(path):
    """Returns (key, clang getter, clang setter, NSC suffix) rows from the table file."""
    rows = []
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 5:
            rows.append((parts[0], parts[1], parts[2], parts[3]))
    return rows


def main():
    """Runs the comparison and writes the report."""
    clang_table = functions(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
    nsc_table = functions(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
    lines = ["acceso operacion clang(orden,volatile,tipo,alineacion) nsc veredicto"]
    failed = False
    for key, getter, setter, name in access_table(sys.argv[4]):
        for operation, pattern, cname, nname in (("lectura", LOAD, getter, "Get" + name),
                                                 ("escritura", STORE, setter, "Set" + name)):
            clang = last_access(pattern, clang_table.get(cname))
            nsc = last_access(pattern, find_nsc(nsc_table, nname))
            result = verdict(clang, nsc)
            if result in ("FALTA", "DEFECTO"):
                failed = True
            lines.append(f"{key} {operation} {clang} {nsc} {result}")
    pathlib.Path(sys.argv[3]).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    if failed:
        return 1
    return 0


sys.exit(main())
