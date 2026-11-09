"""T08-016-03 and T08-016-04: fixed C arrays against Clang.

03a) Fixed arrays of integer and floating scalars, in fields and globals.
03b) Multidimensional scalar matrices, in a field and a global.
03c) Arrays of structs, unions and enums.
03d) Arrays of `const` pointers and of pointers to `const` or `volatile` data, as C globals. As record fields they
     are not imported yet: see the pending item T08-016-03d1.
03e) Arrays inside packed and nested records.
04a) Size, element offsets and alignment compared with Clang (`sizeof`, `offsetof`, `_Alignof`); NativeSharp
     measures offsets with address differences.
04b) Cross reads and writes: C fills and NativeSharp reads; NativeSharp writes and C checks.
04c) The address of an element is accepted when aligned and rejected when the element is underaligned.
04d) The same body through a template of a precompiled producer consumed without the header.
The body also runs in a Clang control. Positive cases run in O0-O3; rejections in O0.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-arreglos-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef enum Mode { MA = 1, MB = 2, MC = 3 } Mode;
typedef struct Pt { short x; int y; } Pt;
typedef union Num { int i; float f; } Num;
typedef struct In { char c; int v; } In;
typedef struct Arrs {
    char tag; int a[4]; double d[3]; short m[2][3]; Pt pts[3]; Num nums[2]; Mode modes[3]; int *ps[2];
} Arrs;
#pragma pack(push, 1)
typedef struct PA { char c; int vals[3]; In inner[2]; char bytes[3]; } PA;
#pragma pack(pop)
typedef struct Nest { char c; PA packed; In more[2]; } Nest;
extern int g_arr[5];
extern float g_f[2];
extern int g_m[2][2];
extern Pt g_pts[2];
extern int *const g_cps[2];
extern const int *g_pcs[2];
extern volatile int *g_pvs[2];
int layout(int which);
void fill(Arrs *s, int *target);
int check(const Arrs *s);
void fill_packed(Nest *n);
int check_packed(const Nest *n);
int check_globals(void);
"""
LAYOUT = ["sizeof(Arrs)", "_Alignof(Arrs)", "offsetof(Arrs, a)", "offsetof(Arrs, d)", "offsetof(Arrs, m)",
          "offsetof(Arrs, pts)", "offsetof(Arrs, nums)", "offsetof(Arrs, modes)", "offsetof(Arrs, ps)",
          "offsetof(Arrs, ps[1])", "sizeof(PA)", "_Alignof(PA)", "offsetof(PA, vals)", "offsetof(PA, inner)",
          "offsetof(PA, bytes)", "sizeof(Nest)", "offsetof(Nest, packed)", "offsetof(Nest, more)"]
SOURCE = """#include <stddef.h>
#include "api.h"
int g_arr[5] = {1, 2, 3, 4, 5};
float g_f[2] = {0.5f, 1.5f};
int g_m[2][2] = {{1, 2}, {3, 4}};
Pt g_pts[2] = {{1, 10}, {2, 20}};
static int pointees[2] = {61, 62};
int *const g_cps[2] = {&pointees[0], &pointees[1]};
const int *g_pcs[2] = {&pointees[0], &pointees[1]};
volatile int *g_pvs[2] = {&pointees[0], &pointees[1]};
int layout(int which)
{
    switch (which) {
""" + "".join(f"    case {k}: return (int){e};\n" for k, e in enumerate(LAYOUT)) + """    }
    return -1;
}
void fill(Arrs *s, int *target)
{
    s->tag = 1;
    for (int i = 0; i < 4; ++i) s->a[i] = 10 * (i + 1);
    for (int i = 0; i < 3; ++i) s->d[i] = i + 0.25;
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 3; ++j) s->m[i][j] = (short)(i * 3 + j - 2);
    for (int i = 0; i < 3; ++i) { s->pts[i].x = (short)i; s->pts[i].y = 100 + i; s->modes[i] = (Mode)(i + 1); }
    s->nums[0].f = 1.0f; s->nums[1].i = -1;
    s->ps[0] = target; s->ps[1] = target + 1;
}
int check(const Arrs *s)
{
    return s->a[0] == 7 && s->a[3] == -7 && s->d[2] == 9.5 && s->m[1][2] == 600 && s->m[0][0] == -600 &&
        s->pts[2].y == 222 && s->pts[1].x == 11 && s->nums[1].f == 2.0f && s->modes[0] == MC;
}
void fill_packed(Nest *n)
{
    n->c = 1; n->packed.c = 2;
    for (int i = 0; i < 3; ++i) { n->packed.vals[i] = 1000 + i; n->packed.bytes[i] = (char)(i + 1); }
    n->packed.inner[1].v = 77; n->more[1].v = 88;
}
int check_packed(const Nest *n)
{ return n->packed.vals[2] == 3003 && n->packed.inner[0].v == 55 && n->more[0].v == 66; }
int check_globals(void) { return g_arr[4] == 50 && g_f[1] == 4.0f && g_m[1][0] == 30 && g_pts[1].y == 200; }
"""
BODY = """    Arrs s;
    Pt slots;
    slots.x = 0;
    slots.y = 0;
    // 04a: layout through address differences, compared with Clang.
    if ((int)sizeof(Arrs) != layout(0) || layout(1) != 8) return 1;
    if ((int)((byte*)&s.a[0] - (byte*)&s) != layout(2) || (int)((byte*)&s.d[0] - (byte*)&s) != layout(3)) return 2;
    if ((int)((byte*)&s.m[0][0] - (byte*)&s) != layout(4) || (int)((byte*)&s.m[1][0] - (byte*)&s.m[0][0]) != 6)
        return 3;
    if ((int)((byte*)&s.pts[0] - (byte*)&s) != layout(5) || (int)((byte*)&s.pts[1] - (byte*)&s.pts[0]) != 8) return 4;
    if ((int)((byte*)&s.nums[0] - (byte*)&s) != layout(6) || (int)((byte*)&s.modes[0] - (byte*)&s) != layout(7))
        return 5;
    if ((int)((byte*)&s.ps[0] - (byte*)&s) != layout(8) || (int)((byte*)&s.ps[1] - (byte*)&s) != layout(9)) return 6;
    if ((int)sizeof(PA) != layout(10) || layout(11) != 1 || layout(12) != 1 || layout(13) != 13 || layout(14) != 29)
        return 7;
    Nest n;
    if ((int)sizeof(Nest) != layout(15) || (int)((byte*)&n.packed - (byte*)&n) != layout(16)) return 8;
    if ((int)((byte*)&n.more[0] - (byte*)&n) != layout(17)) return 8;
    // 03a-03d / 04b: C fills, NativeSharp reads.
    int* target = &slots.y;
    fill(&s, target);
    if (s.a[1] != 20 || s.a[3] != 40 || s.d[2] != 2.25 || s.m[1][2] != 3 || s.m[0][0] != -2) return 9;
    if (s.pts[2].x != 2 || s.pts[2].y != 102 || s.nums[0].i != 1065353216 || s.nums[1].i != -1) return 10;
    if (s.modes[2] != MC || (int)s.modes[0] != 1) return 11;
    *s.ps[0] = 5;
    if (slots.y != 5 || s.ps[1] != target + 1) return 12;
    // 03d: arrays of const pointers and of pointers to const or volatile data (C globals).
    *g_cps[1] = 72;
    g_pcs[0] = g_cps[1];
    var vslot = g_pvs[1];
    *vslot = 73;
    if (*g_pcs[0] != 73 || *g_pcs[1] != 73 || *g_pvs[0] != 61 || *g_cps[0] != 61) return 12;
    // NativeSharp writes, C checks.
    s.a[0] = 7;
    s.a[3] = -7;
    s.d[2] = 9.5;
    s.m[1][2] = 600;
    s.m[0][0] = -600;
    s.pts[2].y = 222;
    s.pts[1].x = 11;
    s.nums[1].f = 2.0f;
    s.modes[0] = MC;
    if (check(&s) != 1) return 13;
    // 03e: arrays in packed and nested records.
    fill_packed(&n);
    if (n.packed.vals[1] != 1001 || n.packed.bytes[2] != 3 || n.packed.inner[1].v != 77 || n.more[1].v != 88) return 14;
    n.packed.vals[2] = 3003;
    n.packed.inner[0].v = 55;
    n.more[0].v = 66;
    if (check_packed(&n) != 1) return 15;
    // 04c: address of an aligned element.
    int* element = &s.a[2];
    *element = 33;
    sbyte* small = &n.packed.bytes[1];
    *small = 9;
    if (s.a[2] != 33 || n.packed.bytes[1] != 9) return 16;
    // Globals.
    if (g_arr[2] != 3 || g_f[1] != 1.5f || g_m[1][1] != 4 || g_pts[0].y != 10) return 17;
    g_arr[4] = 50;
    g_f[1] = 4.0f;
    g_m[1][0] = 30;
    g_pts[1].y = 200;
    return check_globals() == 1 ? 0 : 18;
"""
REJECTIONS = [
    ("04c-elemento-subalineado", "Nest n; int* q = &n.packed.vals[1]; return 0;", "addressable storage"),
    ("04c-registro-subalineado", "Nest n; In* q = &n.packed.inner[0]; return 0;", "addressable storage"),
    ("03d-escribir-puntero-const", "int x = 1; g_cps[0] = &x; return 0;", "not assignable"),
    ("03d-escribir-apuntado-const", "*g_pcs[0] = 2; return 0;", "not assignable"),
    ("03d-quitar-volatile", "int* q = g_pvs[0]; return *q;", "volatile"),
]


def c_spelling(text):
    return text.replace("(byte*)", "(unsigned char*)").replace("sbyte*", "char*").replace(
        "var vslot", "volatile int* vslot")


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
    run([lib, "/nologo", "/out:carr.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="carr.lib" />'


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
         "control.obj", "carr.lib", sdk / "um/x64/kernel32.lib"], folder)
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
    (producer / "P.ns").write_text("namespace Arrays { public class Probe { public static int Run<T>(T unused)\n{\n" +
                                   BODY + "} } }\n", encoding="utf-8")
    project(producer, "ArrPub", "P.ns", REF, library=True)
    result = build(producer, "ArrPub", opt, "lib", "ArrPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Arrays.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/ArrPub.lib" />')
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
print("observación terminada" if observe else "Arreglos C verificados:", root)
