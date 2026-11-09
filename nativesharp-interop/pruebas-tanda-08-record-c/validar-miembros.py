"""T08-010-04: members of C structs against Clang.

a) Scalar fields: integers of every width, `float`, `double`, an enum and `_Bool`.
b) Pointer fields: plain, `const` and `volatile` pointees (also `volatile void *`), a `const` pointer, a function
   pointer typedef and a raw function pointer.
c) A fixed array field and a flexible array member at the end of the struct.
d) Bit fields: signed and unsigned, an unnamed bit field and a zero-width bit field.
e) Promotion of members of anonymous structs and unions over several levels.
f) A struct nested in structs packed with `pack(1)`, `pack(2)` and `pack(4)`.
06) The same members through the three consumption paths (direct header, precompiled producer with the header
    in the consumer, metadata only), including `const` and `volatile` flexible members (06d).
For every struct NativeSharp compares its size and offsets with Clang (`sizeof`/`offsetof` compiled by Clang),
C fills an object that NativeSharp reads, and NativeSharp writes values that C checks. The same body runs in a
Clang control, directly with the header and through a template of a precompiled producer consumed without the
header, O0-O3. Rejections run in O0.
04b) Accesses through a `volatile int *` field follow the volatile mode of the C unit: with the Microsoft mode
     (and the default of this target) loads are acquire and stores release, as Clang with `-fms-volatile`; with
     the ISO mode they are plain volatile accesses, as Clang with `-fno-ms-volatile`. Checked on the optimized IR
     of the direct path and of a precompiled template consumed without the header.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-record-miembros-", dir=r"A:\nsc-interop-build"))
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

HEADER = """#include <stddef.h>
typedef enum Kind { KA = 1, KB = 2 } Kind;
typedef int (*Op)(int);
typedef struct Scal {
    char c; short s; int i; long long ll; float f; double d; Kind k; _Bool b; unsigned char uc;
} Scal;
typedef struct Ptrs {
    int *p; const int *cp; volatile int *vp; int *const pc; Op op; int (*raw)(int); volatile void *vv;
} Ptrs;
typedef struct Arr { int n; short items[3]; double tail[]; } Arr;
typedef struct Bits { int a : 3; unsigned b : 5; int : 4; unsigned : 0; unsigned c : 7; int d : 1; } Bits;
typedef struct Anon {
    int head;
    union { int asInt; unsigned asUnsigned; struct { short lo; short hi; }; };
    struct { char tag; union { char small; struct { char deep; char deeper; }; }; };
} Anon;
typedef struct Inner { char c; int i; } Inner;
#pragma pack(push, 1)
typedef struct P1 { char a; Inner inner; short s; } P1;
#pragma pack(pop)
#pragma pack(push, 2)
typedef struct P2 { char a; Inner inner; int i; } P2;
#pragma pack(pop)
#pragma pack(push, 4)
typedef struct P4 { char a; long long ll; Inner inner; } P4;
#pragma pack(pop)
typedef struct CFlex { int n; const int items[]; } CFlex;
typedef struct VFlex { int n; volatile int items[]; } VFlex;
int layout(int which);
int twice(int v);
int negate(int v);
void fill_scal(Scal *s);
int check_scal(const Scal *s);
Ptrs make_ptrs(int *target);
int check_ptrs(const Ptrs *p, int *target);
int same_address(volatile void *v, int *target);
Arr *make_arr(void);
int check_arr(const Arr *a);
void fill_bits(Bits *b);
int check_bits(const Bits *b);
void fill_anon(Anon *a);
int check_anon(const Anon *a);
void fill_packed(P1 *p1, P2 *p2, P4 *p4);
int check_packed(const P1 *p1, const P2 *p2, const P4 *p4);
CFlex *make_cflex(void);
VFlex *make_vflex(void);
int check_vflex(const VFlex *v);
"""

OFFSETS = [
    "sizeof(Scal)", "offsetof(Scal, ll)", "offsetof(Scal, f)", "offsetof(Scal, k)", "offsetof(Scal, b)",
    "sizeof(Ptrs)", "offsetof(Ptrs, vp)", "offsetof(Ptrs, raw)", "sizeof(Arr)", "offsetof(Arr, tail)",
    "sizeof(Bits)", "sizeof(Anon)", "offsetof(Anon, asInt)", "sizeof(P1)", "offsetof(P1, s)", "sizeof(P2)",
    "offsetof(P2, i)", "sizeof(P4)", "offsetof(P4, inner)",
]

SOURCE = """#include "api.h"
static double storage[8];
int layout(int which)
{
    switch (which) {
""" + "".join(f"    case {k}: return (int){expr};\n" for k, expr in enumerate(OFFSETS)) + """    }
    return -1;
}
int twice(int v) { return v * 2; }
int negate(int v) { return -v; }
void fill_scal(Scal *s)
{
    s->c = -3; s->s = -300; s->i = 70000; s->ll = 5000000000LL; s->f = 1.5f; s->d = -2.25; s->k = KB;
    s->b = 1; s->uc = 250;
}
int check_scal(const Scal *s)
{
    return s->c == 7 && s->s == 3000 && s->i == -70000 && s->ll == -5000000000LL && s->f == 0.25f &&
        s->d == 8.5 && s->k == KA && s->b == 0 && s->uc == 5;
}
Ptrs make_ptrs(int *target) { Ptrs p = {target, target, target, target, twice, negate, target}; return p; }
int same_address(volatile void *v, int *target) { return v == target; }
int check_ptrs(const Ptrs *p, int *target)
{
    return p->p == target && p->cp == target && p->vp == target && p->op(4) == -4 && p->raw(5) == 10 &&
        p->vv == 0;
}
Arr *make_arr(void)
{
    Arr *a = (Arr *)storage;
    a->n = 2; a->items[0] = 1; a->items[1] = 2; a->items[2] = 3; a->tail[0] = 0.5; a->tail[1] = 1.5;
    return a;
}
int check_arr(const Arr *a) { return a->n == 3 && a->items[2] == 30 && a->tail[2] == 9.0; }
void fill_bits(Bits *b) { b->a = -3; b->b = 31; b->c = 100; b->d = -1; }
int check_bits(const Bits *b) { return b->a == 2 && b->b == 17 && b->c == 127 && b->d == 0; }
void fill_anon(Anon *a) { a->head = 1; a->lo = 2; a->hi = 3; a->tag = 4; a->deep = 5; a->deeper = 6; }
int check_anon(const Anon *a)
{ return a->head == 10 && a->asInt == -1 && a->tag == 40 && a->small == 50 && a->deeper == 60; }
void fill_packed(P1 *p1, P2 *p2, P4 *p4)
{
    p1->a = 1; p1->inner.c = 2; p1->inner.i = 3; p1->s = 4;
    p2->a = 5; p2->inner.c = 6; p2->inner.i = 7; p2->i = 8;
    p4->a = 9; p4->ll = 10; p4->inner.c = 11; p4->inner.i = 12;
}
int check_packed(const P1 *p1, const P2 *p2, const P4 *p4)
{ return p1->inner.i == 30 && p1->s == 40 && p2->inner.i == 70 && p2->i == 80 && p4->ll == 100 && p4->inner.i == 120; }
static int cstore[4] = {2, 7, 8};
static int vstore[4] = {2, 5, 6};
CFlex *make_cflex(void) { return (CFlex *)cstore; }
VFlex *make_vflex(void) { return (VFlex *)vstore; }
int check_vflex(const VFlex *v) { return v->n == 2 && v->items[0] == 5 && v->items[1] == 60; }
"""

BODY = """    // 04a-04f: sizes and offsets seen by NativeSharp against Clang.
    if (layout(0) != (int)sizeof(Scal) || layout(5) != (int)sizeof(Ptrs)) return 1;
    if (layout(10) != (int)sizeof(Bits)) return 1;
    if (layout(11) != (int)sizeof(Anon) || layout(13) != (int)sizeof(P1) || layout(15) != (int)sizeof(P2)) return 2;
    if (layout(17) != (int)sizeof(P4) || layout(8) != (int)sizeof(Arr)) return 3;
    // 04a
    Scal s;
    fill_scal(&s);
    if (s.c != -3 || s.s != -300 || s.i != 70000 || s.ll != 5000000000L || s.f != 1.5f || s.d != -2.25) return 4;
    if (s.k != KB || s.b != true || s.uc != 250) return 5;
    s.c = 7; s.s = 3000; s.i = -70000; s.ll = -5000000000L; s.f = 0.25f; s.d = 8.5; s.k = KA;
    s.b = false; s.uc = 5;
    if (check_scal(&s) != 1) return 6;
    // 04b
    int target = 41;
    Ptrs p = make_ptrs(&target);
    if (*p.p != 41 || *p.cp != 41 || *p.vp != 41 || *p.pc != 41 || p.op(4) != 8 || p.raw(5) != -5) return 7;
    *p.vp = 42;
    *p.pc = 43;
    if (target != 43 || *p.cp != 43) return 8;
    p.op = negate;
    p.raw = twice;
    if (same_address(p.vv, &target) != 1) return 9;
    p.vv = null;
    if (check_ptrs(&p, &target) != 1) return 9;
    // 04c
    Arr* a = make_arr();
    if (a->n != 2 || a->items[0] != 1 || a->items[2] != 3 || a->tail[0] != 0.5 || a->tail[1] != 1.5) return 10;
    a->n = 3;
    a->items[2] = 30;
    a->tail[2] = 9.0;
    if (check_arr(a) != 1) return 11;
    // 04d
    Bits b;
    fill_bits(&b);
    if (b.a != -3 || b.b != 31 || b.c != 100 || b.d != -1) return 12;
    b.a = 2; b.b = 17; b.c = 127; b.d = 0;
    if (check_bits(&b) != 1) return 13;
    // 04e
    Anon n;
    fill_anon(&n);
    if (n.head != 1 || n.lo != 2 || n.hi != 3 || n.tag != 4) return 14;
    if (n.deep != 5 || n.deeper != 6 || n.small != 5) return 14;
    n.head = 10; n.asInt = -1; n.tag = 40; n.small = 50; n.deeper = 60;
    if (check_anon(&n) != 1 || n.asUnsigned != 4294967295 || n.lo != -1) return 15;
    // 04f
    P1 p1;
    P2 p2;
    P4 p4;
    fill_packed(&p1, &p2, &p4);
    if (p1.inner.i != 3 || p1.s != 4 || p2.inner.i != 7 || p2.i != 8 || p4.ll != 10 || p4.inner.i != 12) return 16;
    p1.inner.i = 30; p1.s = 40; p2.inner.i = 70; p2.i = 80; p4.ll = 100; p4.inner.i = 120;
    if (check_packed(&p1, &p2, &p4) != 1) return 17;
    // 06d: qualified flexible members.
    CFlex* cf = make_cflex();
    if (cf->n != 2 || cf->items[0] != 7 || cf->items[1] != 8) return 18;
    VFlex* vf = make_vflex();
    if (vf->items[0] != 5) return 19;
    vf->items[1] = 60;
    if (check_vflex(vf) != 1 || vf->items[1] != 60) return 20;
    return 0;
"""

REJECTIONS = [
    ("const-puntero", "int t = 1; Ptrs p = make_ptrs(&t); p.pc = &t; return 0;",
     "left side of assignment is not assignable", True),
    ("escribir-apuntado-const", "int t = 1; Ptrs p = make_ptrs(&t); *p.cp = 2; return 0;",
     "left side of assignment is not assignable", True),
    ("quitar-volatile-campo", "int t = 1; Ptrs p = make_ptrs(&t); int* q = p.vp; return *q;",
     "initialization would discard pointer pointee volatile qualification", False),
    ("bit-field-direccion", "Bits b; int* q = &b.a; return 0;",
     "bit fields have no address", True),
    ("escribir-flexible-const", "CFlex* cf = make_cflex(); cf->items[0] = 1; return 0;",
     "left side of assignment is not assignable", True),
]


def c_spelling(text):
    return text.replace("5000000000L", "5000000000LL").replace("true", "1").replace("false", "0").replace(
        "Arr* a", "Arr *a").replace("4294967295", "4294967295u").replace("= null;", "= 0;")


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
    run([lib, "/nologo", "/out:crecord.lib", "api.obj"], folder)


HEADER_REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="crecord.lib" />'


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
         "control.obj", "crecord.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    for name, statements, _, c_rejects in REJECTIONS:
        probe = folder / f"rechazo-{name}.c"
        probe.write_text(f'#include "api.h"\nint probe(void) {{ {statements.replace("int* q", "int *q")} }}\n',
                         encoding="utf-8")
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
    (folder / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], HEADER_REF)
    build_and_run(f"directo O{opt}", folder, "Direct", opt)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "Producer.ns").write_text(
        "namespace Records { public class Bridge { public static int Run<T>(T unused)\n{\n" + BODY + "} } }\n",
        encoding="utf-8")
    project(producer, "RecordBridge", ["Producer.ns"], HEADER_REF, library=True)
    result = nsc_build(producer, "RecordBridge", opt, "lib", "RecordBridge.lib")
    if result.returncode:
        report(f"productor O{opt}", result, None)
        return
    # 06b: the consumer also has the header; 06c: metadata only.
    with_header = folder / "con-encabezado"
    with_header.mkdir()
    (with_header / "Consumer.ns").write_text("int Main() { return Records.Bridge.Run<int>(0); }\n",
                                             encoding="utf-8")
    project(with_header, "Consumer", ["Consumer.ns"], '<LibraryReference Include="../producer/RecordBridge.lib" />'
            '<CppLibraryReference Header="../producer/api.h" Language="C" Standard="c11" />')
    build_and_run(f"precompilado con encabezado O{opt}", with_header, "Consumer", opt)
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Records.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/RecordBridge.lib" />')
    build_and_run(f"precompilado sin encabezado O{opt}", folder, "Consumer", opt)


IR_HEADER = "typedef struct Ptrs { int *p; volatile int *vp; } Ptrs;\n"
IR_C = """#include "api.h"
int cread(Ptrs *p) { return *p->vp + p->vp[1]; }
void cwrite(Ptrs *p) { *p->vp = 42; p->vp[1] = 7; }
void cupdate(Ptrs *p) { *p->vp += 3; }
"""
IR_NS = """    public static int NRead(Ptrs* p) { return *p->vp + p->vp[1]; }
    public static void NWrite(Ptrs* p) { *p->vp = 42; p->vp[1] = 7; }
    public static void NUpdate(Ptrs* p) { *p->vp += 3; }
"""
IR_DRIVE = """    public static int Drive<T>(T unused)
    {
        int a = 1;
        Ptrs s;
        s.vp = &a;
        *s.vp = 42;
        s.vp[0] = 7;
        *s.vp += 3;
        return *s.vp + s.vp[0];
    }
"""


def volatile_accesses(text):
    lines = [l.strip() for l in text.splitlines() if "volatile i32" in l and ("load" in l or "store" in l)]
    atomic = [l for l in lines if " atomic volatile " in l and (" acquire" in l or " release" in l)]
    return len(lines), len(atomic)


def expect_ir(label, text, atomic):
    total, atomics = volatile_accesses(text)
    if observe:
        print(label, total, atomics, flush=True)
        return
    if total < 6 or atomics != (total if atomic else 0):
        raise AssertionError(f"{label}: {total} accesos volatile, {atomics} atómicos:\n{text}")
    print(label, "correcto", flush=True)


def ir_check():
    folder = root / "ir"
    folder.mkdir()
    (folder / "api.h").write_text(IR_HEADER, encoding="utf-8")
    (folder / "c.c").write_text(IR_C, encoding="utf-8")
    for mode, flag in (("ms", "-fms-volatile"), ("iso", "-fno-ms-volatile")):
        run([clang, "-S", "-emit-llvm", "--target=x86_64-pc-windows-msvc", "-std=c11", "-O1", flag, "c.c", "-o",
             f"c-{mode}.ll"], folder)
        expect_ir(f"IR Clang {mode}", (folder / f"c-{mode}.ll").read_text(encoding="utf-8"), mode == "ms")
    for mode in ("Default", "Microsoft", "ISO"):
        direct_folder = folder / mode
        direct_folder.mkdir()
        (direct_folder / "api.h").write_text(IR_HEADER, encoding="utf-8")
        (direct_folder / "M.ns").write_text("namespace Ir { public class V {\n" + IR_NS + IR_DRIVE + "} }\n"
                                            "int Main() { return Ir.V.Drive<int>(0) == 20 ? 0 : 1; }\n",
                                            encoding="utf-8")
        ref = f'<CppLibraryReference Header="api.h" Language="C" Standard="c11" VolatileMode="{mode}" />'
        project(direct_folder, "M", ["M.ns"], ref)
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", "M.nsproj", "-O1"],
                     direct_folder, check=False)
        if result.returncode:
            raise AssertionError(f"IR directo {mode}:\n{result.stdout}")
        expect_ir(f"IR directo {mode}", result.stdout, mode != "ISO")
        producer = folder / f"productor-{mode}"
        producer.mkdir()
        (producer / "api.h").write_text(IR_HEADER, encoding="utf-8")
        (producer / "P.ns").write_text("namespace Ir { public class V {\n" + IR_DRIVE + "} }\n", encoding="utf-8")
        project(producer, "IrPub", ["P.ns"], ref, library=True)
        result = nsc_build(producer, "IrPub", "1", "lib", "IrPub.lib")
        if result.returncode:
            raise AssertionError(f"IR productor {mode}:\n{result.stdout}")
        (producer / "api.h").unlink()
        consumer = folder / f"consumidor-{mode}"
        consumer.mkdir()
        (consumer / "C.ns").write_text("int Main() { return Ir.V.Drive<int>(0) == 20 ? 0 : 1; }\n",
                                       encoding="utf-8")
        project(consumer, "C", ["C.ns"], f'<LibraryReference Include="../productor-{mode}/IrPub.lib" />')
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", "C.nsproj", "-O1"],
                     consumer, check=False)
        if result.returncode:
            raise AssertionError(f"IR consumidor {mode}:\n{result.stdout}")
        expect_ir(f"IR precompilado sin encabezado {mode}", result.stdout, mode != "ISO")


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
ir_check()
for opt in opts:
    direct(opt)
    precompiled(opt)
rejections()
print("observación terminada" if observe else "Miembros de struct C verificados:", root)
