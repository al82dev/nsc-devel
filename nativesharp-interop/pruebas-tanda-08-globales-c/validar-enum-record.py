"""T08-004-04b: C globals of enum, pointer-to-enum and record type.

Globals of a named enum (with a negative enumerator), an anonymous-typedef enum, a pointer to enum, a struct,
a union, a pointer to struct, and `const` and `volatile` records. NativeSharp reads them, writes the mutable
ones (C checks the writes) and reads what C changes afterwards. Checked against a Clang control, directly,
from a DLL and through templates of a precompiled producer consumed without the header, where the record or
enum type appears only as the type of the global (before the fix the consumer could not project those
globals: "C/C++ global type is not yet mapped"). Rejections (O0): writing a `const` record field, writing
the whole `const` record, and assigning a pointer to an enum global.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-globales-enum-record-", dir=r"A:\nsc-interop-build"))
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
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

TYPES = """typedef enum Color { Red = 1, Green = 2, Blue = 7, Minus = -3 } Color;
typedef enum { ModeA = 10, ModeB = 20 } Mode;
typedef struct Pair { int a; long long b; } Pair;
typedef union Number { int i; float f; } Number;
"""
DECLARATIONS = [("Color", "g_color", "Blue"), ("Mode", "g_mode", "ModeB"), ("Color *", "g_color_ptr", "&g_color"),
                ("Pair", "g_pair", "{4, 5000000000LL}"), ("Number", "g_number", "{.f = 1.5f}"),
                ("Pair *", "g_pair_ptr", "&g_pair"), ("const Pair", "g_cpair", "{6, 7}"),
                ("volatile Pair", "g_vpair", "{8, 9}")]


def header(dll=False):
    prefix = "__declspec(dllimport) " if dll else ""
    return TYPES + "".join(f"{prefix}extern {ctype} {name};\n" for ctype, name, _ in DECLARATIONS) + \
        f"{prefix}int changed_by_c(void);\n{prefix}void change_from_c(void);\n"


def source(dll=False):
    prefix = "__declspec(dllexport) " if dll else ""
    body = TYPES + "".join(f"{prefix}{ctype} {name} = {value};\n" for ctype, name, value in DECLARATIONS)
    body += (f"{prefix}int changed_by_c(void) {{ return g_color == Minus && g_mode == ModeA && g_pair.a == 40 && "
             "g_pair.b == -6000000000LL && g_number.i == 77 && g_vpair.a == 80; }\n"
             f"{prefix}void change_from_c(void) {{ g_color = Green; g_pair.b = 123; g_vpair.b = 90; "
             "g_number.f = 2.5f; }\n")
    return body + ("int _fltused = 0;\n" if dll else "")


NS_BODY = """    if (g_color != Blue || g_mode != ModeB || *g_color_ptr != Blue) return 1;
    if (g_pair.a != 4 || g_pair.b != 5000000000L || g_number.f != 1.5f) return 2;
    if (g_pair_ptr->a != 4 || g_cpair.a + g_cpair.b != 13 || g_vpair.a + g_vpair.b != 17) return 3;
    g_color = Minus; g_mode = ModeA; g_pair.a = 40; g_pair.b = -6000000000L; g_number.i = 77;
    g_vpair.a = 80;
    if (changed_by_c() != 1) return 4;
    change_from_c();
    if (*g_color_ptr != Green || g_pair_ptr->b != 123 || g_vpair.b != 90 || g_number.f != 2.5f) return 5;
    Pair copy = g_pair;
    copy.a = 1;
    if (g_pair.a != 40) return 6;
    g_pair = copy;
    if (g_pair.a != 1 || (int)Minus != -3) return 7;
    return 0;
"""
C_BODY = NS_BODY.replace("5000000000L", "5000000000LL").replace("6000000000L", "6000000000LL")


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


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def build(folder, name, opt):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", f"{name}.nsproj", "-O" + opt,
                "--no-artifact-cache", "-o", f"out/{name}.exe"], folder, check=False)


def build_and_run(folder, name, opt):
    result = build(folder, name, opt)
    if result.returncode:
        raise AssertionError(f"{folder.name}: compilación:\n{result.stdout}")
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


control = root / "control"
fixture(control)
(control / "Control.c").write_text('#include "api.h"\n'
                                   "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
                                   "int _fltused = 0;\nstatic int checks(void) {\n" + C_BODY + "}\n"
                                   "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", "capi.lib", sdk / "um/x64/kernel32.lib"], control)
code = run([control / "Control.exe"], control, check=False).returncode
if code:
    raise AssertionError(f"control C: comprobación {code} falló")
print("control C correcto", flush=True)

for opt in opts:
    for mode, dll in (("estatica", False), ("dll", True)):
        folder = root / f"directo-{mode}-O{opt}"
        fixture(folder, dll)
        (folder / "Main.ns").write_text("int Main() {\n" + NS_BODY + "}\n", encoding="utf-8")
        project(folder, "Direct", ["Main.ns"], reference(dll))
        code = build_and_run(folder, "Direct", opt)
        if code:
            raise AssertionError(f"directo {mode} O{opt}: comprobación {code} falló")
        print("directo", mode, f"O{opt}", "correcto", flush=True)
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    # One template per global, so that each record or enum type appears only through its global.
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Globals { public class Bridge {\n"
        "    public static int ReadColor<T>(T unused) { return (int)g_color; }\n"
        "    public static int ReadMode<T>(T unused) { return (int)g_mode; }\n"
        "    public static int ReadColorPtr<T>(T unused) { return g_color_ptr == null ? -1 : (int)*g_color_ptr; }\n"
        "    public static long ReadPair<T>(T unused) { return g_pair.a + g_pair.b; }\n"
        "    public static int ReadNumber<T>(T unused) { return g_number.i; }\n"
        "    public static int ReadPairPtr<T>(T unused) { return g_pair_ptr->a; }\n"
        "    public static long ReadConstPair<T>(T unused) { return g_cpair.a + g_cpair.b; }\n"
        "    public static long ReadVolatilePair<T>(T unused) { g_vpair.a = 30; return g_vpair.a + g_vpair.b; }\n"
        "    public static int Write<T>(T unused) { g_color = Green; g_pair.a = 41; return changed(); }\n"
        "    public static int changed() { return g_pair.a; }\n"
        "} }\n", encoding="utf-8")
    project(folder / "producer", "Globals", ["Producer.ns"], reference(), library=True)
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Globals.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", "Globals.lib"], folder / "producer")
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text(
        "int Main() {\n"
        "    if (Globals.Bridge.ReadColor<int>(0) != 7) return 1;\n"
        "    if (Globals.Bridge.ReadMode<int>(0) != 20) return 2;\n"
        "    if (Globals.Bridge.ReadColorPtr<int>(0) != 7) return 3;\n"
        "    if (Globals.Bridge.ReadPair<int>(0) != 5000000004L) return 4;\n"
        "    if (Globals.Bridge.ReadNumber<int>(0) != 1069547520) return 5;\n"
        "    if (Globals.Bridge.ReadPairPtr<int>(0) != 4) return 6;\n"
        "    if (Globals.Bridge.ReadConstPair<int>(0) != 13) return 7;\n"
        "    if (Globals.Bridge.ReadVolatilePair<int>(0) != 39) return 8;\n"
        "    if (Globals.Bridge.Write<int>(0) != 41 || Globals.Bridge.ReadColorPtr<int>(0) != 2) return 9;\n"
        "    return 0;\n}\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Globals.lib" />')
    code = build_and_run(folder, "Consumer", opt)
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto", flush=True)

REJECTIONS = [
    ("campo-const", "g_cpair.a = 1; return 0;", "not assignable"),
    ("record-const", "Pair p = g_pair; g_cpair = p; return 0;", "not assignable"),
    # C allows assigning another enum's constant (`g_color = ModeA;` builds, as in C); a pointer does not.
    ("puntero-a-enum", "g_color = g_pair_ptr; return 0;", "cannot implicitly convert assignment RHS"),
]
for name, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixture(folder)
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], reference())
    result = build(folder, "Negative", "0")
    if "--observar" in sys.argv:
        print(name, result.returncode, [l for l in result.stdout.splitlines() if "error" in l][:2], flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("Globales C de enum y record verificadas:", root)
