"""T08-D14: named C members whose type is a struct or union without a tag.

`union U3 { struct { short lo; short hi; } pair; int whole; }` and, in a struct, `struct { int x, y; } point, other;`
(two members sharing one unnamed type), an unnamed union member, an unnamed struct nested in another unnamed
struct, and an array and a pointer of one unnamed type (`struct { int a, b; } pairs[2], *current;`). NativeSharp
gives each unnamed type a synthesized identity nested in its owner (it does not promote the
members, unlike an anonymous member). Locals, member access at every depth, copies between the members that share
the type, `var` of the member type, by-value and by-pointer exchange with C, and `sizeof` checked against Clang,
directly and through a template of a precompiled producer consumed without the header, O0-O3, plus a Clang
control. Rejections (O0): the members of a named member are not promoted (`u.lo`), and two members of different
unnamed types cannot be assigned to each other.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-miembro-sin-etiqueta-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef union U3 { struct { short lo; short hi; } pair; int whole; } U3;
typedef struct S
{
    struct { int x, y; } point, other;
    union { int i; float f; } val;
    struct { struct { char c; } inner; int z; } deep;
    struct { int a, b; } pairs[2], *current;
    int tag;
} S;
int c_u3(U3 u);
U3 c_make_u3(short lo, short hi);
int c_check(S *s);
void c_fill(S *s);
int c_size_s(void);
int c_size_u3(void);
"""
SOURCE = """#include "api.h"
int c_u3(U3 u) { return u.pair.lo + u.pair.hi * 10; }
U3 c_make_u3(short lo, short hi) { U3 u; u.pair.lo = lo; u.pair.hi = hi; return u; }
int c_check(S *s)
{
    return s->point.x == 5 && s->point.y == 6 && s->other.x == 5 && s->other.y == 7 && s->val.f == 1.5f &&
        s->deep.inner.c == 8 && s->deep.z == 9 && s->tag == 10 && s->pairs[0].a == 11 && s->pairs[1].b == 12 &&
        s->current == &s->pairs[1];
}
void c_fill(S *s)
{
    s->point.x = 50; s->point.y = 60; s->other.y = 70; s->val.i = 77; s->deep.inner.c = 80; s->deep.z = 90;
    s->current = &s->pairs[0]; s->current->b = 13;
}
int c_size_s(void) { return (int)sizeof(S); }
int c_size_u3(void) { return (int)sizeof(U3); }
"""
BODY = """    U3 u;
    u.whole = 0;
    u.pair.lo = 1;
    u.pair.hi = 2;
    if (u.whole != 131073 || c_u3(u) != 21) return 1;
    U3 m = c_make_u3(3, 4);
    if (m.pair.lo != 3 || m.pair.hi != 4 || (int)sizeof(U3) != c_size_u3()) return 2;
    S s;
    s.point.x = 5;
    s.point.y = 6;
    s.other = s.point;
    s.other.y = 7;
    s.val.f = 1.5f;
    s.deep.inner.c = 8;
    s.deep.z = 9;
    s.tag = 10;
    s.pairs[0].a = 11;
    s.pairs[1].b = 12;
    s.current = &s.pairs[1];
    if (s.current->b != 12) return 6;
    if (c_check(&s) != 1) return 3;
    c_fill(&s);
    if (s.point.x != 50 || s.other.y != 70 || s.val.i != 77 || s.deep.inner.c != 80 || s.deep.z != 90) return 4;
    if (s.current != &s.pairs[0] || s.pairs[0].b != 13 || s.current->a != 11) return 7;
    var p = s.point;
    if (p.x + p.y != 110 || (int)sizeof(S) != c_size_s()) return 5;
    return 0;
"""
C_BODY = BODY.replace("var p = s.point;", "__typeof__(s.point) p = s.point;")


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixture(folder):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:capi.lib", "api.obj"], folder)


REFERENCE = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib" />'


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def build(folder, name, opt, kind="exe", output=None):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output or f"out/{name}.exe"], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def build_and_run(folder, name, opt):
    result = build(folder, name, opt)
    if result.returncode:
        if observe:
            print(folder.name, errors(result), flush=True)
            return None
        raise AssertionError(f"{folder}: compilación:\n{result.stdout}")
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


control = root / "control"
fixture(control)
(control / "Control.c").write_text(
    '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\nint _fltused = 0;\n'
    "static int checks(void) {\n" + C_BODY + "}\nvoid mainCRTStartup(void) { ExitProcess(checks()); }\n",
    encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", "capi.lib", sdk / "um/x64/kernel32.lib"], control)
code = run([control / "Control.exe"], control, check=False).returncode
if code:
    raise AssertionError(f"control C: comprobación {code} falló")
print("control C correcto", flush=True)

for opt in opts:
    folder = root / f"directo-O{opt}"
    fixture(folder)
    (folder / "Main.ns").write_text("int Main() {\n" + BODY + "}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], REFERENCE)
    code = build_and_run(folder, "Direct", opt)
    if code:
        raise AssertionError(f"directo O{opt}: comprobación {code} falló")
    print("directo", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Members { public class Bridge { public static int Run<T>(T unused) {\n" + BODY + "} } }\n",
        encoding="utf-8")
    project(folder / "producer", "Members", ["Producer.ns"], REFERENCE, library=True)
    result = build(folder / "producer", "Members", opt, "lib", "Members.lib")
    if result.returncode:
        if observe:
            print("productor", f"O{opt}", errors(result), flush=True)
            continue
        raise AssertionError("productor:\n" + result.stdout)
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Members.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Members.lib" />')
    code = build_and_run(folder, "Consumer", opt)
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)

REJECTIONS = [
    ("sin-promocion", "U3 u; u.lo = 1; return 0;", "lo"),
    ("tipos-distintos", "S s; s.point = s.deep; return 0;", "cannot implicitly convert assignment RHS"),
]
for name, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixture(folder)
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], REFERENCE)
    result = build(folder, "Negative", "0")
    if observe:
        print(name, result.returncode, errors(result), flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("observación terminada" if observe else "Miembros C de tipo sin etiqueta verificados:", root)
