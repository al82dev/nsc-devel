"""Compares the alignment of every C field access emitted by NSC with the access emitted by Clang.

Usage: verificar_alineacion_clang.py <clang -O1 reference .ll> <nsc project .ll> <report .txt> [table]

The optional table has one access per line: <key> <clang getter> <clang setter> <NSC method suffix> <0|1>,
where the last column marks a bit-field. Without it, the built-in ACCESSES list below is used with the
acc_get_<key>/acc_set_<key> Clang names of pruebas-tanda-08-layout-clang.

For each entry of ACCESSES the Clang reference defines acc_get_<key> and acc_set_<key>; the NativeSharp
project defines Access.Get<Name> and Access.Set<Name>. The field access is the last load of the getter
and the last store of the setter. Verdicts for ordinary fields:
  IGUAL        NSC alignment == min(Clang alignment, natural alignment of the IR type)
  SUBALINEADO  NSC alignment is lower: always safe, recorded as evidence
  DEFECTO      NSC alignment is higher than Clang's, or the accessed IR type differs
Bit-fields are compared only informatively (their storage windows may legitimately differ); their
bytes are checked at runtime by the fixture through a byte comparison against Clang-compiled writes.
Exit code 1 when a function is missing or any DEFECTO is found.
"""
import pathlib
import re
import sys

ACCESSES = [
    ("lnatural_d", "LNaturalD", False),
    ("lnatural_s", "LNaturalS", False),
    ("lpack4_d", "LPack4D", False),
    ("lpack4_s", "LPack4S", False),
    ("lpack2_d", "LPack2D", False),
    ("lpack2_s", "LPack2S", False),
    ("lpack1_d", "LPack1D", False),
    ("lpack1_s", "LPack1S", False),
    ("louter_a", "LOuterA", False),
    ("louter_b", "LOuterB", False),
    ("lhost_v", "LHostV", False),
    ("lhost_z", "LHostZ", False),
    ("lpack2array_a", "LPack2ArrayA", False),
    ("lpack2array_b", "LPack2ArrayB", False),
    ("lalignedfield_x", "LAlignedFieldX", False),
    ("lalignedtypedef_x", "LAlignedTypedefX", False),
    ("lundertypedef_x", "LUnderTypedefX", False),
    ("lunion_d", "LUnionD", False),
    ("lunion_i", "LUnionI", False),
    ("lpackedunion_d", "LPackedUnionD", False),
    ("lpackedholdsunion_d", "LPackedHoldsUnionD", False),
    ("lpackedholdsunion_i", "LPackedHoldsUnionI", False),
    ("lunionholdsstruct_b", "LUnionHoldsStructB", False),
    ("lunionholdsstruct_ll", "LUnionHoldsStructLl", False),
    ("lbits_a", "LBitsA", True),
    ("lbits_b", "LBitsB", True),
    ("lbits_c", "LBitsC", True),
    ("lpackedbits_a", "LPackedBitsA", True),
    ("lpackedbits_b", "LPackedBitsB", True),
    ("lbitsunion_a", "LBitsUnionA", True),
    ("lbitsunion_b", "LBitsUnionB", True),
]

NATURAL = {"i8": 1, "i16": 2, "i32": 4, "i64": 8, "float": 4, "double": 8, "ptr": 8}
DEFINE = re.compile(r'^define [^@]*@("(?:[^"]+)"|[\w.$]+)\(', re.MULTILINE)
LOAD = re.compile(r"= load (?:volatile )?([\w]+), ptr [^,\n]+, align (\d+)")
STORE = re.compile(r"store (?:volatile )?([\w]+) [^,\n]+, ptr [^,\n]+, align (\d+)")


def functions(text):
    """Returns a map from function name (quotes removed) to its body text."""
    result = {}
    matches = list(DEFINE.finditer(text))
    for index, match in enumerate(matches):
        end = text.find("\n}", match.end())
        result[match.group(1).strip('"')] = text[match.end():end if end >= 0 else len(text)]
    return result


def find_clang(table, name):
    """Returns the body of an exact Clang function name, or None."""
    return table.get(name)


def find_nsc(table, name):
    """Returns the body of the single NSC method Access.<name> (mangled as Access__<name>__...), or None."""
    pattern = re.compile(r"Access__" + re.escape(name) + r"__")
    found = [body for key, body in table.items() if pattern.search(key)]
    return found[0] if len(found) == 1 else None


def last(pattern, body):
    """Returns (IR type, alignment) of the last match of pattern in body, or None."""
    matches = pattern.findall(body or "")
    return (matches[-1][0], int(matches[-1][1])) if matches else None


def verdict(clang, nsc, bitfield):
    """Classifies one access comparison."""
    if clang is None or nsc is None:
        return "FALTA"
    if bitfield:
        return "INFORMATIVO"
    if clang[0] != nsc[0]:
        return "DEFECTO"
    expected = min(clang[1], NATURAL.get(clang[0], clang[1]))
    if nsc[1] > clang[1]:
        return "DEFECTO"
    return "IGUAL" if nsc[1] == expected else "SUBALINEADO"


def access_table():
    """Returns (key, clang getter, clang setter, NSC suffix, bitfield) rows from argv[4] or ACCESSES."""
    if len(sys.argv) < 5:
        return [(key, "acc_get_" + key, "acc_set_" + key, name, bitfield) for key, name, bitfield in ACCESSES]
    rows = []
    for line in pathlib.Path(sys.argv[4]).read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 5:
            rows.append((parts[0], parts[1], parts[2], parts[3], parts[4] == "1"))
    return rows


def main():
    """Runs the comparison and writes the report."""
    clang_table = functions(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
    nsc_table = functions(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
    lines = ["acceso operacion clang nsc veredicto"]
    failed = False
    for key, getter, setter, name, bitfield in access_table():
        for operation, pattern, cname, nname in (("lectura", LOAD, getter, "Get" + name),
                                                 ("escritura", STORE, setter, "Set" + name)):
            clang = last(pattern, find_clang(clang_table, cname))
            nsc = last(pattern, find_nsc(nsc_table, nname))
            result = verdict(clang, nsc, bitfield)
            failed |= result in ("FALTA", "DEFECTO")
            lines.append(f"{key} {operation} {clang} {nsc} {result}")
    pathlib.Path(sys.argv[3]).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 1 if failed else 0


sys.exit(main())
