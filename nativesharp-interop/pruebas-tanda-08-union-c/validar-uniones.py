"""T08-011-02 and T08-011-03: C unions against Clang.

02a) A natural union with integer, floating, pointer and byte-array members.
02b) A union packed with `pack(1)` inside a struct, so its members are misaligned. Their offsets are checked
     through the bytes of the struct, because NativeSharp rejects the address of a misaligned member.
02c) A union inside a struct and a tagged struct inside a union. A named member whose struct type has no tag
     is not covered: see the pending decision T08-011-02c1.
02d) An anonymous union with promoted members and overlapping bit fields.
03a) Size and alignment of every union and the offsets that place them, compared with Clang (`sizeof`,
     `_Alignof` and `offsetof` compiled by Clang; NativeSharp measures sizes with `sizeof` and offsets and
     alignment with address differences).
03b) Bytes after writing through one member and reading through another, compared with C puns.
03c) The same body through a template of a precompiled producer consumed without the header.
The body also runs in a Clang control. Positive cases run in O0-O3; the rejection in O0.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-uniones-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef union U1 { int i; float f; double d; void *p; long long ll; unsigned char bytes[8]; } U1;
typedef struct A1 { char c; U1 u; } A1;
#pragma pack(push, 1)
typedef union UP { char c; long long ll; double d; } UP;
typedef struct SP { char a; UP u; short tail; } SP;
#pragma pack(pop)
typedef union UV { int i; float f; } UV;
typedef struct S3 { int tag; UV v; char after; } S3;
typedef struct Pair { short lo; short hi; } Pair;
typedef union U3 { Pair pair; int whole; } U3;
typedef struct A3 { char c; U3 u; } A3;
typedef struct S4 {
    int head;
    union { unsigned all; float real; struct { unsigned lo : 4; unsigned mid : 8; unsigned top : 20; }; };
} S4;
int layout(int which);
int float_bits(float f);
long long double_bits(double d);
void fill_packed(SP *s);
int check_packed(const SP *s);
int check_s4(const S4 *s);
"""
LAYOUT = [
    "sizeof(U1)", "_Alignof(U1)", "offsetof(A1, u)", "sizeof(UP)", "_Alignof(UP)", "sizeof(SP)",
    "offsetof(SP, u)", "offsetof(SP, tail)", "sizeof(S3)", "offsetof(S3, v)", "offsetof(S3, after)",
    "sizeof(U3)", "_Alignof(U3)", "offsetof(A3, u)", "sizeof(S4)", "offsetof(S4, all)",
]
SOURCE = """#include <stddef.h>
#include "api.h"
int layout(int which)
{
    switch (which) {
""" + "".join(f"    case {k}: return (int){expr};\n" for k, expr in enumerate(LAYOUT)) + """    }
    return -1;
}
int float_bits(float f) { union { float f; int i; } u; u.f = f; return u.i; }
long long double_bits(double d) { union { double d; long long ll; } u; u.d = d; return u.ll; }
void fill_packed(SP *s) { s->a = 7; s->u.ll = 0x1122334455667788LL; s->tail = -2; }
int check_packed(const SP *s) { return s->a == 9 && s->u.d == 2.5 && s->tail == 300; }
int check_s4(const S4 *s)
{ return s->head == 1 && s->lo == 5 && s->mid == 200 && s->top == 1000 && s->all == 0x3E8C85u; }
"""
BODY = """    // 03a: sizes measured by NativeSharp, offsets and alignment by address differences.
    A1 a1;
    SP sp;
    S3 s3;
    A3 a3;
    S4 s4;
    if ((int)sizeof(U1) != layout(0) || (int)((byte*)&a1.u - (byte*)&a1) != layout(2) || layout(1) != 8) return 1;
    if ((int)sizeof(UP) != layout(3) || layout(4) != 1 || (int)sizeof(SP) != layout(5)) return 2;
    if (layout(6) != 1 || layout(7) != 9) return 3;
    if ((int)sizeof(S3) != layout(8) || (int)((byte*)&s3.v - (byte*)&s3) != layout(9)) return 4;
    if ((int)((byte*)&s3.after - (byte*)&s3) != layout(10) || (int)sizeof(U3) != layout(11)) return 4;
    if ((int)((byte*)&a3.u - (byte*)&a3) != layout(13) || layout(12) != 4) return 5;
    if ((int)sizeof(S4) != layout(14) || (int)((byte*)&s4.all - (byte*)&s4) != layout(15)) return 5;
    // 02a/03b: one member written, another read.
    U1 u;
    u.ll = 0;
    u.f = 1.5f;
    if (u.i != float_bits(1.5f) || u.bytes[3] != 63 || u.bytes[2] != 192) return 6;
    u.d = -0.25;
    if (u.ll != double_bits(-0.25) || u.bytes[7] != 191) return 7;
    u.ll = 0;
    u.bytes[0] = 255;
    u.bytes[1] = 1;
    if (u.i != 511) return 8;
    int target = 3;
    u.p = &target;
    if (*(int*)u.p != 3) return 9;
    // 02b: packed union inside a packed struct.
    fill_packed(&sp);
    if (sp.a != 7 || sp.u.ll != 1234605616436508552L || sp.u.c != 136 - 256 || sp.tail != -2) return 10;
    // The packed members are misaligned and have no address: their offsets are checked through the bytes.
    byte* raw = (byte*)&sp;
    if (raw[0] != 7 || raw[1] != 136 || raw[8] != 17 || raw[9] != 254 || raw[10] != 255) return 10;
    sp.a = 9;
    sp.u.d = 2.5;
    sp.tail = 300;
    if (check_packed(&sp) != 1) return 11;
    // 02c: union inside a struct and struct inside a union.
    s3.tag = 1;
    s3.v.f = 2.0f;
    s3.after = 5;
    if (s3.v.i != float_bits(2.0f) || s3.after != 5) return 12;
    U3 w;
    w.whole = 0;
    w.pair.lo = -1;
    w.pair.hi = 2;
    if (w.whole != 196607) return 13;
    a3.u.whole = 65539;
    if (a3.u.pair.lo != 3 || a3.u.pair.hi != 1) return 14;
    // 02d: anonymous union, promoted members and overlapping bit fields.
    s4.head = 1;
    s4.all = 0;
    s4.lo = 5;
    s4.mid = 200;
    s4.top = 1000;
    if (s4.all != 4099205 || check_s4(&s4) != 1) return 15;
    s4.real = 1.0f;
    if ((int)s4.all != float_bits(1.0f) || s4.lo != 0 || s4.top != 260096) return 16;
    return 0;
"""
REJECTION = ("bit-field-direccion-anonima", "S4 s; uint* q = &s.mid; return 0;",
             "bit fields have no address")


def c_spelling(text):
    return text.replace("(byte*)", "(unsigned char*)").replace("byte* raw", "unsigned char* raw").replace(
        "1234605616436508552L", "1234605616436508552LL")


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
    run([lib, "/nologo", "/out:cunion.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cunion.lib" />'


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
         "control.obj", "cunion.lib", sdk / "um/x64/kernel32.lib"], folder)
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
    (producer / "P.ns").write_text("namespace Unions { public class Probe { public static int Run<T>(T unused)\n{\n" +
                                   BODY + "} } }\n", encoding="utf-8")
    project(producer, "UnionPub", "P.ns", REF, library=True)
    result = build(producer, "UnionPub", opt, "lib", "UnionPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Unions.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/UnionPub.lib" />')
    expect_run(f"precompilado sin encabezado O{opt}", base, "C", opt)


def rejection():
    name, statements, text = REJECTION
    folder = root / "rechazo"
    folder.mkdir()
    c_library(folder)
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", "Main.ns", REF)
    result = build(folder, "Negative", "0", "exe", "out/Negative.exe")
    if observe:
        print("rechazo", name, result.returncode, errors(result), flush=True)
    elif result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{text}»:\n{result.stdout}")
    else:
        print("rechazo", name, "correcto", flush=True)


control()
for opt in opts:
    paths(opt)
rejection()
print("observación terminada" if observe else "Uniones C verificadas:", root)
