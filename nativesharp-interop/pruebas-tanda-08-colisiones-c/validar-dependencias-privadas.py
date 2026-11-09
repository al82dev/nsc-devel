"""T08-D19: non-public members used by a published template travel in NSMETA.

A public generic method of a precompiled class uses members that are not public: a static field and a static method
named like a C global and a C function (`counter`, `twice`), a private field without collision, an internal static
field, and a private instance field and method. NSMETA publishes them with their accessibility, so the consumer
resolves the template's names to the class members, as the producer does, instead of failing ("unknown identifier")
or binding them silently to the C declarations. Checked O0-O3 with a static library and a NativeSharp DLL (the
private symbols are exported), with the consumer seeing the C header. Rejections (O0): consumer code still cannot
use the private and internal members, and a private member that no published template names is not published.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-dependencias-privadas-", dir=r"A:\nsc-interop-build"))
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

HEADER = "extern int counter;\nint twice(int v);\nint read_counter(void);\n"
SOURCE = """#include "api.h"
int counter = 40;
int twice(int v) { return 2 * v; }
int read_counter(void) { return counter; }
"""
PRODUCER = """namespace Pub
{
    public class Names
    {
        static int counter = 7;
        static int twice(int v) { return 7 * v; }
        static int other = 5;
        internal static int inner = 3;
        static int hidden = 99;
        int value = 11;
        int Scale(int v) { return v * value; }
        public static int Hidden() { return hidden; }
        public static int Run<T>(T unused)
        {
            Names names = new Names();
            int result = 0;
            if (counter != 7) result += 1;
            if (twice(2) != 14) result += 2;
            if (other != 5) result += 4;
            if (inner != 3) result += 8;
            if (names.value != 11 || names.Scale(2) != 22) result += 16;
            counter = 8;
            if (counter != 8 || read_counter() != 40) result += 32;
            return result;
        }
    }
}
"""
CONSUMER = """int Main()
{
    int code = Pub.Names.Run<int>(0);
    if (code != 0) return code;
    if (counter != 40 || twice(3) != 6 || Pub.Names.Hidden() != 99) return 64;
    return 0;
}
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:capi.lib", "api.obj"], folder)


REFERENCE = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib" />'


def producer(folder, opt, dll):
    c_library(folder)
    (folder / "P.ns").write_text(PRODUCER, encoding="utf-8")
    kind = "SharedLibrary" if dll else "Library"
    (folder / "Pub.nsproj").write_text(
        f"<Project><PropertyGroup><Name>Pub</Name><OutputType>{kind}</OutputType></PropertyGroup><ItemGroup>"
        '<Compile Include="P.ns" />' + REFERENCE + "</ItemGroup></Project>", encoding="utf-8")
    output = "out/Pub.dll" if dll else "Pub.lib"
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-" + ("dll" if dll else "lib"),
                  "Pub.nsproj", "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)
    if result.returncode:
        raise AssertionError(f"productor {folder}:\n{result.stdout}")
    return '<LibraryReference Include="p/out/Pub.lib" Runtime="p/out/Pub.dll" />' if dll else \
        '<LibraryReference Include="p/Pub.lib" />'


def consumer(folder, source, reference, opt, dll=False):
    # EN: the consumer sees the C header. With a static producer the C definitions come once, from the producer's
    #     library; a NativeSharp DLL does not export them, so the consumer links its own copy.
    # ES: el consumidor ve el encabezado C. Con un productor estatico las definiciones C llegan una vez, de su
    #     biblioteca; una DLL NativeSharp no las exporta, asi que el consumidor enlaza su propia copia.
    if dll:
        c_library(folder)
    else:
        (folder / "api.h").write_text(HEADER, encoding="utf-8")
    library = ' Library="capi.lib"' if dll else ""
    (folder / "C.ns").write_text(source, encoding="utf-8")
    (folder / "C.nsproj").write_text(
        '<Project><PropertyGroup><Name>C</Name></PropertyGroup><ItemGroup><Compile Include="C.ns" />' + reference +
        f'<CppLibraryReference Header="api.h" Language="C" Standard="c11"{library} /></ItemGroup></Project>',
        encoding="utf-8")
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "C.nsproj", "-O" + opt,
                "--no-artifact-cache", "-o", "exe/C.exe"], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


for dll in (False, True):
    kind = "dll" if dll else "estatica"
    for opt in opts:
        folder = root / f"{kind}-O{opt}"
        reference = producer(folder / "p", opt, dll)
        result = consumer(folder, CONSUMER, reference, opt, dll)
        if result.returncode:
            if observe:
                print(kind, f"O{opt}", errors(result), flush=True)
                continue
            raise AssertionError(f"consumidor {kind} O{opt}:\n{result.stdout}")
        code = run([folder / "exe" / "C.exe"], Path("A:/"), check=False).returncode
        if observe:
            print(kind, f"O{opt}", "salida", code, flush=True)
            continue
        if code:
            raise AssertionError(f"{kind} O{opt}: comprobación {code} falló")
        print(kind, f"O{opt}", "correcto", flush=True)

REJECTIONS = [
    ("campo-privado", "int Main() { return Pub.Names.counter; }", "member 'counter' is not accessible"),
    ("metodo-privado", "int Main() { return Pub.Names.twice(1); }", "member 'twice' is not accessible"),
    ("campo-interno", "int Main() { return Pub.Names.inner; }", "member 'inner' is not accessible"),
    ("no-publicado", "int Main() { return Pub.Names.hidden; }", "has no compatible member named 'hidden'"),
]
for name, source, expected in REJECTIONS:
    folder = root / "rechazos" / name
    reference = producer(folder / "p", "0", False)
    result = consumer(folder, source, reference, "0")
    if observe:
        print(name, result.returncode, errors(result), flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("observación terminada" if observe else "Dependencias privadas de plantillas verificadas:", root)
