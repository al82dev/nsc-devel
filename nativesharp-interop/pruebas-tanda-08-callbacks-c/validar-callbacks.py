"""T08-017-03 to T08-017-05: C function pointers (callbacks).

03a) A callback with scalar parameters and result, called from NativeSharp and passed to C.
03b) A callback with a record pointer and an enum in its signature.
03c) A variadic C callback: its identity survives a NativeSharp variable and C calls it.
03d) A `__vectorcall` callback: called directly, through a NativeSharp variable and by C (T08-D22; the full
     vectorcall ABI is in validar-vectorcall.py).
04a) Callbacks stored in C struct fields. C checks a callback written by NativeSharp by calling it: a C function
     assigned in NativeSharp is stored as a boundary thunk, so its address differs from C's (pending decision
     T08-017-04a1).
04b) Callbacks stored in C globals (`BinOp` and `volatile BinOp`).
04c) Callbacks returned by C functions, also called directly on the result.
04d) A `volatile` callback field, aligned and inside a `pack(1)` struct: read, called and written.
04e) Writing a `const` callback field is rejected.
05a-c) The signatures and conventions published in metadata: the same body runs in a template of a precompiled
       producer consumed without the header (05a/05c) and with the header (05b).
The body also runs in a Clang control. Positive cases run in O0-O3; rejections in O0.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-callbacks-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef enum Kind { KOne = 1, KTwo = 2 } Kind;
typedef struct Item { int v; Kind k; } Item;
typedef int (*BinOp)(int, int);
typedef int (*ItemFn)(Item *, Kind);
typedef Item *(*MakeFn)(int);
typedef int (*VarFn)(int, ...);
typedef double (__vectorcall *VecFn)(double, double);
typedef struct Slots { BinOp op; ItemFn item; volatile BinOp vop; const BinOp cop; } Slots;
#pragma pack(push, 1)
typedef struct PSlots { char c; volatile BinOp vop; BinOp op; } PSlots;
#pragma pack(pop)
extern BinOp g_op;
extern volatile BinOp g_vop;
int add(int a, int b);
int mul(int a, int b);
int item_fn(Item *i, Kind k);
Item *make_item(int v);
int sum_var(int count, ...);
double __vectorcall vec_add(double a, double b);
int apply(BinOp op, int a, int b);
int apply_item(ItemFn fn, Item *i, Kind k);
int apply_var(VarFn fn);
double apply_vec(VecFn fn, double a, double b);
BinOp pick(int which);
MakeFn maker(void);
void fill_slots(Slots *s);
int check_slots(const Slots *s);
void fill_packed(PSlots *p);
int check_packed(const PSlots *p);
"""
SOURCE = """#include <stdarg.h>
#include "api.h"
int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
BinOp g_op = add;
volatile BinOp g_vop = mul;
int item_fn(Item *i, Kind k) { return i->v * (int)k + (int)i->k; }
static Item items[4];
Item *make_item(int v) { items[v & 3].v = v; items[v & 3].k = KTwo; return &items[v & 3]; }
int sum_var(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int s = 0;
    for (int i = 0; i < count; ++i) s += va_arg(ap, int);
    va_end(ap);
    return s;
}
double __vectorcall vec_add(double a, double b) { return a + b; }
int apply(BinOp op, int a, int b) { return op(a, b); }
int apply_item(ItemFn fn, Item *i, Kind k) { return fn(i, k); }
int apply_var(VarFn fn) { return fn(3, 1, 2, 3); }
double apply_vec(VecFn fn, double a, double b) { return fn(a, b); }
BinOp pick(int which) { return which ? mul : add; }
MakeFn maker(void) { return make_item; }
static const Slots initial = {add, item_fn, mul, add};
void fill_slots(Slots *s) { __builtin_memcpy(s, &initial, sizeof initial); }
int check_slots(const Slots *s)
{ return s->op(2, 3) == 6 && s->vop(2, 3) == 5 && s->item == item_fn && s->cop == add; }
void fill_packed(PSlots *p) { p->c = 1; p->vop = add; p->op = mul; }
int check_packed(const PSlots *p) { return p->vop(2, 3) == 6 && p->op(2, 3) == 5; }
"""
BODY = """    // 03a: scalar callback, called directly and passed to C.
    BinOp op = add;
    if (op(2, 3) != 5 || apply(op, 4, 5) != 9 || apply(mul, 4, 5) != 20) return 1;
    // 03b: record pointer and enum in the signature.
    Item it;
    it.v = 3;
    it.k = KOne;
    ItemFn fi = item_fn;
    if (fi(&it, KTwo) != 7 || apply_item(fi, &it, KTwo) != 7) return 2;
    // 03c: variadic callback identity (passed to C and called there).
    VarFn vf = sum_var;
    VarFn again = vf;
    if (apply_var(vf) != 6 || apply_var(again) != 6) return 3;
    // 03d: __vectorcall callback (T08-D22).
    VecFn vc = vec_add;
    if (vc(1.0, 2.0) != 3.0 || apply_vec(vc, 1.5, 2.0) != 3.5 || vec_add(2.0, 3.0) != 5.0) return 4;
    // 04a/04d/04e: callbacks in fields, volatile aligned and packed.
    Slots s;
    fill_slots(&s);
    if (s.op(6, 7) != 13 || s.vop(6, 7) != 42 || s.cop(1, 1) != 2 || s.item(&it, KOne) != 4) return 5;
    s.op = mul;
    s.vop = add;
    if (check_slots(&s) != 1) return 6;
    PSlots ps;
    fill_packed(&ps);
    if (ps.vop(2, 2) != 4 || ps.op(2, 3) != 6) return 7;
    ps.vop = mul;
    ps.op = add;
    if (check_packed(&ps) != 1) return 8;
    // 04b: globals.
    if (g_op(1, 2) != 3 || g_vop(2, 5) != 10) return 9;
    g_op = mul;
    g_vop = add;
    if (g_op(3, 3) != 9 || g_vop(3, 3) != 6) return 10;
    g_op = add;
    g_vop = mul;
    // 04c: callbacks returned by C functions.
    BinOp picked = pick(1);
    MakeFn mk = maker();
    Item* made = mk(5);
    if (picked(3, 4) != 12 || pick(0)(3, 4) != 7 || made->v != 5 || made->k != KTwo) return 11;
    return 0;
"""
REJECTIONS = [
    ("04e-escribir-callback-const", "Slots s; fill_slots(&s); s.cop = mul; return 0;", "not assignable"),
    ("03a-firma-incompatible", "BinOp op = item_fn; return 0;", ""),
]


def c_spelling(text):
    return text


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
    run([lib, "/nologo", "/out:ccb.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="ccb.lib" />'


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


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


def control():
    folder = root / "control"
    folder.mkdir()
    c_library(folder)
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\nint _fltused = 0;\n'
        f"static int run(void)\n{{\n{c_spelling(BODY)}}}\n"
        "void mainCRTStartup(void) { ExitProcess(run()); }\n", encoding="utf-8")
    result = run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
                 folder, check=False)
    if result.returncode:
        raise AssertionError(f"control Clang:\n{result.stdout}")
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "ccb.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    print("control Clang correcto", flush=True)


def paths(opt):
    direct = root / f"directo-O{opt}"
    direct.mkdir()
    c_library(direct)
    (direct / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(direct, "Direct", "Main.ns", REF)
    expect_run(f"directo O{opt}", direct, "Direct", opt)
    base = root / f"precompilado-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text("namespace Callbacks { public class Probe {\n"
                                   "public static int Run<T>(T unused)\n{\n" +
                                   BODY + "} } }\n", encoding="utf-8")
    project(producer, "CbPub", "P.ns", REF, library=True)
    result = build(producer, "CbPub", opt, "lib", "CbPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    with_header = base / "con-encabezado"
    with_header.mkdir()
    (with_header / "C.ns").write_text("int Main() { return Callbacks.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(with_header, "C", "C.ns", '<LibraryReference Include="../producer/CbPub.lib" />'
            '<CppLibraryReference Header="../producer/api.h" Language="C" Standard="c11" />')
    expect_run(f"precompilado con encabezado O{opt}", with_header, "C", opt)
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Callbacks.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/CbPub.lib" />')
    expect_run(f"precompilado sin encabezado O{opt}", base, "C", opt)


def rejections():
    for name, body, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder)
        (folder / "Main.ns").write_text("int Main() { " + body + " }\n", encoding="utf-8")
        project(folder, "N", "Main.ns", REF)
        result = build(folder, "N", "0", "exe", "out/N.exe")
        if observe:
            print("rechazo", name, result.returncode, errors(result), flush=True)
        elif result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{text}»:\n{result.stdout}")
        else:
            print("rechazo", name, "correcto", flush=True)


control()
for opt in opts:
    paths(opt)
rejections()
print("observación terminada" if observe else "Callbacks C verificados:", root)
