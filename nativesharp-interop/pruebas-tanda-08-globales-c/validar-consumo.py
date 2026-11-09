"""T08-004-06: consumption and resolution of C globals.

a) A consumer that has the header too: its own code and the producer's template use one object (the template
   writes, the consumer reads and writes, the template reads back).
b) The same template consumed with the header hidden (metadata only).
c) Two headers of one project that declare a homonymous global with another type are ambiguous; a
   NativeSharp local or parameter hides the C global only in its scope.
d) `static` globals of two homonymous headers (`same.h` in two folders): each producer keeps its own object
   through its templates, and one project that sees both is ambiguous.
Positive cases run in O0-O3; rejections in O0.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-globales-consumo-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def c_library(folder, source, name):
    (folder / f"{name}.c").write_text(source, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{name}.c", "-o", f"{name}.obj"], folder)
    run([lib, "/nologo", f"/out:{name}.lib", f"{name}.obj"], folder)


def emit_lib(folder, name, opt):
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", f"{name}.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", f"{name}.lib"], folder)


def exe(folder, name, opt):
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", f"{name}.nsproj", "-O" + opt,
                  "--no-artifact-cache", "-o", f"out/{name}.exe"], folder, check=False)
    code = run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode if result.returncode == 0 \
        else None
    return result, code


def expect_run(label, result, code):
    if observe:
        print(label, result.returncode, code, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                               if "error" in l][:3], flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def expect_reject(label, result, text):
    if observe:
        print(label, result.returncode, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                         if "error" in l][:3], flush=True)
        return
    if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{text}»:\n{result.stdout}")
    print(label, "rechazado", flush=True)


SHARED_HEADER = "extern int shared_counter;\nint shared_read(void);\n"
SHARED_SOURCE = "int shared_counter = 3;\nint shared_read(void) { return shared_counter; }\n"
TEMPLATE = ("namespace Shared { public class Bridge {\n"
            "    public static int Bump<T>(T unused) { shared_counter = shared_counter + 10; return shared_counter; }\n"
            "    public static int Read<T>(T unused) { return shared_counter; }\n} }\n")

for opt in opts:
    # a) and b): one producer, two consumers (header present / hidden).
    base = root / f"plantilla-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    (producer / "api.h").write_text(SHARED_HEADER, encoding="utf-8")
    c_library(producer, SHARED_SOURCE, "cshared")
    (producer / "Producer.ns").write_text(TEMPLATE, encoding="utf-8")
    project(producer, "Shared", ["Producer.ns"],
            '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cshared.lib" />', library=True)
    emit_lib(producer, "Shared", opt)
    with_header = base / "con-encabezado"
    with_header.mkdir()
    (with_header / "Consumer.ns").write_text(
        "int Main() {\n    if (Shared.Bridge.Bump<int>(0) != 13 || shared_counter != 13) return 1;\n"
        "    shared_counter = 20;\n    if (Shared.Bridge.Read<int>(0) != 20 || shared_read() != 20) return 2;\n"
        "    return 0;\n}\n", encoding="utf-8")
    project(with_header, "Consumer", ["Consumer.ns"], '<LibraryReference Include="../producer/Shared.lib" />'
            '<CppLibraryReference Header="../producer/api.h" Language="C" Standard="c11" />')
    expect_run(f"a con-encabezado O{opt}", *exe(with_header, "Consumer", opt))
    (producer / "api.h").rename(producer / "api.h.oculto")
    hidden = base / "sin-encabezado"
    hidden.mkdir()
    (hidden / "Consumer.ns").write_text(
        "int Main() { return Shared.Bridge.Bump<int>(0) == 13 && Shared.Bridge.Read<int>(0) == 13 ? 0 : 1; }\n",
        encoding="utf-8")
    project(hidden, "Consumer", ["Consumer.ns"], '<LibraryReference Include="../producer/Shared.lib" />')
    expect_run(f"b sin-encabezado O{opt}", *exe(hidden, "Consumer", opt))

    # c) local and parameter shadowing, directly.
    shadow = root / f"sombra-O{opt}"
    shadow.mkdir()
    (shadow / "api.h").write_text("extern int g_value;\n", encoding="utf-8")
    c_library(shadow, "int g_value = 7;\n", "cshadow")
    (shadow / "Main.ns").write_text(
        "class S { public static int Param(int g_value) { return g_value; } }\n"
        "int Main() {\n    {\n        int g_value = 1;\n        if (g_value != 1) return 1;\n    }\n"
        "    if (g_value != 7 || S.Param(2) != 2) return 2;\n    g_value = 8;\n"
        "    for (int g_value = 0; g_value < 1; g_value++) { }\n    return g_value == 8 ? 0 : 3;\n}\n",
        encoding="utf-8")
    project(shadow, "Shadow", ["Main.ns"],
            '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cshadow.lib" />')
    expect_run(f"c sombra O{opt}", *exe(shadow, "Shadow", opt))

    # d) static globals of two homonymous headers, one per producer.
    statics = root / f"static-O{opt}"
    for side, start in (("a", 100), ("b", 200)):
        folder = statics / side
        folder.mkdir(parents=True)
        (folder / "same.h").write_text(f"static int counter = {start};\n", encoding="utf-8")
        (folder / "Producer.ns").write_text(
            f"namespace Static{side.upper()} {{ public class Bridge {{ public static int Next<T>(T unused) "
            "{ counter = counter + 1; return counter; } } }\n", encoding="utf-8")
        project(folder, f"Static{side.upper()}", ["Producer.ns"],
                '<CppLibraryReference Header="same.h" Language="C" Standard="c11" />', library=True)
        emit_lib(folder, f"Static{side.upper()}", opt)
        (folder / "same.h").unlink()
    (statics / "Consumer.ns").write_text(
        "int Main() {\n    if (StaticA.Bridge.Next<int>(0) != 101 || StaticB.Bridge.Next<int>(0) != 201) return 1;\n"
        "    if (StaticA.Bridge.Next<int>(0) != 102 || StaticB.Bridge.Next<int>(0) != 202) return 2;\n"
        "    return 0;\n}\n", encoding="utf-8")
    project(statics, "Consumer", ["Consumer.ns"], '<LibraryReference Include="a/StaticA.lib" />'
            '<LibraryReference Include="b/StaticB.lib" />')
    expect_run(f"d static O{opt}", *exe(statics, "Consumer", opt))

# c) and d) rejections.
rejections = root / "rechazos"
for name, headers, statements, text in [
    ("dos-encabezados-otro-tipo", {"x.h": "extern int dup;\n", "y.h": "extern long long dup;\n"},
     "return (int)dup;", "cannot import C/C++ global 'dup': incompatible C/C++ global declarations"),
    ("static-homonimas", {"dirA/same.h": "static int counter = 1;\n", "dirB/same.h": "static int counter = 2;\n"},
     "return counter;", "cannot import C/C++ global 'counter': ambiguous C internal global origins"),
]:
    folder = rejections / name
    refs = ""
    for header, text_h in headers.items():
        (folder / header).parent.mkdir(parents=True, exist_ok=True)
        (folder / header).write_text(text_h, encoding="utf-8")
        refs += f'<CppLibraryReference Header="{header}" Language="C" Standard="c11" />'
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], refs)
    result, _ = exe(folder, "Negative", "0")
    expect_reject(f"rechazo {name}", result, text)
print("observación terminada" if observe else "Consumo y resolución de globales C verificados:", root)
