"""T08-004-04a: integer and floating C globals with `const` and `volatile`.

Globals of every integer width, `_Bool`, `float` and `double`, plain, `const`, `volatile` and
`const volatile`, defined in C. NativeSharp reads them, writes the mutable ones (C observes the writes) and
reads again after C changes the volatile ones. Checked against a Clang control, directly with a static
library, with `__declspec(dllimport)` data from a DLL and through a precompiled template consumed without
the header, O0-O3. The IR keeps volatile loads and stores for the volatile globals. Rejections (O0): writes to
`const` globals and the address of a `volatile` global. Since T08-D06 the address of a `const` global is a
pointer to constant data, as in C; validar-direccion-const.py checks it.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-globales-escalares-", dir=r"A:\nsc-interop-build"))
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

# (C type, name, initial value, NativeSharp literal of the initial value, kind)
GLOBALS = [
    ("signed char", "g_sc", "-5", "(sbyte)-5", "plain"),
    ("unsigned char", "g_uc", "250", "(byte)250", "plain"),
    ("short", "g_s", "-300", "(short)-300", "plain"),
    ("unsigned short", "g_us", "65000", "(ushort)65000", "plain"),
    ("int", "g_i", "-70000", "-70000", "plain"),
    ("unsigned", "g_u", "4000000000u", "4000000000u", "plain"),
    ("long long", "g_ll", "-5000000000LL", "-5000000000L", "plain"),
    ("unsigned long long", "g_ull", "18000000000000000000ULL", "18000000000000000000UL", "plain"),
    ("_Bool", "g_b", "1", "true", "plain"),
    ("float", "g_f", "1.5f", "1.5f", "plain"),
    ("double", "g_d", "-2.25", "-2.25", "plain"),
    ("const int", "g_ci", "11", "11", "const"),
    ("const double", "g_cd", "3.5", "3.5", "const"),
    ("const unsigned short", "g_cus", "60000", "(ushort)60000", "const"),
    ("volatile int", "g_vi", "21", "21", "volatile"),
    ("volatile float", "g_vf", "4.5f", "4.5f", "volatile"),
    ("volatile double", "g_vd", "5.25", "5.25", "volatile"),
    ("volatile long long", "g_vll", "-7", "-7L", "volatile"),
    ("const volatile int", "g_cvi", "31", "31", "const-volatile"),
    ("const volatile unsigned char", "g_cvuc", "200", "(byte)200", "const-volatile"),
]


def header(dll=False):
    prefix = "__declspec(dllimport) " if dll else ""
    lines = [f"{prefix}extern {ctype} {name};" for ctype, name, _, _, _ in GLOBALS]
    lines += [f"{prefix}int changed_by_c(void);", f"{prefix}void change_volatiles(void);"]
    return "\n".join(lines) + "\n"


def source(dll=False):
    prefix = "__declspec(dllexport) " if dll else ""
    # The `const volatile` globals model a read-only view of storage that C changes: the definition (which
    # does not include the header) is mutable, as for a hardware register exposed read-only.
    lines = [f"{prefix}{ctype.replace('const volatile', 'volatile')} {name} = {value};"
             for ctype, name, value, _, _ in GLOBALS]
    lines.append(f"{prefix}int changed_by_c(void) {{ return g_sc == 6 && g_uc == 7 && g_s == 8 && g_us == 9 && "
                 "g_i == 10 && g_u == 11u && g_ll == 12 && g_ull == 13u && g_b == 0 && g_f == 0.5f && "
                 "g_d == 0.75 && g_vi == 14 && g_vf == 1.25f && g_vd == 1.75 && g_vll == 15; }")
    if dll:
        lines.append("int _fltused = 0;")
    lines.append(f"{prefix}void change_volatiles(void) {{ g_vi = 41; g_vf = 2.5f; g_vd = 3.25; g_vll = 42; "
                 "g_cvi = 43; g_cvuc = 44; }")
    return "\n".join(lines) + "\n"


READS = [f"{name} == {literal}" for _, name, _, literal, _ in GLOBALS]
WRITES = ("g_sc = 6; g_uc = 7; g_s = 8; g_us = 9; g_i = 10; g_u = 11; g_ll = 12; g_ull = 13; g_b = false; "
          "g_f = 0.5f; g_d = 0.75; g_vi = 14; g_vf = 1.25f; g_vd = 1.75; g_vll = 15;")
AFTER = ["g_vi == 41", "g_vf == 2.5f", "g_vd == 3.25", "g_vll == 42", "g_cvi == 43", "g_cvuc == 44",
         "*mutable_address() == 10"]
# `&` of a mutable C global is a plain `T*`; of a `const` global, a `T const*` (T08-D06).
NS_HELPERS = """    static int* mutable_address() { int* p = &g_i; return p; }
"""
C_HELPERS = """static int *mutable_address(void) { int *p = &g_i; return p; }
"""


def ns_body():
    checks = "".join(f"    if (!({read})) return {index};\n" for index, read in enumerate(READS, 1))
    after = "".join(f"    if (!({read})) return {index};\n" for index, read in enumerate(AFTER, 40))
    return (checks + "    " + WRITES + "\n    if (changed_by_c() != 1) return 30;\n    change_volatiles();\n" +
            after + "    return 0;\n")


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
c_body = (ns_body().replace("true", "1").replace("false", "0").replace("(sbyte)", "(signed char)")
          .replace("(byte)", "(unsigned char)").replace("(ushort)", "(unsigned short)")
          .replace("-5000000000L", "-5000000000LL").replace("18000000000000000000UL", "18000000000000000000ULL")
          .replace("-7L", "-7LL"))
(control / "Control.c").write_text('#include "api.h"\n'
                                   "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
                                   "int _fltused = 0;\n" + C_HELPERS + "static int checks(void) {\n" + c_body +
                                   "}\nvoid mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", "capi.lib", sdk / "um/x64/kernel32.lib"], control)
code = run([control / "Control.exe"], control, check=False).returncode
if code:
    raise AssertionError(f"control C: comprobación {code} falló")
print("control C correcto", flush=True)

ir = root / "ir"
fixture(ir)
(ir / "Main.ns").write_text("int Touch() { g_vi = g_vi + 1; g_vd = g_vd * 2.0; return g_i; }\n"
                            "int Main() { return Touch() == -70000 ? 0 : 1; }\n", encoding="utf-8")
project(ir, "Ir", ["Main.ns"], reference())
dump = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], ir)
(ir / "native.ll").write_text(dump.stdout, encoding="utf-8")
touch = re.findall(r"define [^\n]*Touch[^\n]*\{(.*?)\n\}", dump.stdout, re.S)
# Microsoft volatile semantics (the MSVC target default) add acquire/release ordering to the volatile access.
if not touch or not re.search(r"load (?:atomic )?volatile i32, ptr @g_vi", touch[0]) or \
        not re.search(r"store (?:atomic )?volatile i32 [^\n]*, ptr @g_vi", touch[0]) or \
        not re.search(r"load (?:atomic )?volatile double, ptr @g_vd", touch[0]) or \
        not re.search(r"store (?:atomic )?volatile double [^\n]*, ptr @g_vd", touch[0]) or \
        re.search(r"volatile i32, ptr @g_i\b", touch[0]):
    raise AssertionError("IR: accesos volatile inesperados para g_vi/g_vd/g_i:\n" +
                         (touch[0] if touch else dump.stdout))
print("IR: g_vi y g_vd con accesos volatile; g_i sin volatile", flush=True)

for opt in opts:
    for mode, dll in (("estatica", False), ("dll", True)):
        folder = root / f"directo-{mode}-O{opt}"
        fixture(folder, dll)
        (folder / "Main.ns").write_text("class Checks {\n" + NS_HELPERS + "    public static int Run() {\n" +
                                        ns_body() + "} }\nint Main() { return Checks.Run(); }\n",
                                        encoding="utf-8")
        project(folder, "Direct", ["Main.ns"], reference(dll))
        code = build_and_run(folder, "Direct", opt)
        if code:
            raise AssertionError(f"directo {mode} O{opt}: comprobación {code} falló")
        print("directo", mode, f"O{opt}", "correcto", flush=True)
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Scalars { public class Bridge {\n" + NS_HELPERS.replace("static", "public static") +
        "    public static int Run<T>(T unused) {\n" + ns_body() + "} } }\n", encoding="utf-8")
    project(folder / "producer", "Scalars", ["Producer.ns"], reference(), library=True)
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Scalars.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", "Scalars.lib"], folder / "producer")
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Scalars.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Scalars.lib" />')
    code = build_and_run(folder, "Consumer", opt)
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto", flush=True)

REJECTIONS = [
    ("escritura-const", "g_ci = 1; return 0;", "not assignable"),
    ("escritura-const-double", "g_cd = 1.0; return 0;", "not assignable"),
    ("escritura-const-volatile", "g_cvi = 1; return 0;", "not assignable"),
    # `int const* p = &g_ci;` is valid since T08-D06 (validar-direccion-const.py); a mutable pointer drops const.
    ("direccion-const-mutable", "int* p = &g_ci; return 0;", "would discard pointee constness"),
    ("direccion-volatile", "var p = &g_vi; return 0;", "cannot take the address of a qualified C/C++ global"),
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
print("Globales C escalares con calificadores verificadas:", root)
