"""T08-004-04c: C global arrays and matrices, including arrays of qualified pointers.

Globals: `int[4]`, `int[2][3]`, `const int[3]`, `volatile int[2]`, `int *[2]`, `const char *[3]`,
`volatile int *[2]`, `int *const [2]`, `Pair[2]`, `const Pair[2][2]` and `double[2][2][2]`. NativeSharp reads
elements, writes the mutable ones (C checks the writes), reads through the pointers and takes a row as a
pointer (`&g_mat[1][0]`). Checked against a Clang control, directly, from a DLL and through a precompiled
template consumed without the header, O0-O3. Rejections (O0): writing an element of a `const` array or of `int *const [2]`,
writing through a `const char *` element, storing a `volatile int *` element in a plain `int*`, assigning a
whole array and indexing with a constant out of bounds.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-globales-arreglos-", dir=r"A:\nsc-interop-build"))
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

TYPES = "typedef struct Pair { int a; int b; } Pair;\n"
DECLARATIONS = [
    ("int", "g_arr", "[4]", "{1, 2, 3, 4}"),
    ("int", "g_mat", "[2][3]", "{{1, 2, 3}, {4, 5, 6}}"),
    ("const int", "g_carr", "[3]", "{7, 8, 9}"),
    ("volatile int", "g_varr", "[2]", "{10, 11}"),
    ("int *", "g_ptrs", "[2]", "{&g_arr[0], &g_arr[3]}"),
    ("const char *", "g_names", "[3]", '{"ab", "cd", "ef"}'),
    ("volatile int *", "g_vptrs", "[2]", "{&g_varr[0], &g_varr[1]}"),
    ("int *const", "g_cptrs", "[2]", "{&g_mat[0][0], &g_mat[1][2]}"),
    ("Pair", "g_pairs", "[2]", "{{1, 2}, {3, 4}}"),
    ("const Pair", "g_cpairs", "[2][2]", "{{{1, 1}, {2, 2}}, {{3, 3}, {4, 4}}}"),
    ("double", "g_cube", "[2][2][2]", "{{{0.5, 1.5}, {2.5, 3.5}}, {{4.5, 5.5}, {6.5, 7.5}}}"),
]


def header(dll=False):
    prefix = "__declspec(dllimport) " if dll else ""
    return TYPES + "".join(f"{prefix}extern {ctype} {name}{dims};\n" for ctype, name, dims, _ in DECLARATIONS) + \
        f"{prefix}int changed_by_c(void);\n{prefix}void change_from_c(void);\n"


def source(dll=False):
    prefix = "__declspec(dllexport) " if dll else ""
    body = TYPES + "".join(f"{prefix}{ctype} {name}{dims} = {value};\n" for ctype, name, dims, value in DECLARATIONS)
    body += (f"{prefix}int changed_by_c(void) {{ return g_arr[1] == 20 && g_mat[1][1] == 50 && g_varr[1] == 110 && "
             "g_pairs[1].b == 40 && g_cube[1][0][1] == 9.25 && *g_ptrs[1] == 44; }\n"
             f"{prefix}void change_from_c(void) {{ g_varr[0] = 100; g_arr[2] = 30; g_mat[0][2] = 33; }}\n")
    return body + ("int _fltused = 0;\n" if dll else "")


NS_BODY = """    if (g_arr[0] + g_arr[3] != 5 || g_mat[1][2] != 6 || g_mat[0][1] != 2) return 1;
    if (g_carr[2] != 9 || g_varr[1] != 11 || *g_ptrs[1] != 4 || g_names[1][1] != (sbyte)100) return 2;
    if (*g_vptrs[0] != 10 || *g_cptrs[1] != 6 || g_pairs[1].a != 3 || g_cpairs[1][0].b != 3) return 3;
    if (g_cube[1][1][0] != 6.5) return 4;
    int* row = &g_mat[1][0];
    if (row[0] != 4) return 5;
    g_arr[1] = 20; g_mat[1][1] = 50; g_varr[1] = 110; g_pairs[1].b = 40; g_cube[1][0][1] = 9.25; *g_ptrs[1] = 44;
    if (changed_by_c() != 1) return 6;
    change_from_c();
    if (*g_vptrs[0] != 100 || g_arr[2] != 30 || *g_cptrs[0] != 1 || g_mat[0][2] != 33) return 7;
    return 0;
"""
C_BODY = NS_BODY.replace("(sbyte)", "(signed char)").replace("int* row", "int *row")
# NativeSharp fixed arrays do not decay implicitly (`int* p = g_mat[1];` is rejected for any T[N], as for
# NativeSharp's own arrays): the row is taken as `&g_mat[1][0]`, which C also accepts.


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
        if observe:
            print(folder.name, [l for l in result.stdout.splitlines() if "error" in l][:4], flush=True)
            return None
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
        print("directo", mode, f"O{opt}", "correcto" if code == 0 else "observado", flush=True)
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Arrays { public class Bridge { public static int Run<T>(T unused) {\n" + NS_BODY + "} } }\n",
        encoding="utf-8")
    project(folder / "producer", "Arrays", ["Producer.ns"], reference(), library=True)
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Arrays.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", "Arrays.lib"], folder / "producer")
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Arrays.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Arrays.lib" />')
    code = build_and_run(folder, "Consumer", opt)
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)

REJECTIONS = [
    ("elemento-const", "g_carr[0] = 1; return 0;", "not assignable"),
    ("puntero-const-en-arreglo", "int x = 0; g_cptrs[0] = &x; return 0;", "not assignable"),
    ("escritura-por-const-char", "sbyte* p = (sbyte*)0; p = g_names[0]; return 0;", "mutability"),
    ("puntero-volatile-a-plano", "int* p = g_vptrs[0]; return p == null ? 1 : 0;",
     "would discard pointer pointee volatile qualification"),
    ("arreglo-entero", "int[4] other; g_arr = other; return 0;", "not assignable"),
    ("fuera-de-limites", "return g_arr[4];", "constant fixed-array index is outside [0, 4)"),
]
for name, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixture(folder)
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], reference())
    result = build(folder, "Negative", "0")
    if observe:
        print(name, result.returncode, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                         if "error" in l][:2], flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("observación terminada" if observe else "Arreglos globales C verificados:", root)
