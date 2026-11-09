"""T08-003-04d1: a C `static` function (without `inline`) defined in a header.

C gives each translation unit its own copy of such a function, exactly like `static inline`. Checked:
- Clang control with the same header and calls;
- direct project with two headers: each function keeps its own static helper and its own function-local
  `static` counter, and a forward declaration `static int f(void);` before the body, O0-O3;
- a template of a precompiled producer consumed without the header, O0-O3, and two producers whose templates
  call homonymous `static` functions with different bodies (each keeps its own), O0-O3;
- rejections (O0): two headers of one project defining the same `static` function and a call to it, and a
  `static` function that is declared but never defined.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-static-encabezado-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER_X = """static int helper_x(int v) { return v + 1; }
static int outer_x(int v) { return helper_x(v) * 10; }
static int counter_x(void) { static int calls; return ++calls; }
static int forward_x(void);
static int forward_x(void) { return 41; }
"""
HEADER_Y = """static int helper_y(int v) { return v + 2; }
static int outer_y(int v) { return helper_y(v) * 10; }
static int counter_y(void) { static int calls = 100; return ++calls; }
"""
CHECKS = ["outer_x(2) == 30", "outer_y(2) == 40", "counter_x() == 1", "counter_x() == 2", "counter_y() == 101",
          "forward_x() == 41"]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def body():
    return "".join(f"    if (!({check})) return {index};\n" for index, check in enumerate(CHECKS, 1))


def project(folder, name, sources, references, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + references +
        "</ItemGroup></Project>", encoding="utf-8")


def compile_and_run(folder, name, opt):
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", f"{name}.nsproj", "-O" + opt,
                  "--no-artifact-cache", "-o", f"out/{name}.exe"], folder, check=False)
    if result.returncode:
        raise AssertionError(f"{folder.name}: compilación:\n{result.stdout}")
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


def headers(folder):
    folder.mkdir(parents=True)
    (folder / "x.h").write_text(HEADER_X, encoding="utf-8")
    (folder / "y.h").write_text(HEADER_Y, encoding="utf-8")


REFERENCES = ('<CppLibraryReference Header="x.h" Language="C" Standard="c11" />'
              '<CppLibraryReference Header="y.h" Language="C" Standard="c11" />')

control = root / "control"
headers(control)
(control / "Control.c").write_text('#include "x.h"\n#include "y.h"\n'
                                   "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
                                   "static int checks(void) {\n" + body() + "    return 0;\n}\n"
                                   "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", sdk / "um/x64/kernel32.lib"], control)
if run([control / "Control.exe"], control, check=False).returncode:
    raise AssertionError("control C falló")
print("control C correcto", flush=True)

for opt in opts:
    folder = root / f"directo-O{opt}"
    headers(folder)
    (folder / "Main.ns").write_text("int Main() {\n" + body() + "    return 0;\n}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], REFERENCES)
    code = compile_and_run(folder, "Direct", opt)
    if code:
        raise AssertionError(f"directo O{opt}: comprobación {code} falló")
    print("directo", f"O{opt}", "correcto", flush=True)

    folder = root / f"precompilado-O{opt}"
    headers(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace StaticHeader { public class Bridge { public static int Run<T>(T unused) {\n" + body() +
        "    return 0; } } }\n", encoding="utf-8")
    project(folder / "producer", "StaticHeader", ["Producer.ns"], REFERENCES, library=True)
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "StaticHeader.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", "StaticHeader.lib"], folder / "producer")
    (folder / "producer" / "x.h").unlink()
    (folder / "producer" / "y.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return StaticHeader.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/StaticHeader.lib" />')
    code = compile_and_run(folder, "Consumer", opt)
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto", flush=True)

    folder = root / f"dos-productores-O{opt}"
    for side, step in (("a", 1), ("b", 2)):
        producer = folder / side
        producer.mkdir(parents=True)
        (producer / "same.h").write_text(f"static int helper(int v) {{ return v + {step}; }}\n"
                                         "static int outer(int v) { return helper(v) * 10; }\n", encoding="utf-8")
        (producer / "Producer.ns").write_text(
            f"namespace Static{side.upper()} {{ public class Bridge {{ public static int Read<T>(T unused) "
            "{ return outer(2); } } }\n", encoding="utf-8")
        project(producer, f"StaticProducer{side.upper()}", ["Producer.ns"],
                '<CppLibraryReference Header="same.h" Language="C" Standard="c11" />', library=True)
        run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib",
             f"StaticProducer{side.upper()}.nsproj", "-O" + opt, "--no-artifact-cache", "-o",
             f"StaticProducer{side.upper()}.lib"], producer)
        (producer / "same.h").unlink()
    (folder / "Consumer.ns").write_text(
        "int Main() { return StaticA.Bridge.Read<int>(0) * 1000 + StaticB.Bridge.Read<int>(0); }\n",
        encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="a/StaticProducerA.lib" />'
            '<LibraryReference Include="b/StaticProducerB.lib" />')
    code = compile_and_run(folder, "Consumer", opt)
    if code != 30040:
        raise AssertionError(f"dos-productores O{opt}: se esperaba 30040, se obtuvo {code}")
    print("dos-productores", f"O{opt}", "correcto", flush=True)

REJECTIONS = [
    ("homonimas", "static int same(void) { return 1; }\n", "static int same(void) { return 2; }\n",
     "return same();", "ambiguous namespace function call to 'same'"),
    ("sin-definicion", "static int missing(void);\n", None, "return missing();", "missing"),
]
for name, header_x, header_y, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    folder.mkdir(parents=True)
    (folder / "x.h").write_text(header_x, encoding="utf-8")
    references = '<CppLibraryReference Header="x.h" Language="C" Standard="c11" />'
    if header_y:
        (folder / "y.h").write_text(header_y, encoding="utf-8")
        references += '<CppLibraryReference Header="y.h" Language="C" Standard="c11" />'
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], references)
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Negative.nsproj", "-O0",
                  "--no-artifact-cache", "-o", "out/N.exe"], folder, check=False)
    if "--observar" in sys.argv:
        print(name, result.returncode, [l for l in result.stdout.splitlines() if "error" in l][:2], flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("Funciones C static de encabezado verificadas:", root)
