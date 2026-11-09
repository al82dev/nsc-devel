"""T08-005-02: forms of C typedef.

a) Chains of scalar typedefs with different signedness and widths (casts, wrap-around, sign).
b) Pointer typedefs with `const`, `volatile` and `const volatile` pointees.
c) Typedef of a fixed array (global, local, record field, decaying parameter) and of a C function pointer.
d) Typedef of struct and union, of anonymous struct and union, and a record with an anonymous member.
e) Typedef of a named enum and of an anonymous enum.
Every body runs in a Clang control, directly with the header and through templates of a precompiled producer
consumed without the header (casts to the typedef names included), O0-O3. Rejections run in O0, each paired with
the Clang verdict for the same statement when C also rejects it.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-typedef-formas-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef signed char i8_t;
typedef i8_t small_t;
typedef unsigned short u16_t;
typedef u16_t word_t;
typedef long long i64_t;
typedef i64_t big_t;
typedef unsigned int u32_t;
typedef u32_t mask_t;
small_t neg_small(small_t v);
word_t next_word(word_t v);
big_t big_mul(big_t a, int b);
mask_t mask_not(mask_t v);
typedef const int *cint_ptr;
typedef volatile int *vint_ptr;
typedef const volatile int *cvint_ptr;
int read_c(cint_ptr p);
int read_v(vint_ptr p);
void write_v(vint_ptr p, int value);
int read_cv(cvint_ptr p);
cint_ptr get_c(void);
typedef int vec3[3];
typedef int (*binop)(int, int);
typedef struct Holder { vec3 items; binop op; } Holder;
extern vec3 g_vec;
int sum3(vec3 v);
int apply(binop f, int a, int b);
binop get_add(void);
int negate(int v);
int hold(Holder *h);
typedef struct Point { int x; int y; } Point;
typedef struct { int w; int h; } Size;
typedef union Bits { int i; unsigned u; } Bits;
typedef union { unsigned u; int i; } AnonBits;
typedef struct Tagged { int kind; union { int i; unsigned u; }; } Tagged;
Point make_point(int x, int y);
int area(Size s);
unsigned bits_u(Bits b);
int anon_i(AnonBits b);
int tagged_i(Tagged *t);
typedef enum Color { Red = 1, Green = 2, Blue = 4 } Color;
typedef enum { Low = 10, High = 20 } Level;
Color next_color(Color c);
int level_value(Level l);
"""

SOURCE = """#include "api.h"
small_t neg_small(small_t v) { return (small_t)-v; }
word_t next_word(word_t v) { return (word_t)(v + 1); }
big_t big_mul(big_t a, int b) { return a * b; }
mask_t mask_not(mask_t v) { return ~v; }
static const int constant = 42;
int read_c(cint_ptr p) { return *p; }
int read_v(vint_ptr p) { return *p; }
void write_v(vint_ptr p, int value) { *p = value; }
int read_cv(cvint_ptr p) { return *p; }
cint_ptr get_c(void) { return &constant; }
vec3 g_vec = {10, 20, 30};
int sum3(vec3 v) { return v[0] + v[1] + v[2]; }
int apply(binop f, int a, int b) { return f(a, b); }
static int add(int a, int b) { return a + b; }
binop get_add(void) { return add; }
int negate(int v) { return -v; }
int hold(Holder *h) { return h->op(h->items[0], h->items[2]); }
Point make_point(int x, int y) { Point p = {x, y}; return p; }
int area(Size s) { return s.w * s.h; }
unsigned bits_u(Bits b) { return b.u; }
int anon_i(AnonBits b) { return b.i; }
int tagged_i(Tagged *t) { return t->kind + t->i; }
Color next_color(Color c) { return c == Red ? Green : c == Green ? Blue : Red; }
int level_value(Level l) { return (int)l; }
"""

# Section bodies; NativeSharp spelling. The C control uses the same text after C_SPELLING.
BODIES = {
    "a": """    small_t a = (small_t)-3;
    if (neg_small(a) != 3) return 1;
    i8_t low = (i8_t)-128;
    if (neg_small(low) != (small_t)-128) return 2;
    word_t w = (word_t)65535;
    if (next_word(w) != 0 || next_word((u16_t)7) != 8) return 3;
    big_t b = big_mul((big_t)3000000000, 3);
    if (b != (i64_t)9000000000) return 4;
    mask_t k = mask_not((mask_t)0);
    if (k != (u32_t)4294967295 || mask_not(k) != 0) return 5;
    return 0;
""",
    "b": """    int x = 5;
    if (read_c(&x) != 5 || read_v(&x) != 5) return 1;
    write_v(&x, 9);
    if (x != 9 || read_cv(&x) != 9) return 2;
    cint_ptr p = get_c();
    if (*p != 42) return 3;
    cint_ptr q = &x;
    vint_ptr r = &x;
    cvint_ptr s = r;
    *r = 11;
    if (*q != 11 || *s != 11 || read_cv(q) != 11) return 4;
    return 0;
""",
    "c": """    if (g_vec[1] != 20 || sum3(&g_vec[0]) != 60) return 1;
    vec3 v;
    v[0] = 1;
    v[1] = 2;
    v[2] = 3;
    if (sum3(&v[0]) != 6) return 2;
    binop f = get_add();
    if (f(2, 3) != 5 || apply(f, 4, 5) != 9) return 3;
    Holder h;
    h.items[0] = 7;
    h.items[2] = 1;
    h.op = f;
    if (h.op(h.items[0], 1) != 8 || hold(&h) != 8) return 4;
    g_vec[2] = 31;
    if (sum3(&g_vec[0]) != 61) return 5;
    return 0;
""",
    "d": """    Point p = make_point(3, 4);
    if (p.x != 3 || p.y != 4) return 1;
    Size s;
    s.w = 5;
    s.h = 6;
    if (area(s) != 30) return 2;
    Bits b;
    b.i = -1;
    if (bits_u(b) != (u32_t)4294967295) return 3;
    AnonBits a;
    a.u = 7;
    if (anon_i(a) != 7) return 4;
    Tagged t;
    t.kind = 2;
    t.u = 5;
    if (tagged_i(&t) != 7 || t.i != 5) return 5;
    Point *pp = &p;
    pp->y = 10;
    if (p.y != 10) return 6;
    return 0;
""",
    "e": """    Color c = next_color(Red);
    if (c != Green || next_color(Blue) != Red) return 1;
    Color d = (Color)2;
    if (d != Green || next_color(d) != Blue) return 2;
    Level l = High;
    if (level_value(l) != 20 || level_value((Level)10) != 10) return 3;
    if ((int)Blue != 4 || (int)Low != 10) return 4;
    Color fromInt = 4;
    Level fromOther = Red;
    if (fromInt != Blue || (int)fromOther != 1) return 5;
    return 0;
""",
}


def c_spelling(body):
    return body.replace("(big_t)3000000000", "(big_t)3000000000LL").replace("(i64_t)9000000000",
                                                                             "(i64_t)9000000000LL")


# Rejections: (name, NativeSharp statements, expected text, whether Clang rejects the same C statements).
# NativeSharp is stricter than C when a pointer conversion drops a pointee qualifier (C only warns).
REJECTIONS = [
    ("b-quitar-const", "int *bad = get_c(); return *bad;",
     "initialization of mutable-pointee pointer 'bad' would discard pointee constness", False),
    ("b-quitar-volatile", "int x = 1; vint_ptr r = &x; int *bad = r; return *bad;",
     "initialization would discard pointer pointee volatile qualification", False),
    ("b-escribir-const", "cint_ptr p = get_c(); *p = 1; return 0;", "left side of assignment is not assignable", True),
    ("c-firma-distinta", "binop f = negate; return f(1, 2);",
     "no method in the method group exactly matches delegate signature", True),
    ("d-record-distinto", "Point p = make_point(1, 2); Size s = p; return s.w;",
     "cannot initialize local 's' of type 'type:Negative::Size' from 'type:Negative::Point'", True),
]


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
    run([lib, "/nologo", "/out:ctypedef.lib", "api.obj"], folder)


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


HEADER_REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="ctypedef.lib" />'


def nsc_build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def report(label, result, code):
    if observe:
        print(label, result.returncode, code, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                               if "error" in l][:3], flush=True)
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
    functions = "".join(f"static int section_{key}(void)\n{{\n{c_spelling(body)}}}\n" for key, body in BODIES.items())
    checks = " ".join(f"if ((code = section_{key}()) != 0) ExitProcess({index + 1} * 10 + code);"
                      for index, key in enumerate(BODIES))
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n' + functions +
        f"void mainCRTStartup(void) {{ int code; {checks} ExitProcess(0); }}\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "ctypedef.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: código {code}")
    for name, statements, _, c_rejects in REJECTIONS:
        probe = folder / f"rechazo-{name}.c"
        probe.write_text(f'#include "api.h"\nint probe(void) {{ {statements} }}\n', encoding="utf-8")
        result = run([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only", probe.name], folder,
                     check=False)
        if observe:
            print("Clang", name, result.returncode, flush=True)
        elif (result.returncode != 0) != c_rejects:
            raise AssertionError(f"Clang {name}: código {result.returncode}:\n{result.stdout}")
    print("control Clang correcto", flush=True)


def direct(opt):
    folder = root / f"directo-O{opt}"
    folder.mkdir()
    c_library(folder)
    functions = "".join(f"int Section{key.upper()}()\n{{\n{body}}}\n" for key, body in BODIES.items())
    checks = "".join(f"    if (Section{key.upper()}() != 0) return {index + 1};\n" for index, key in enumerate(BODIES))
    (folder / "Main.ns").write_text(functions + "int Main()\n{\n" + checks + "    return 0;\n}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], HEADER_REF)
    build_and_run(f"directo O{opt}", folder, "Direct", opt)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    methods = "".join(f"    public static int Section{key.upper()}<T>(T unused)\n    {{\n{body}    }}\n"
                      for key, body in BODIES.items())
    (producer / "Producer.ns").write_text("namespace Typedefs { public class Bridge {\n" + methods + "} }\n",
                                          encoding="utf-8")
    project(producer, "Typedefs", ["Producer.ns"], HEADER_REF, library=True)
    result = nsc_build(producer, "Typedefs", opt, "lib", "Typedefs.lib")
    if result.returncode:
        report(f"productor O{opt}", result, None)
        return
    (producer / "api.h").unlink()
    checks = "".join(f"    if (Typedefs.Bridge.Section{key.upper()}<int>(0) != 0) return {index + 1};\n"
                     for index, key in enumerate(BODIES))
    (folder / "Consumer.ns").write_text("int Main()\n{\n" + checks + "    return 0;\n}\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Typedefs.lib" />')
    build_and_run(f"precompilado O{opt}", folder, "Consumer", opt)


def rejections():
    for name, statements, text, _ in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder)
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
print("observación terminada" if observe else "Formas de typedef C verificadas:", root)
