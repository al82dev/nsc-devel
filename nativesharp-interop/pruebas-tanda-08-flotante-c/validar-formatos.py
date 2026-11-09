"""T08-007-02 and T08-007-03: C floating-point types against Clang.

- 02a/02b: size and IEEE representation of `float` and `double`; bit patterns of values produced by each side
  (0.1, -0.0, the smallest subnormal, infinity and NaN), read through C.
- 02c: `long double` is IEEE binary64 on the fixed target (x86_64-pc-windows-msvc): same size and bits as
  `double`. A floating type without a NativeSharp carrier (`_Float16`) is diagnosed instead of being
  approximated, in a global and in a called function (parameter or return); the mapping rule for a
  `long double` that is not binary64 is pinned by CppClangSessionTests ("equal width alone does not establish
  floating-point representation").
- 03a: parameters and returns of each type, mixed with integers, and enough arguments to use the stack.
- 03b: fields (with an array of floats), globals written by each side and read by the other.
- 03c: the same body as a template of a precompiled producer consumed without the header.
Positive cases run in a Clang control, directly with the header and through the precompiled template, O0-O3.
Rejections run in O0.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-flotantes-", dir=r"A:\nsc-interop-build"))
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
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER = """typedef struct Floats { float f; double d; long double ld; char tag; float fa[2]; } Floats;
extern float g_f;
extern double g_d;
extern long double g_ld;
int size_f(void);
int size_d(void);
int size_ld(void);
int size_floats(void);
int offset_ld(void);
int offset_fa(void);
unsigned int f_bits(float v);
unsigned long long d_bits(double v);
unsigned long long ld_bits(long double v);
float f_from(unsigned int bits);
double d_from(unsigned long long bits);
float f_add(float a, float b);
double d_add(double a, double b);
long double ld_add(long double a, long double b);
double mix(int i, float a, double b, float c, long double d, long long e, float f);
double many(double a, double b, double c, double d, double e, double f, float g, long double h);
float f_neg_zero(void);
float f_min_subnormal(void);
double d_inf(void);
double d_nan(void);
void fill(Floats *s);
int check(const Floats *s);
void fill_globals(void);
int check_globals(void);
"""

SOURCE = """#include "api.h"
#include <stddef.h>
float g_f;
double g_d;
long double g_ld;
int size_f(void) { return (int)sizeof(float); }
int size_d(void) { return (int)sizeof(double); }
int size_ld(void) { return (int)sizeof(long double); }
int size_floats(void) { return (int)sizeof(Floats); }
int offset_ld(void) { return (int)offsetof(Floats, ld); }
int offset_fa(void) { return (int)offsetof(Floats, fa); }
unsigned int f_bits(float v) { unsigned int r; __builtin_memcpy(&r, &v, sizeof r); return r; }
unsigned long long d_bits(double v) { unsigned long long r; __builtin_memcpy(&r, &v, sizeof r); return r; }
unsigned long long ld_bits(long double v) { unsigned long long r; __builtin_memcpy(&r, &v, sizeof r); return r; }
float f_from(unsigned int bits) { float r; __builtin_memcpy(&r, &bits, sizeof r); return r; }
double d_from(unsigned long long bits) { double r; __builtin_memcpy(&r, &bits, sizeof r); return r; }
float f_add(float a, float b) { return a + b; }
double d_add(double a, double b) { return a + b; }
long double ld_add(long double a, long double b) { return a + b; }
double mix(int i, float a, double b, float c, long double d, long long e, float f)
{ return i + a + b + c + (double)d + (double)e + f; }
double many(double a, double b, double c, double d, double e, double f, float g, long double h)
{ return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * (double)h; }
float f_neg_zero(void) { return -0.0f; }
float f_min_subnormal(void) { return f_from(1u); }
double d_inf(void) { return d_from(0x7FF0000000000000ull); }
double d_nan(void) { return d_from(0x7FF8000000000000ull); }
void fill(Floats *s) { s->f = 1.5f; s->d = -2.25; s->ld = 1e300L; s->tag = 7; s->fa[0] = 0.5f; s->fa[1] = -0.0f; }
int check(const Floats *s)
{
    return s->f == 3.25f && s->d == 1e-300 && s->ld == -0.125L && s->tag == 9 && s->fa[0] == 0.1f &&
        f_bits(s->fa[1]) == 0x80000000u;
}
void fill_globals(void) { g_f = 0.1f; g_d = 0.1; g_ld = 0.1L; }
int check_globals(void) { return g_f == 2.5f && g_d == -1e10 && g_ld == 0.3L; }
"""

BODY = """    if (size_f() != 4 || size_d() != 8 || size_ld() != 8) return 1;
    if ((int)sizeof(float) != 4 || (int)sizeof(double) != 8) return 2;
    if (size_floats() != (int)sizeof(Floats) || offset_ld() != 16 || offset_fa() != 28) return 3;
    if (f_bits(0.1f) != 0x3DCCCCCD || d_bits(0.1) != 0x3FB999999999999A || ld_bits(0.1) != d_bits(0.1)) return 4;
    if (f_bits(-0.0f) != 0x80000000 || f_bits(f_neg_zero()) != 0x80000000) return 5;
    if (f_bits(f_min_subnormal()) != 1 || f_bits(1.401298464324817e-45f) != 1) return 6;
    if (f_from(0x3F800000) != 1.0f || d_from(0x4000000000000000) != 2.0) return 7;
    double inf = d_inf();
    double nan = d_nan();
    if (!(inf > 1e308) || d_bits(-inf) != 0xFFF0000000000000 || !(nan != nan)) return 8;
    if (f_add(0.1f, 0.2f) != 0.1f + 0.2f || d_add(0.1, 0.2) != 0.1 + 0.2 || ld_add(0.5, 0.25) != 0.75) return 9;
    if (f_bits(f_add(16777216.0f, 1.0f)) != f_bits(16777216.0f)) return 10;
    if (mix(1, 2.5f, 3.25, 4.5f, 5.0, 6, 0.25f) != 22.5) return 11;
    if (many(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0f, 0.5) != 32.0) return 12;
    Floats s;
    fill(&s);
    if (s.f != 1.5f || s.d != -2.25 || s.ld != 1e300 || s.tag != 7 || s.fa[0] != 0.5f) return 13;
    if (f_bits(s.fa[1]) != 0x80000000) return 14;
    s.f = 3.25f;
    s.d = 1e-300;
    s.ld = -0.125;
    s.tag = 9;
    s.fa[0] = 0.1f;
    s.fa[1] = -0.0f;
    if (check(&s) != 1) return 15;
    fill_globals();
    if (g_f != 0.1f || g_d != 0.1 || g_ld != 0.1 || d_bits(g_ld) != 0x3FB999999999999A) return 16;
    g_f = 2.5f;
    g_d = -1e10;
    g_ld = 0.3;
    if (check_globals() != 1) return 17;
    return 0;
"""


def c_spelling(text):
    for hexa in ("0x3DCCCCCD", "0x80000000", "0x3F800000"):
        text = text.replace(hexa, hexa + "u")
    for hexa in ("0x3FB999999999999A", "0x4000000000000000", "0xFFF0000000000000"):
        text = text.replace(hexa, hexa + "ull")
    return text


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder, extra_header=""):
    (folder / "api.h").write_text(HEADER + extra_header, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:cflot.lib", "api.obj"], folder)


HEADER_REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cflot.lib" />'


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def nsc_build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def report(label, result, code):
    if observe:
        print(label, result.returncode, code, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                               if "error" in l][:4], flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def build_and_run(label, folder, name, opt):
    result = nsc_build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode \
        if result.returncode == 0 else None
    report(label, result, code)


def control():
    folder = root / "control"
    folder.mkdir()
    c_library(folder)
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\nint _fltused = 0;\n'
        f"static int run(void)\n{{\n{c_spelling(BODY)}}}\n"
        "void mainCRTStartup(void) { ExitProcess(run()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "cflot.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    # Clang's own view of the format on this target.
    probe = folder / "formato.c"
    probe.write_text("#include <float.h>\n_Static_assert(sizeof(long double) == 8 && LDBL_MANT_DIG == 53, \"\");\n"
                     "_Static_assert(FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53, \"\");\n", encoding="utf-8")
    run([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only", probe.name], folder)
    print("control Clang correcto", flush=True)


def direct(opt):
    folder = root / f"directo-O{opt}"
    folder.mkdir()
    c_library(folder)
    (folder / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], HEADER_REF)
    build_and_run(f"directo O{opt}", folder, "Direct", opt)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "Producer.ns").write_text(
        "namespace Floats { public class Bridge { public static int Run<T>(T unused)\n{\n" + BODY + "} } }\n",
        encoding="utf-8")
    project(producer, "FloatBridge", ["Producer.ns"], HEADER_REF, library=True)
    result = nsc_build(producer, "FloatBridge", opt, "lib", "FloatBridge.lib")
    if result.returncode:
        report(f"productor O{opt}", result, None)
        return
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Floats.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/FloatBridge.lib" />')
    build_and_run(f"precompilado O{opt}", folder, "Consumer", opt)


REJECTIONS = [
    ("half-parametro", "int h_param(_Float16 v);\n", "return h_param(1.0f);",
     "C/C++ function 'h_param' cannot be imported"),
    ("half-retorno", "_Float16 h_value(int v);\n", "h_value(1); return 0;",
     "C/C++ function 'h_value' cannot be imported"),
    ("half-global", "extern _Float16 g_h;\n", "return (int)g_h;",
     "cannot import C/C++ global 'g_h': C/C++ global type is not yet mapped"),
    ("float-a-double-implicito", "", "float f = d_add(1.0, 2.0); return (int)f;", "cannot initialize local 'f'"),
]


def rejections():
    for name, extra, statements, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder, extra)
        (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
        project(folder, "Negative", ["Main.ns"], HEADER_REF)
        result = nsc_build(folder, "Negative", "0", "exe", "out/Negative.exe")
        if observe:
            print("rechazo", name, result.returncode, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                                        if "error" in l][:2], flush=True)
            continue
        if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{text}»:\n{result.stdout}")
        print("rechazo", name, "correcto", flush=True)


control()
for opt in opts:
    direct(opt)
    precompiled(opt)
rejections()
print("observación terminada" if observe else "Formatos flotantes C verificados:", root)
