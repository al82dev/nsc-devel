"""T08-D11/T08-D12: diagnostic of C integer operations whose NativeSharp result differs from C.

NativeSharp keeps its own integer rules in every expression (no semantic change by the origin of a value). When an
operand is read directly from C (a C global or enumerator, a C record field, a C function result, or an element or
pointee reached from one of them) and the operation gives a different result than C, NSC reports a warning with the
explicit correction:
- T08-D11: unary `-` or `~` on a `byte`, `sbyte`, `short` or `ushort` C operand: C promotes it to `int`;
  correction `~(int)x`;
- T08-D12: comparison or arithmetic of `int` with `uint` that NativeSharp widens to `long`, with a C operand: C
  converts the `int` operand to `unsigned int`; correction `(uint)` before the `int` operand.
Checked: every case that must warn warns on its own line and the others do not (native operands, explicit
corrections, a C enum value that already follows C, constants NativeSharp adapts to `uint`, and the documented
limitation of a C value copied to a local); the warning does not stop the build; the program runs at O0-O3 with
the NativeSharp values; the corrected forms give the C values; a precompiled library warns when it is built; a
precompiled generic template consumed without the header compiles, runs and warns where it is instantiated, on the
lines of the transported template source.
T08-D11/T08-D12: diagnostico de operaciones enteras C cuyo resultado NativeSharp difiere del de C.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-divergencia-c-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef struct S { unsigned char ub; signed char sb; int i; unsigned u; } S;
typedef enum E { EN = -1, EP = 1 } E;
typedef unsigned char u8_t;
extern unsigned char g_ub;
extern signed char g_sb;
extern short g_ss;
extern int g_i;
extern unsigned g_u;
extern unsigned g_two;
extern u8_t g_t8;
extern E g_e;
extern S g_s;
extern unsigned char g_bytes[2];
extern unsigned char *g_pub;
unsigned char f_ub(void);
int f_i(void);
unsigned f_u(void);
"""
SOURCE = """#include "api.h"
unsigned char g_ub = 255;
signed char g_sb = -128;
short g_ss = -32768;
int g_i = -1;
unsigned g_u = 1;
unsigned g_two = 2;
u8_t g_t8 = 255;
E g_e = EN;
S g_s = {255, -128, -1, 1};
unsigned char g_bytes[2] = {255, 254};
unsigned char *g_pub = &g_bytes[0];
unsigned char f_ub(void) { return 255; }
int f_i(void) { return -1; }
unsigned f_u(void) { return 1; }
"""
# (statement, warns, NativeSharp value): each case is one line of Run, which returns the 1-based number of the
# first case whose value differs, or 0.
# (sentencia, avisa, valor NativeSharp): cada caso es una linea de Run, que devuelve el numero (desde 1) del
# primer caso cuyo valor difiere, o 0.
CASES = [
    ("v = ~g_ub;", True, 0),
    ("v = -g_sb;", True, -128),
    ("v = -g_ss;", True, -32768),
    ("v = ~g_s.ub;", True, 0),
    ("v = -g_s.sb;", True, -128),
    ("v = ~f_ub();", True, 0),
    ("v = ~g_t8;", True, 0),
    ("v = ~g_bytes[1];", True, 1),
    ("v = ~*g_pub;", True, 0),
    ("v = g_i < g_u ? 1 : 0;", True, 1),
    ("v = g_i < 1u ? 1 : 0;", True, 1),
    ("v = -1 < g_u ? 1 : 0;", True, 1),
    ("{ int a = -1; v = a < g_u ? 1 : 0; }", True, 1),
    ("v = EN < g_u ? 1 : 0;", True, 1),
    ("v = g_s.i < g_s.u ? 1 : 0;", True, 1),
    ("v = f_i() < f_u() ? 1 : 0;", True, 1),
    ("v = g_i / g_two;", True, 0),
    ("v = g_i * g_u;", True, -1),
    ("v = g_i % g_two;", True, -1),
    ("{ byte b = 255; v = ~b; }", False, 0),
    ("{ int a = -1; uint b = 1; v = a < b ? 1 : 0; }", False, 1),
    ("v = ~(int)g_ub;", False, -256),
    ("v = -(int)g_sb;", False, 128),
    ("v = (uint)g_i < g_u ? 1 : 0;", False, 0),
    ("v = (uint)g_i / g_two;", False, 2147483647),
    ("v = (uint)g_i * g_u;", False, 4294967295),
    ("v = g_e < g_u ? 1 : 0;", False, 0),
    ("v = g_u > 0 ? 1 : 0;", False, 1),
    ("v = g_u + 1;", False, 2),
    ("{ var t = g_ub; v = ~t; }", False, 0),
]
FIRST_LINE = 5


def program(namespace, generic):
    head = "public static long Run<T>(T unused)\n{\n" if generic else "public static long Run()\n{\n"
    text = f"namespace {namespace} {{ public class P {{\n" + head + "    long v = 0;\n"
    for n, (statement, _, value) in enumerate(CASES):
        text += f"    {statement} if (v != {value}L) return {n + 1};\n"
    return text + "    return 0;\n}\n} }\n"


def failed_case(code):
    return CASES[code - 1][0] if 0 < code <= len(CASES) else f"salida {code}"


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:div.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="div.lib" />'


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def warned_lines(text, source):
    lines = set()
    for line in text.splitlines():
        match = re.search(re.escape(source) + r":(\d+):\d+: warning: ", line)
        if match and ("C value of type" in line or "with 'uint' and a C operand" in line):
            lines.add(int(match.group(1)))
    return lines


def check_warnings(label, text, source, shifted=False):
    got = warned_lines(text, source)
    first = FIRST_LINE
    if shifted and got:
        # EN: the transported template source has its own prologue; the cases keep their order and spacing.
        # ES: la fuente de plantilla transportada tiene su propio prologo; los casos conservan orden y espaciado.
        first = min(got)
    expected = {first + n for n, (_, warns, _) in enumerate(CASES) if warns}
    if observe:
        print(label, "avisos en lineas", sorted(got), "esperadas", sorted(expected), flush=True)
        return
    if got != expected:
        missing = [CASES[l - first][0] for l in sorted(expected - got)]
        extra = [CASES[l - first][0] for l in sorted(got - expected) if 0 <= l - first < len(CASES)]
        raise AssertionError(f"{label}: faltan avisos {missing}, sobran {extra}:\n{text}")
    print(label, "avisos correctos", flush=True)


def direct():
    for opt in opts:
        folder = root / f"directo-O{opt}"
        folder.mkdir()
        c_library(folder)
        (folder / "M.ns").write_text(program("Div", False) +
                                     "int Main() { return (int)Div.P.Run(); }\n",
                                     encoding="utf-8")
        project(folder, "M", "M.ns", REF)
        result = build(folder, "M", opt, "exe", "out/M.exe")
        if result.returncode:
            raise AssertionError(f"directo O{opt}: el aviso no debe impedir la construccion:\n{result.stdout}")
        if opt == opts[0]:
            check_warnings(f"directo O{opt}", result.stdout, "M.ns")
        code = run([folder / "out" / "M.exe"], folder, check=False).returncode
        if observe:
            print("ejecucion directa", opt, code, flush=True)
        elif code:
            raise AssertionError(f"directo O{opt}: valor NativeSharp distinto en {failed_case(code)}")
        else:
            print(f"ejecucion directa O{opt} correcta", flush=True)


def precompiled_library():
    producer = root / "productor-no-generico"
    producer.mkdir()
    c_library(producer)
    (producer / "P.ns").write_text(program("DivLib", False), encoding="utf-8")
    project(producer, "DivLib", "P.ns", REF, library=True)
    result = build(producer, "DivLib", "0", "lib", "DivLib.lib")
    if result.returncode:
        raise AssertionError(f"productor no generico:\n{result.stdout}")
    check_warnings("productor precompilado no generico", result.stdout, "P.ns")


def precompiled():
    producer = root / "productor"
    producer.mkdir()
    c_library(producer)
    (producer / "P.ns").write_text(program("DivPub", True), encoding="utf-8")
    project(producer, "DivPub", "P.ns", REF, library=True)
    result = build(producer, "DivPub", "0", "lib", "DivPub.lib")
    if result.returncode:
        raise AssertionError(f"productor:\n{result.stdout}")
    (producer / "api.h").unlink()
    for opt in ("0", "2"):
        consumer = root / f"consumidor-O{opt}"
        consumer.mkdir()
        (consumer / "C.ns").write_text("int Main() { return (int)DivPub.P.Run<int>(0); }\n",
                                       encoding="utf-8")
        project(consumer, "C", "C.ns", '<LibraryReference Include="../productor/DivPub.lib" />')
        built = build(consumer, "C", opt, "exe", "out/C.exe")
        if built.returncode:
            raise AssertionError(f"consumidor O{opt}:\n{built.stdout}")
        if opt == "0":
            check_warnings("consumidor de la plantilla", built.stdout, "DivPub.nsmeta.public-templates.ns", True)
        code = run([consumer / "out" / "C.exe"], consumer, check=False).returncode
        if observe:
            print("consumidor", opt, code, flush=True)
        elif code:
            raise AssertionError(f"consumidor O{opt}: valor NativeSharp distinto en {failed_case(code)}")
        else:
            print(f"consumidor precompilado O{opt} correcto", flush=True)


direct()
precompiled_library()
precompiled()
print("observación terminada" if observe else "Diagnostico de divergencia entera C verificado:", root)
