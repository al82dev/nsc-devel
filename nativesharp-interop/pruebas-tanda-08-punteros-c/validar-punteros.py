"""T08-014-02 to T08-014-04: C pointers against Clang.

02a) Pointers to every integer width, `float`, `double` and `_Bool`: C writes through them and returns them.
02b) Pointers to a struct, a union, an enum and `void`.
02c) `T**` and qualifiers at each represented level (`const int **`, `int *const *`, `volatile int **`).
02d) Compatible conversions (adding `const`/`volatile`, `T*` to `void*`, explicit `void*` to `T*`) and the
     diagnostics for losing a qualifier.
03a) Pointers as parameters and returns of C functions.
03b) Pointers in C fields and globals.
03c) The same body through a template of a precompiled producer consumed without the header.
04a) Address of a natural field and of a packed field (accepted when aligned, rejected when misaligned).
04b) Null pointers through C, and the dereference diagnostics.
04c) `const` and `volatile` propagation in initialization and reassignment. In NativeSharp `int const*` points to
     a constant and `const int*` is a constant local; a pointer to `volatile` comes from C and is kept by `var`.
     `int* p; int const** q = &p;` is accepted (C warns, C++ rejects): see the pending decision T08-014-02d1.
The body also runs in a Clang control. Positive cases run in O0-O3; rejections in O0.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-punteros-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef enum Mode { MOff = 0, MOn = 7 } Mode;
typedef struct Pt { int x; int y; } Pt;
typedef union Num { int i; float f; } Num;
#pragma pack(push, 1)
typedef struct Packed { char c; int i; char d; } Packed;
#pragma pack(pop)
typedef struct Links { int *ip; const char *name; Pt *pt; void *any; int **ipp; } Links;
extern int g_value;
extern int *g_ptr;
extern const char *g_name;
signed char *w8(signed char *p);
short *w16(short *p);
int *w32(int *p);
long long *w64(long long *p);
unsigned char *wu8(unsigned char *p);
unsigned short *wu16(unsigned short *p);
unsigned *wu32(unsigned *p);
unsigned long long *wu64(unsigned long long *p);
float *wf(float *p);
double *wd(double *p);
_Bool *wb(_Bool *p);
Pt *move_pt(Pt *p);
Num *set_num(Num *n);
Mode *set_mode(Mode *m);
void *same(void *p);
int read_any(const void *p);
int sum_pp(int **pp, int count);
int read_cpp(const int **pp);
int read_icp(int *const *pp);
int read_vpp(volatile int **pp);
int *null_ptr(void);
int is_null(const int *p);
int links_sum(const Links *l);
int read_const(const int *p);
int read_volatile(volatile int *p);
volatile int *vptr(int *p);
"""
SOURCE = """#include "api.h"
int g_value = 11;
int *g_ptr = &g_value;
const char *g_name = "pc";
signed char *w8(signed char *p) { *p = -8; return p; }
short *w16(short *p) { *p = -1600; return p; }
int *w32(int *p) { *p = -320000; return p; }
long long *w64(long long *p) { *p = -6400000000LL; return p; }
unsigned char *wu8(unsigned char *p) { *p = 200; return p; }
unsigned short *wu16(unsigned short *p) { *p = 60000; return p; }
unsigned *wu32(unsigned *p) { *p = 4000000000u; return p; }
unsigned long long *wu64(unsigned long long *p) { *p = 18000000000000000000ull; return p; }
float *wf(float *p) { *p = 2.5f; return p; }
double *wd(double *p) { *p = -0.125; return p; }
_Bool *wb(_Bool *p) { *p = !*p; return p; }
Pt *move_pt(Pt *p) { p->x += 1; p->y -= 1; return p; }
Num *set_num(Num *n) { n->f = 1.0f; return n; }
Mode *set_mode(Mode *m) { *m = MOn; return m; }
void *same(void *p) { return p; }
int read_any(const void *p) { return *(const int *)p; }
int sum_pp(int **pp, int count) { int s = 0; for (int i = 0; i < count; ++i) s += *pp[i]; return s; }
int read_cpp(const int **pp) { return **pp; }
int read_icp(int *const *pp) { return **pp; }
int read_vpp(volatile int **pp) { return **pp; }
int *null_ptr(void) { return 0; }
int is_null(const int *p) { return p == 0; }
int links_sum(const Links *l) { return *l->ip + l->pt->x + read_any(l->any) + **l->ipp + (l->name[0] == 'p'); }
int read_const(const int *p) { return *p; }
int read_volatile(volatile int *p) { return *p; }
volatile int *vptr(int *p) { return p; }
"""
BODY = """    // 02a/03a: scalar pointers as parameters and returns.
    sbyte sb = 0; short ss = 0; int si = 0; long sl = 0;
    byte ub = 0; ushort us = 0; uint ui = 0; ulong ul = 0;
    float f = 0; double d = 0; bool b = false;
    if (*w8(&sb) != -8 || *w16(&ss) != -1600 || *w32(&si) != -320000 || *w64(&sl) != -6400000000L) return 1;
    if (*wu8(&ub) != 200 || *wu16(&us) != 60000 || *wu32(&ui) != 4000000000 ||
        *wu64(&ul) != 18000000000000000000) return 2;
    if (*wf(&f) != 2.5f || *wd(&d) != -0.125 || !*wb(&b) || b != true) return 3;
    // 02b: struct, union, enum and void.
    Pt p;
    p.x = 1;
    p.y = 1;
    Pt* pp = move_pt(&p);
    if (pp != &p || p.x != 2 || pp->y != 0) return 4;
    Num n;
    n.i = 0;
    if (set_num(&n)->i != 1065353216) return 5;
    Mode m = MOff;
    if (*set_mode(&m) != MOn || m != MOn) return 6;
    void* any = &si;
    if (same(any) != any || read_any(any) != -320000 || *(int*)same(&si) != -320000) return 7;
    // 02c: pointer to pointer and qualifiers at each level.
    int one = 1;
    int two = 2;
    int* first = &one;
    int** both = &first;
    if (sum_pp(both, 1) != 1 || **both != 1) return 8;
    int const* cfirst = &two;
    int const** cboth = &cfirst;
    if (**cboth != 2) return 9;
    var vfirst = vptr(&two);
    if (*vfirst != 2 || read_volatile(vfirst) != 2) return 10;
    // 02d: compatible conversions.
    int const* added = first;
    void* erased = first;
    int* restored = (int*)erased;
    if (read_const(added) != 1 || read_volatile(first) != 1 || restored != first) return 11;
    // 03b: fields and globals.
    Links l;
    l.ip = &one;
    l.name = g_name;
    l.pt = &p;
    l.any = &two;
    l.ipp = both;
    if (links_sum(&l) != 7 || l.pt == null) return 12;
    if (*g_ptr != 11 || g_ptr != &g_value) return 13;
    g_ptr = &two;
    if (*g_ptr != 2) return 14;
    g_ptr = &g_value;
    // 04a: address of natural and packed fields.
    int* natural = &p.y;
    *natural = 40;
    if (p.y != 40) return 15;
    Packed k;
    sbyte* aligned = &k.c;
    *aligned = 3;
    if (k.c != 3) return 16;
    // 04b: null through C.
    int* nothing = null_ptr();
    if (nothing != null || is_null(nothing) != 1 || is_null(null) != 1 || is_null(&one) != 0) return 17;
    // 04c: qualifier propagation on reassignment.
    int const* moving = &one;
    moving = &two;
    var vmoving = vptr(&one);
    vmoving = vptr(&two);
    return *moving == 2 && *vmoving == 2 ? 0 : 18;
"""
REJECTIONS = [
    ("02d-quitar-const", "int x = 1; int const* c = &x; int* q = c; return *q;", "constness"),
    ("02d-quitar-volatile", "int x = 1; var v = vptr(&x); int* q = v; return *q;",
     "volatile qualification"),
    ("02d-void-implicito", "int x = 1; void* v = &x; int* q = v; return *q;", "from 'builtin:void*'"),
    ("04a-campo-empaquetado", "Packed k; int* q = &k.i; return 0;", "addressable storage"),
    ("04b-desreferencia-void", "int x = 1; void* v = &x; return *v;", "non-void pointer"),
    ("04b-desreferencia-entero", "int x = 1; return *x;", "non-void pointer"),
    ("04c-escribir-const", "int x = 1; int const* c = &x; *c = 2; return 0;", "not assignable"),
    ("04c-reasignar-local-const", "int x = 1; const int* c = &x; c = &x; return 0;", "not assignable"),
    ("02c-no-representado-const", "int x = 1; int const* c = &x; return read_cpp(&c);", "is not yet mapped"),
    ("02c-no-representado-volatile", "int x = 1; var v = vptr(&x); return read_vpp(&v);", "is not yet mapped"),
    ("02c-no-representado-intermedio", "int x = 1; int* p = &x; return read_icp(&p);", "is not yet mapped"),
]


def c_spelling(text):
    text = re.sub(r"\bnull\b", "0", text)
    text = text.replace("var vfirst", "volatile int* vfirst").replace("var vmoving", "volatile int* vmoving")
    return text.replace("sbyte ", "signed char ").replace("byte ", "unsigned char ").replace(
        "ushort ", "unsigned short ").replace("uint ", "unsigned ").replace("ulong ", "unsigned long long ").replace(
        "long sl", "long long sl").replace("bool b = false", "_Bool b = 0").replace("true", "1").replace(
        "6400000000L", "6400000000LL").replace("18000000000000000000", "18000000000000000000ull").replace(
        "4000000000 ", "4000000000u ").replace("sbyte*", "signed char*")


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
    run([lib, "/nologo", "/out:cptr.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cptr.lib" />'


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
         "control.obj", "cptr.lib", sdk / "um/x64/kernel32.lib"], folder)
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
    (producer / "P.ns").write_text("namespace Ptrs { public class Probe { public static int Run<T>(T unused)\n{\n" +
                                   BODY + "} } }\n", encoding="utf-8")
    project(producer, "PtrPub", "P.ns", REF, library=True)
    result = build(producer, "PtrPub", opt, "lib", "PtrPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Ptrs.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/PtrPub.lib" />')
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
print("observación terminada" if observe else "Punteros C verificados:", root)
