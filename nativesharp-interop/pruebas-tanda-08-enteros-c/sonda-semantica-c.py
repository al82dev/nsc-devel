"""T08-D11/T08-D12 (T08-006-04b1, T08-009-03c1): C integer semantics against NativeSharp's, measured.

Research probe, not a rule change. Every case is evaluated by a Clang control and by NSC (direct path, O0 and O2);
each value is widened to 64 bits and printed through WriteFile by the C library, the same in both programs. The cases
cover unary `-` and `~` on narrow integers and `int` against `unsigned int`, with operands that are NativeSharp
values, C globals, C fields, C calls, a C typedef, C enumerators and C enum values, NativeSharp locals copied from C
values, mixed C and NativeSharp operands, and explicit casts.
The probe fails when a C or NSC value differs from the snapshot recorded on 2026-11-09 (EXPECTED); `--observar`
prints the table. The analysis is in registro-commits/t08-d11-d12-semantica-c-investigacion.md.
Sonda de investigacion T08-D11/T08-D12: semantica de enteros C frente a la de NativeSharp.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

observe = "--observar" in sys.argv
root = Path(tempfile.mkdtemp(prefix="t08-d11-d12-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"

HEADER = """typedef struct S { unsigned char ub; signed char sb; unsigned short us; short ss; int i; unsigned u; } S;
typedef enum E { EN = -1, EP = 1 } E;
typedef unsigned char u8_t;
extern unsigned char g_ub;
extern signed char g_sb;
extern unsigned short g_us;
extern short g_ss;
extern int g_i;
extern unsigned g_u;
extern unsigned g_two;
extern u8_t g_t8;
extern E g_e;
extern S g_s;
unsigned char f_ub(void);
signed char f_sb(void);
int f_i(void);
unsigned f_u(void);
void record(int index, long long value);
void flush(void);
"""
SOURCE = r"""#include "api.h"
__declspec(dllimport) void *__stdcall GetStdHandle(unsigned long);
__declspec(dllimport) int __stdcall WriteFile(void *, const void *, unsigned long, unsigned long *, void *);
unsigned char g_ub = 255;
signed char g_sb = -128;
unsigned short g_us = 65535;
short g_ss = -32768;
int g_i = -1;
unsigned g_u = 1;
unsigned g_two = 2;
u8_t g_t8 = 255;
E g_e = EN;
S g_s = {255, -128, 65535, -32768, -1, 1};
unsigned char f_ub(void) { return 255; }
signed char f_sb(void) { return -128; }
int f_i(void) { return -1; }
unsigned f_u(void) { return 1; }
static long long values[128];
static int used[128];
void record(int index, long long value) { values[index] = value; used[index] = 1; }
void flush(void)
{
    char text[64 * 128];
    unsigned long length = 0;
    for (int i = 0; i < 128; ++i)
    {
        if (!used[i]) continue;
        char digits[32];
        int n = 0;
        unsigned long long magnitude = values[i] < 0 ? 0ull - (unsigned long long)values[i]
                                                      : (unsigned long long)values[i];
        int index = i;
        char head[8];
        int h = 0;
        do { head[h++] = (char)('0' + index % 10); index /= 10; } while (index);
        while (h) text[length++] = head[--h];
        text[length++] = '=';
        if (values[i] < 0) text[length++] = '-';
        do { digits[n++] = (char)('0' + magnitude % 10); magnitude /= 10; } while (magnitude);
        while (n) text[length++] = digits[--n];
        text[length++] = '\n';
    }
    unsigned long written = 0;
    WriteFile(GetStdHandle((unsigned long)-11), text, length, &written, 0);
}
"""
# (label, NativeSharp statements before, NativeSharp expression, C statements before, C expression)
# Booleans are written `cond ? 1 : 0` in both languages.
# (etiqueta, sentencias NativeSharp previas, expresion NativeSharp, sentencias C previas, expresion C)
CASES = [
    # D11: unary - and ~ on narrow integers. ES: - y ~ unarios sobre enteros estrechos.
    ("nativo ~byte", "byte b = 255;", "~b", "unsigned char b = 255;", "~b"),
    ("nativo -sbyte", "sbyte b = -128;", "-b", "signed char b = -128;", "-b"),
    ("global C ~g_ub", "", "~g_ub", "", "~g_ub"),
    ("global C -g_sb", "", "-g_sb", "", "-g_sb"),
    ("global C ~g_us", "", "~g_us", "", "~g_us"),
    ("global C -g_ss", "", "-g_ss", "", "-g_ss"),
    ("campo C ~g_s.ub", "", "~g_s.ub", "", "~g_s.ub"),
    ("campo C -g_s.sb", "", "-g_s.sb", "", "-g_s.sb"),
    ("llamada C ~f_ub()", "", "~f_ub()", "", "~f_ub()"),
    ("llamada C -f_sb()", "", "-f_sb()", "", "-f_sb()"),
    ("typedef C ~g_t8", "", "~g_t8", "", "~g_t8"),
    ("var de global C ~t", "var t = g_ub;", "~t", "unsigned char t = g_ub;", "~t"),
    ("byte de global C ~t", "byte t = g_ub;", "~t", "unsigned char t = g_ub;", "~t"),
    ("conversion ~(byte)g_i", "", "~(byte)g_i", "", "~(unsigned char)g_i"),
    ("comparacion ~g_ub == 0", "", "~g_ub == 0 ? 1 : 0", "", "~g_ub == 0 ? 1 : 0"),
    ("mixto ~g_ub + byte", "byte b = 1;", "~g_ub + b", "unsigned char b = 1;", "~g_ub + b"),
    ("mixto -(g_sb) * byte", "byte b = 1;", "-(g_sb) * b", "unsigned char b = 1;", "-(g_sb) * b"),
    ("enum C -EP", "", "-EP", "", "-EP"),
    ("explicito ~(int)g_ub", "", "~(int)g_ub", "", "~(int)g_ub"),
    ("explicito -(int)g_sb", "", "-(int)g_sb", "", "-(int)g_sb"),
    # D12: int against unsigned int. ES: int frente a unsigned int.
    ("nativo int < uint", "int a = -1; uint b = 1;", "a < b ? 1 : 0", "int a = -1; unsigned b = 1;", "a < b ? 1 : 0"),
    ("globales C g_i < g_u", "", "g_i < g_u ? 1 : 0", "", "g_i < g_u ? 1 : 0"),
    ("global C g_i < 1u", "", "g_i < 1u ? 1 : 0", "", "g_i < 1u ? 1 : 0"),
    ("literal -1 < g_u", "", "-1 < g_u ? 1 : 0", "", "-1 < g_u ? 1 : 0"),
    ("local int < g_u", "int a = -1;", "a < g_u ? 1 : 0", "int a = -1;", "a < g_u ? 1 : 0"),
    ("var de C a < g_u", "var a = g_i;", "a < g_u ? 1 : 0", "int a = g_i;", "a < g_u ? 1 : 0"),
    ("campos C g_s.i < g_s.u", "", "g_s.i < g_s.u ? 1 : 0", "", "g_s.i < g_s.u ? 1 : 0"),
    ("llamadas C f_i() < f_u()", "", "f_i() < f_u() ? 1 : 0", "", "f_i() < f_u() ? 1 : 0"),
    ("enumerador EN < g_u", "", "EN < g_u ? 1 : 0", "", "EN < g_u ? 1 : 0"),
    ("enum C g_e < g_u", "", "g_e < g_u ? 1 : 0", "", "g_e < g_u ? 1 : 0"),
    ("division g_i / g_two", "", "g_i / g_two", "", "g_i / g_two"),
    ("producto g_i * g_u", "", "g_i * g_u", "", "g_i * g_u"),
    ("suma g_i + g_u > 0", "", "g_i + g_u > 0 ? 1 : 0", "", "g_i + g_u > 0 ? 1 : 0"),
    ("desplazamiento g_i >> 1 mixto", "", "(g_i * g_u) >> 1", "", "(g_i * g_u) >> 1"),
    ("modulo g_i % g_two", "", "g_i % g_two", "", "g_i % g_two"),
    ("explicito (uint)g_i < g_u", "", "(uint)g_i < g_u ? 1 : 0", "", "(unsigned)g_i < g_u ? 1 : 0"),
    ("explicito (uint)g_i / g_two", "", "(uint)g_i / g_two", "", "(unsigned)g_i / g_two"),
    ("explicito (uint)g_i * g_u", "", "(uint)g_i * g_u", "", "(unsigned)g_i * g_u"),
]

EXPECTED = {
    "nativo ~byte": ("-256", "0"),
    "nativo -sbyte": ("128", "-128"),
    "global C ~g_ub": ("-256", "0"),
    "global C -g_sb": ("128", "-128"),
    "global C ~g_us": ("-65536", "0"),
    "global C -g_ss": ("32768", "-32768"),
    "campo C ~g_s.ub": ("-256", "0"),
    "campo C -g_s.sb": ("128", "-128"),
    "llamada C ~f_ub()": ("-256", "0"),
    "llamada C -f_sb()": ("128", "-128"),
    "typedef C ~g_t8": ("-256", "0"),
    "var de global C ~t": ("-256", "0"),
    "byte de global C ~t": ("-256", "0"),
    "conversion ~(byte)g_i": ("-256", "0"),
    "comparacion ~g_ub == 0": ("0", "1"),
    "mixto ~g_ub + byte": ("-255", "1"),
    "mixto -(g_sb) * byte": ("128", "-128"),
    "enum C -EP": ("-1", "-1"),
    "explicito ~(int)g_ub": ("-256", "-256"),
    "explicito -(int)g_sb": ("128", "128"),
    "nativo int < uint": ("0", "1"),
    "globales C g_i < g_u": ("0", "1"),
    "global C g_i < 1u": ("0", "1"),
    "literal -1 < g_u": ("0", "1"),
    "local int < g_u": ("0", "1"),
    "var de C a < g_u": ("0", "1"),
    "campos C g_s.i < g_s.u": ("0", "1"),
    "llamadas C f_i() < f_u()": ("0", "1"),
    "enumerador EN < g_u": ("0", "1"),
    "enum C g_e < g_u": ("0", "0"),
    "division g_i / g_two": ("2147483647", "0"),
    "producto g_i * g_u": ("4294967295", "-1"),
    "suma g_i + g_u > 0": ("0", "0"),
    "desplazamiento g_i >> 1 mixto": ("2147483647", "-1"),
    "modulo g_i % g_two": ("1", "-1"),
    "explicito (uint)g_i < g_u": ("0", "0"),
    "explicito (uint)g_i / g_two": ("2147483647", "2147483647"),
    "explicito (uint)g_i * g_u": ("4294967295", "4294967295"),
}


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def library(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "-O0", "-fno-builtin", "-mno-stack-arg-probe",
         "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:sonda.lib", "api.obj"], folder)


def parse(text):
    values = {}
    for line in text.splitlines():
        if "=" in line and line.split("=")[0].isdigit():
            key, value = line.split("=", 1)
            values[int(key)] = value.strip()
    return values


c_folder = root / "c"
c_folder.mkdir()
library(c_folder)
body = "".join(f"    {{ {cpre} record({n}, (long long)({cexpr})); }}\n"
               for n, (_, _, _, cpre, cexpr) in enumerate(CASES))
(c_folder / "control.c").write_text(
    '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n'
    "void mainCRTStartup(void)\n{\n" + body + "    flush();\n    ExitProcess(0);\n}\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "-O0", "-Wno-everything", "control.c", "-o",
     "control.obj"], c_folder)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
     "control.obj", "sonda.lib", sdk / "um/x64/kernel32.lib"], c_folder)
c_values = parse(run([c_folder / "control.exe"], c_folder).stdout)

ns_values = {}
diagnostics = {}
for opt in ("0", "2"):
    folder = root / f"ns-O{opt}"
    folder.mkdir()
    library(folder)
    statements = "".join(f"    {{ {pre} record({n}, (long)({expr})); }}\n"
                         for n, (_, pre, expr, _, _) in enumerate(CASES))
    (folder / "M.ns").write_text("int Main()\n{\n" + statements + "    flush();\n    return 0;\n}\n", encoding="utf-8")
    (folder / "M.nsproj").write_text(
        '<Project><PropertyGroup><Name>M</Name></PropertyGroup><ItemGroup><Compile Include="M.ns" />'
        '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="sonda.lib" /></ItemGroup></Project>',
        encoding="utf-8")
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "M.nsproj", "-O" + opt,
                  "--no-artifact-cache", "-o", "out/M.exe"], folder, check=False)
    if result.returncode:
        diagnostics[opt] = [l for l in result.stdout.splitlines() if "error" in l][:12]
        continue
    ns_values[opt] = parse(run([folder / "out" / "M.exe"], folder).stdout)

for opt, lines in diagnostics.items():
    print(f"NSC O{opt} no compila:", *lines, sep="\n  ")
bad = []
print(f"{'caso':34} {'C':>12} {'NSC O0':>12} {'NSC O2':>12}  resultado")
for n, (label, *_rest) in enumerate(CASES):
    c = c_values.get(n, "?")
    o0 = ns_values.get("0", {}).get(n, "?")
    o2 = ns_values.get("2", {}).get(n, "?")
    verdict = "igual" if c == o0 == o2 else "DIFIERE"
    print(f"{label:34} {c:>12} {o0:>12} {o2:>12}  {verdict}")
    if (c, o0) != EXPECTED[label] or o0 != o2:
        bad.append(label)
if diagnostics or (bad and not observe):
    raise SystemExit(f"valores distintos de la instantanea del 2026-11-09: {bad}")
print("observación terminada" if observe else "Instantanea de semantica C/NativeSharp verificada:", root)
