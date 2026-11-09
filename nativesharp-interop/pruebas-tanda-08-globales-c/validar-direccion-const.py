"""T08-D06: `&` of a `const` C global yields a pointer to constant data; writes through it are rejected.

C accepts `const int *p = &g_ci;` and rejects writing through `p` or initializing an `int *` with `&g_ci`
(Clang control). NativeSharp now does the same; before, it rejected the address ("cannot take the address of
a qualified C/C++ global"). Checked with `const int`, `const double`, a `const` record and its members (directly
and through a `const Pair *`), passed to C functions that take `const T *` and compared with the address C
reports. Directly with a static library, with `__declspec(dllimport)` data from a DLL and through a precompiled
template consumed without the header, O0-O3.
Negatives (O0, direct): writing through the pointer or through a member address, writing a member through the
record pointer, an `int*` or a mutable record pointer from those addresses. `&` of `volatile` and
`const volatile` globals and of a pointer-to-const global stay rejected: they need qualified address types.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-direccion-const-", dir=r"A:\nsc-interop-build"))
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


def header(dll):
    p = "__declspec(dllimport) " if dll else ""
    return f"""typedef struct Pair {{ int a; int b; }} Pair;
{p}extern const int g_ci;
{p}extern const double g_cd;
{p}extern const Pair g_cp;
{p}extern volatile int g_vi;
{p}extern const volatile int g_cvi;
{p}extern const int *g_pci;
{p}int read_int(const int *p);
{p}double read_double(const double *p);
{p}int read_pair(const Pair *p);
{p}const int *address_ci(void);
{p}const Pair *address_cp(void);
"""


def source(dll):
    e = "__declspec(dllexport) " if dll else ""
    return f"""typedef struct Pair {{ int a; int b; }} Pair;
{e}const int g_ci = 11;
{e}const double g_cd = 3.5;
{e}const Pair g_cp = {{4, 5}};
{e}volatile int g_vi = 21;
{e}volatile int g_cvi = 31;
{e}const int *g_pci = &g_ci;
{e}int read_int(const int *p) {{ return *p; }}
{e}double read_double(const double *p) {{ return *p; }}
{e}int read_pair(const Pair *p) {{ return p->a * 10 + p->b; }}
{e}const int *address_ci(void) {{ return &g_ci; }}
{e}const Pair *address_cp(void) {{ return &g_cp; }}
""" + ("int _fltused = 0;\n" if dll else "")


BODY = """    int const* p = &g_ci;
    if (*p != 11 || read_int(&g_ci) != 11 || read_int(p) != 11) return 1;
    var q = &g_ci;
    if (*q != 11 || q != address_ci()) return 2;
    double const* d = &g_cd;
    if (*d != 3.5 || read_double(&g_cd) != 3.5) return 3;
    Pair const* r = &g_cp;
    if (r->a != 4 || r->b != 5 || read_pair(&g_cp) != 45 || r != address_cp()) return 4;
    int const* member = &g_cp.b;
    int const* throughPointer = &r->a;
    if (*member != 5 || *throughPointer != 4 || read_int(&g_cp.a) != 4) return 5;
    return 0;
"""
NEGATIVES = [
    ("escritura-puntero", "int const* p = &g_ci; *p = 1; return 0;", "not assignable"),
    ("escritura-var", "var p = &g_ci; *p = 1; return 0;", "not assignable"),
    ("pierde-const", "int* p = &g_ci; return 0;", "would discard pointee constness"),
    ("record-mutable", "Pair* r = &g_cp; return 0;", "would discard pointee constness"),
    ("escritura-miembro", "var r = &g_cp; r->a = 1; return 0;", "not assignable"),
    ("escritura-direccion-miembro", "var m = &g_cp.b; *m = 1; return 0;", "not assignable"),
    ("miembro-pierde-const", "int* m = &g_cp.a; return 0;", "would discard pointee constness"),
    ("direccion-volatile", "var p = &g_vi; return 0;", "cannot take the address of a qualified C/C++ global"),
    ("direccion-const-volatile", "var p = &g_cvi; return 0;", "cannot take the address of a qualified C/C++ global"),
    ("direccion-puntero-const", "var p = &g_pci; return 0;", "cannot take the address of a qualified C/C++ global"),
]
C_NEGATIVES = [("escritura-puntero", "const int *p = &g_ci; *p = 1;"), ("pierde-const", "int *p = &g_ci;")]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixture(folder, dll=False):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(header(dll), encoding="utf-8")
    (folder / "api.c").write_text(source(dll), encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    if dll:
        run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:capi.dll", "/implib:capi.lib",
             "api.obj"], folder)
    else:
        run([lib, "/nologo", "/out:capi.lib", "api.obj"], folder)


def reference(dll=False):
    runtime = ' Runtime="capi.dll"' if dll else ""
    return f'<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib"{runtime} />'


def project(folder, name, items, library=False):
    kind = "<OutputType>Library</OutputType>" if library else ""
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>{kind}</PropertyGroup><ItemGroup>{items}</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind="exe", output=None):
    output = output or f"out/{name}.exe"
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def build_and_run(folder, name, opt, label):
    result = build(folder, name, opt)
    if result.returncode:
        raise AssertionError(f"{label}:\n{result.stdout}")
    code = subprocess.run([str(folder / "out" / f"{name}.exe")], cwd=folder / "out").returncode
    if code:
        raise AssertionError(f"{label}: comprobación {code} falló")
    print(label, "correcto", flush=True)


def c_control():
    folder = root / "control-c"
    fixture(folder)
    checks = BODY.replace("int const*", "const int *").replace("double const*", "const double *") \
        .replace("Pair const*", "const Pair *").replace("var q", "const int *q").replace("r->a", "r->a")
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned);\nint _fltused = 0;\n'
        "static int check(void)\n{\n" + checks + "}\nvoid mainCRTStartup(void) { ExitProcess(check()); }\n",
        encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "capi.lib", sdk / "um/x64/kernel32.lib"], folder)
    if subprocess.run([str(folder / "control.exe")], cwd=folder).returncode:
        raise AssertionError("control C: valores incorrectos")
    for name, statements in C_NEGATIVES:
        (folder / f"{name}.c").write_text(f'#include "api.h"\nvoid f(void) {{ {statements} }}\n', encoding="utf-8")
        result = run([clang, "-fsyntax-only", "-Werror", "--target=x86_64-pc-windows-msvc", "-std=c11",
                      f"{name}.c"], folder, check=False)
        if result.returncode == 0:
            raise AssertionError(f"Clang aceptó {name}")
    print("control C: lectura por puntero const correcta; escritura y pérdida de const rechazadas", flush=True)


def direct(opt, dll):
    label = f"{'dll' if dll else 'directo'} O{opt}"
    folder = root / label.replace(" ", "-")
    fixture(folder, dll)
    (folder / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(folder, "Direct", '<Compile Include="Main.ns" />' + reference(dll))
    if dll:
        (folder / "out").mkdir(exist_ok=True)
        (folder / "out" / "capi.dll").write_bytes((folder / "capi.dll").read_bytes())
    build_and_run(folder, "Direct", opt, label)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    producer = folder / "producer"
    fixture(producer)
    (producer / "P.ns").write_text("namespace ConstAddress { public class Bridge { public static int Run<T>(T unused)"
                                   " {\n" + BODY + "} } }\n", encoding="utf-8")
    project(producer, "ConstAddress", '<Compile Include="P.ns" />' + reference(), library=True)
    result = build(producer, "ConstAddress", opt, "lib", "ConstAddress.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (folder / "C.ns").write_text("int Main() { return ConstAddress.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "C", '<Compile Include="C.ns" /><LibraryReference Include="producer/ConstAddress.lib" />')
    build_and_run(folder, "C", opt, f"precompilado O{opt}")


def negatives():
    for name, statements, expected in NEGATIVES:
        folder = root / "rechazos" / name
        fixture(folder)
        (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
        project(folder, "Negative", '<Compile Include="Main.ns" />' + reference())
        result = build(folder, "Negative", "0")
        errors = [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:2]
        if observe:
            print(name, result.returncode, errors, flush=True)
            continue
        if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"{name}: se esperaba «{expected}»:\n{result.stdout}")
        print("rechazo", name, flush=True)


c_control()
for opt in opts:
    direct(opt, False)
    direct(opt, True)
    precompiled(opt)
negatives()
print("Dirección de globales C const verificada;", root)
