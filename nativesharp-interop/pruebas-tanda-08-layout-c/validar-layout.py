"""T08-012-02 and T08-012-03: layout of C records against Clang.

03a) Offsets and alignment of scalar fields in a natural struct and under `pack(1)`, `pack(2)` and `pack(4)`.
03b) Arrays of records (natural and packed) and records nested in a packed record.
03c) Bit fields with and without a name, zero-width bit fields and the access windows NSC uses for them.
03d) `_Alignas` on a field and typedefs with `aligned(8)` and `aligned(1)`.
03e) The same layout restored from metadata: a consumer without the header measures the published types with
     `sizeof`, `alignof` and `offsetof` in its own code, and runs the accesses through a precompiled template.
02)  Effective access alignment: for every scalar access, the alignment of the `load` and `store` that NSC emits
     at -O1 must equal Clang's (T08-D15; it was `min(Clang alignment, natural alignment of the IR type)`).
     Bit-field windows are reported (they may be narrower than Clang's) and their values are checked by C.
Layout values come from `sizeof`, `_Alignof`, `offsetof` and `__alignof__` compiled by Clang. Every access is a
getter and a setter in NativeSharp and in C; NativeSharp writes through its setter and C reads the value back, and
C writes and NativeSharp reads. Positive cases run in O0-O3.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-layout-c-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER = """typedef struct N { char c; short s; int i; long long ll; float f; double d; char tail; } N;
#pragma pack(push, 1)
typedef struct P1 { char c; short s; int i; long long ll; double d; } P1;
#pragma pack(pop)
#pragma pack(push, 2)
typedef struct P2 { char c; short s; int i; long long ll; double d; } P2;
#pragma pack(pop)
#pragma pack(push, 4)
typedef struct P4 { char c; short s; int i; long long ll; double d; } P4;
#pragma pack(pop)
typedef struct In { char c; int i; } In;
typedef struct Arr { short h; In items[3]; P1 packed[2]; } Arr;
#pragma pack(push, 1)
typedef struct Nest { char a; In inner; N whole; } Nest;
#pragma pack(pop)
typedef struct Bits {
    unsigned a : 3; unsigned : 2; unsigned b : 5; unsigned : 0; unsigned char c : 4; int d : 7; long long e : 33;
} Bits;
typedef struct Al { char c; _Alignas(16) int x; short y; } Al;
typedef int __attribute__((aligned(8))) Int8;
typedef int __attribute__((aligned(1))) Int1;
typedef struct AT { char c; Int8 a; char d; Int1 b; } AT;
int layout(int which);
"""
TYPES = ["N", "P1", "P2", "P4", "In", "Arr", "Nest", "Bits", "Al", "AT"]
OFFSETS = [("N", "s"), ("N", "ll"), ("N", "d"), ("N", "tail"), ("P1", "s"), ("P1", "ll"), ("P1", "d"),
           ("P2", "i"), ("P2", "ll"), ("P4", "ll"), ("P4", "d"), ("Arr", "items"), ("Arr", "packed"),
           ("Nest", "inner"), ("Nest", "whole"), ("Al", "x"), ("Al", "y"), ("AT", "a"), ("AT", "d"), ("AT", "b")]
LAYOUT = [(f"sizeof({t})", f"sizeof({t})") for t in TYPES] + \
    [(f"_Alignof({t})", f"alignof({t})") for t in TYPES] + \
    [(f"offsetof({t}, {f})", f"offsetof({t}, {f})") for t, f in OFFSETS]
# (record, access, NativeSharp type, C type, bit field, value)
ACCESSES = [
    ("N", "c", "sbyte", "char", False, "-5"), ("N", "s", "short", "short", False, "-300"),
    ("N", "i", "int", "int", False, "70000"), ("N", "ll", "long", "long long", False, "-5000000000"),
    ("N", "f", "float", "float", False, "1.5f"), ("N", "d", "double", "double", False, "-2.25"),
    ("P1", "s", "short", "short", False, "1234"), ("P1", "i", "int", "int", False, "-77777"),
    ("P1", "ll", "long", "long long", False, "6000000000"), ("P1", "d", "double", "double", False, "3.5"),
    ("P2", "i", "int", "int", False, "88888"), ("P2", "ll", "long", "long long", False, "-7000000000"),
    ("P2", "d", "double", "double", False, "0.75"), ("P4", "ll", "long", "long long", False, "8000000000"),
    ("P4", "d", "double", "double", False, "-0.5"),
    ("Arr", "h", "short", "short", False, "-9"), ("Arr", "items[1].i", "int", "int", False, "424242"),
    ("Arr", "items[2].c", "sbyte", "char", False, "33"), ("Arr", "packed[1].ll", "long", "long long", False,
                                                            "9000000001"),
    ("Arr", "packed[0].s", "short", "short", False, "-1111"),
    ("Nest", "a", "sbyte", "char", False, "12"), ("Nest", "inner.i", "int", "int", False, "-31337"),
    ("Nest", "whole.d", "double", "double", False, "6.25"), ("Nest", "whole.s", "short", "short", False, "777"),
    ("Al", "x", "int", "int", False, "16161"), ("Al", "y", "short", "short", False, "-16"),
    ("AT", "a", "int", "int", False, "818181"), ("AT", "b", "int", "int", False, "-121212"),
    ("Bits", "a", "uint", "unsigned", True, "5"), ("Bits", "b", "uint", "unsigned", True, "31"),
    ("Bits", "c", "byte", "unsigned char", True, "9"), ("Bits", "d", "int", "int", True, "-64"),
    ("Bits", "e", "long", "long long", True, "-4294967296"),
]


def c_literal(value, ctype):
    if ctype == "long long" and not value.endswith("LL"):
        return value + "LL"
    return value


SOURCE = '#include <stddef.h>\n#include "api.h"\nint layout(int which)\n{\n    switch (which) {\n' + \
    "".join(f"    case {k}: return (int){c};\n" for k, (c, _) in enumerate(LAYOUT)) + \
    "    }\n    return -1;\n}\n" + \
    "".join(f"{ct} cget_{k}(const {r} *p) {{ return p->{a}; }}\n"
            f"void cset_{k}({r} *p, {ct} v) {{ p->{a} = v; }}\n"
            for k, (r, a, _, ct, _, _) in enumerate(ACCESSES))
HEADER_FULL = HEADER + "".join(f"{ct} cget_{k}(const {r} *p);\nvoid cset_{k}({r} *p, {ct} v);\n"
                               for k, (r, a, _, ct, _, _) in enumerate(ACCESSES))

GETTERS = "".join(f"    public static {nt} Get_{k:03}_({r}* p) {{ return p->{a}; }}\n"
                  f"    public static void Set_{k:03}_({r}* p, {nt} v) {{ p->{a} = v; }}\n"
                  for k, (r, a, nt, _, _, _) in enumerate(ACCESSES))
def layout_body(source):
    return "".join(f"    if ((int){ns} != {source}({k})) return {k + 1};\n" for k, (_, ns) in enumerate(LAYOUT))


def ns_literal(value, nstype):
    return value + "L" if nstype == "long" and abs(int(value)) > 2147483647 else value


def access_body(owner):
    lines = []
    for k, (r, a, nt, _, _, value) in enumerate(ACCESSES):
        code = 100 + k
        v = ns_literal(value, nt)
        lines.append(f"    {{ {r} o; {owner}Set_{k:03}_(&o, ({nt}){v}); if (cget_{k}(&o) != ({nt}){v}) return {code};"
                     f" cset_{k}(&o, ({nt})1);\n      if ({owner}Get_{k:03}_(&o) != ({nt})1) return {code}; }}\n")
    return "".join(lines)


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(HEADER_FULL, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:clayout.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="clayout.lib" />'


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:4]


def expect_run(label, folder, name, opt):
    result = build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode \
        if result.returncode == 0 else None
    if observe:
        print(label, result.returncode, code, errors(result), flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def function_blocks(text):
    blocks = {}
    current = None
    for line in text.splitlines():
        if line.startswith("define "):
            current = []
            blocks[line] = current
        elif line.startswith("}"):
            current = None
        elif current is not None:
            current.append(line)
    return blocks


def access_alignment(blocks, token, kind):
    for header, body in blocks.items():
        if token in header:
            for line in reversed(body):
                match = re.search(r"\b" + kind + r" (?:volatile )?(i\d+|float|double)(?:, ptr [^,]+)?, align (\d+)",
                                  line) if kind == "load" else re.search(
                    r"\bstore (?:volatile )?(i\d+|float|double) [^,]+, ptr [^,]+, align (\d+)", line)
                if match:
                    return match.group(1), int(match.group(2))
            return None
    return None


def compare_ir():
    folder = root / "ir"
    folder.mkdir()
    c_library(folder)
    run([clang, "-S", "-emit-llvm", "-O1", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o",
         "c.ll"], folder)
    (folder / "Main.ns").write_text("namespace L { public class A {\n" + GETTERS + "} }\nint Main() { return 0; }\n",
                                    encoding="utf-8")
    project(folder, "Ir", "Main.ns", REF)
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", "Ir.nsproj", "-O1"],
                 folder, check=False)
    if result.returncode:
        raise AssertionError(f"IR NSC:\n{result.stdout}")
    c_blocks = function_blocks((folder / "c.ll").read_text(encoding="utf-8"))
    ns_blocks = function_blocks(result.stdout)
    report = []
    defects = []
    for k, (r, a, nt, _, bit, _) in enumerate(ACCESSES):
        for kind, c_name, ns_name in (("load", f"@cget_{k}(", f"Get_{k:03}_"), ("store", f"@cset_{k}(",
                                                                                  f"Set_{k:03}_")):
            c_access = access_alignment(c_blocks, c_name, kind)
            ns_access = access_alignment(ns_blocks, ns_name, kind)
            if not c_access or not ns_access:
                defects.append(f"{r}.{a} {kind}: sin acceso (clang {c_access}, nsc {ns_access})")
                continue
            if bit:
                verdict = "VENTANA" if ns_access[1] <= c_access[1] else "DEFECTO"
            else:
                # T08-D15: Clang's alignment, also above the natural one (it was min(Clang, natural)).
                # T08-D15: la alineacion de Clang, tambien por encima de la natural (era min(Clang, natural)).
                expected = c_access[1]
                verdict = "IGUAL" if ns_access == (c_access[0], expected) else "DEFECTO"
            report.append(f"{r}.{a} {kind} clang={c_access} nsc={ns_access} {verdict}")
            if verdict == "DEFECTO":
                defects.append(report[-1])
    (root / "alineacion.txt").write_text("\n".join(report) + "\n", encoding="utf-8")
    if observe:
        print("\n".join(report), flush=True)
        print("defectos", defects, flush=True)
        return
    if defects:
        raise AssertionError("alineación de acceso distinta de Clang:\n" + "\n".join(defects))
    print("IR de acceso correcto:", len(report), "accesos;", sum(" VENTANA" in l for l in report),
          "ventanas de campos de bits", flush=True)


def paths(opt):
    direct = root / f"directo-O{opt}"
    direct.mkdir()
    c_library(direct)
    (direct / "Main.ns").write_text("namespace L { public class A {\n" + GETTERS + "} }\nint Main()\n{\n" +
                                    layout_body("layout") + access_body("L.A.") + "    return 0;\n}\n",
                                    encoding="utf-8")
    project(direct, "Direct", "Main.ns", REF)
    expect_run(f"directo O{opt}", direct, "Direct", opt)
    base = root / f"precompilado-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text("namespace L { public class A {\n" + GETTERS +
                                   "    public static int Layout(int which) { return layout(which); }\n" +
                                   "    public static int Run<T>(T unused)\n    {\n" + access_body("") +
                                   "        return 0;\n    }\n} }\n", encoding="utf-8")
    project(producer, "LayoutPub", "P.ns", REF, library=True)
    result = build(producer, "LayoutPub", opt, "lib", "LayoutPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    consumer = "int Main()\n{\n" + layout_body("L.A.Layout") + "    return L.A.Run<int>(0);\n}\n"
    (base / "C.ns").write_text(consumer,
                               encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/LayoutPub.lib" />')
    expect_run(f"solo metadata O{opt}", base, "C", opt)


compare_ir()
for opt in opts:
    paths(opt)
print("observación terminada" if observe else "Layout C verificado contra Clang:", root)
