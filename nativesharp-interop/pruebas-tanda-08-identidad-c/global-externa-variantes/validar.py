"""T08-002-09m7d: remaining qualifiers and linkage variants of a C external global shared by the templates
of two precompiled libraries (consumed without the producers' headers).

Each case gives the declaration seen by producer A, the one seen by producer B, the C definition linked
into the consumer and the expression both templates read. Compatible declarations must link and return
the expected value in O0-O3; incompatible ones must be rejected without joining them (O0). A DLL case
covers `__declspec(dllimport)` data. Mode `--observar` prints the outcome of every case without asserting.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-global-externa-variantes-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

# (name, header A, header B, C definition, read expression, expected value or None for a rejection)
CASES = [
    ("const-volatile", "extern const volatile int g;", "extern const volatile int g;", "const volatile int g = 21;",
     "g", 21),
    ("puntero-doble", "extern int **pp;", "extern int **pp;", "int v = 22; int *p = &v; int **pp = &p;", "**pp", 22),
    ("arreglo", "extern int arr[4];", "extern int arr[4];", "int arr[4] = {1, 2, 23, 4};", "arr[2]", 23),
    ("puntero-restrict", "extern int *restrict rp;", "extern int *restrict rp;", "int rv = 25; int *restrict rp = &rv;",
     "*rp", 25),
    ("puntero-const-a-const", "extern const int *const cpc;", "extern const int *const cpc;",
     "static const int cv = 26; const int *const cpc = &cv;", "*cpc", 26),
    ("alineado", "extern _Alignas(16) int aligned;", "extern _Alignas(16) int aligned;",
     "_Alignas(16) int aligned = 27;", "aligned", 27),
    ("rechazo-arreglo", "extern int arr[4];", "extern int arr[5];", "int arr[5] = {0};", "arr[0]", None),
    ("rechazo-const-volatile", "extern const volatile int g;", "extern const int g;", "const int g = 1;", "g", None),
    ("rechazo-restrict", "extern int *restrict rp;", "extern int *rp;", "int *rp = 0;", "rp == null ? 1 : 0", None),
]

# Known gaps recorded in the tracking (not asserted here): `extern int open_arr[];` is not imported
# ("C/C++ global type is not yet mapped"), and `extern const int **pp;` in producer B is not imported either,
# so B's template silently binds to producer A's `int **pp` instead of being rejected.
GAPS = [
    ("arreglo-incompleto", "extern int open_arr[];", "extern int open_arr[];", "int open_arr[3] = {5, 24, 6};",
     "open_arr[1]", 24),
    ("rechazo-puntero-doble", "extern int **pp;", "extern const int **pp;", "int **pp = 0;", "pp == null ? 1 : 0",
     None),
]


def run(args, cwd):
    return subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8", errors="replace",
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def build_case(name, header_a, header_b, definition, expression, opt, dll=False):
    folder = root / f"{name}-O{opt}"
    for side, header in (("a", header_a), ("b", header_b)):
        (folder / side).mkdir(parents=True)
        (folder / side / "same.h").write_text(header + "\n", encoding="utf-8")
        (folder / side / "Producer.ns").write_text(
            f"namespace Inline{side.upper()} {{ public class Bridge {{ public static int Read<T>(T value) "
            f"{{ return {expression}; }} }} }}\n", encoding="utf-8")
        reference = ('<CppLibraryReference Header="same.h" Language="C" Standard="c11" Library="../fixture.lib" '
                     'Runtime="../fixture.dll" />' if dll else
                     '<CppLibraryReference Header="same.h" Language="C" Standard="c11" />')
        (folder / side / "Producer.nsproj").write_text(
            f"<Project><PropertyGroup><Name>InlineProducer{side.upper()}</Name><OutputType>Library</OutputType>"
            f'</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />{reference}</ItemGroup></Project>',
            encoding="utf-8")
    (folder / "fixture.c").write_text(definition + "\n", encoding="utf-8")
    result = run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "fixture.c", "-o", "fixture.obj"],
                 folder)
    if result.returncode:
        raise AssertionError(f"{name}: clang fixture.c:\n" + result.stdout)
    if dll:
        result = run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:fixture.dll",
                      "/implib:fixture.lib", "fixture.obj"], folder)
    else:
        result = run([lib, "/nologo", "/out:fixture.lib", "fixture.obj"], folder)
    if result.returncode:
        raise AssertionError(f"{name}: biblioteca del fixture:\n" + result.stdout)
    for side in ("a", "b"):
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj",
                      "-O" + opt, "--no-artifact-cache", "-o", f"InlineProducer{side.upper()}.lib"], folder / side)
        if result.returncode:
            return "productor " + side, result.stdout
        (folder / side / "same.h").unlink()
    (folder / "Consumer.ns").write_text(
        "int Main() { return InlineA.Bridge.Read<int>(0) * 1000 + InlineB.Bridge.Read<int>(0); }\n", encoding="utf-8")
    runtime = ' Runtime="fixture.dll"' if dll else ""
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>InlineConsumer</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Consumer.ns" /><LibraryReference Include="a/InlineProducerA.lib" />'
        '<LibraryReference Include="b/InlineProducerB.lib" />'
        f'<CppLibraryReference Header="consumer.h" Language="C" Standard="c11" Library="fixture.lib"{runtime} />'
        '</ItemGroup></Project>', encoding="utf-8")
    (folder / "consumer.h").write_text(header_a + "\n", encoding="utf-8")
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj", "-O" + opt,
                  "--no-artifact-cache", "-o", "out/Consumer.exe"], folder)
    if result.returncode:
        return "consumidor", result.stdout
    code = subprocess.run([str(folder / "out" / "Consumer.exe")], cwd=Path("A:/")).returncode
    return "ejecución", code


DLL_CASE = ("dllimport", "__declspec(dllimport) extern int dll_global;", "__declspec(dllimport) extern int dll_global;",
            "__declspec(dllexport) int dll_global = 28;", "dll_global", 28)

for opt in opts:
    for name, header_a, header_b, definition, expression, expected in CASES + [DLL_CASE] + (GAPS if observe else []):
        if expected is None and opt != "0":
            continue
        stage, detail = build_case(name, header_a, header_b, definition, expression, opt, dll=name == "dllimport")
        if observe:
            text = detail if isinstance(detail, int) else [line.split(": error: ")[-1][:160]
                                                            for line in detail.splitlines() if "error" in line][:2]
            print(name, f"O{opt}", stage, text, flush=True)
            continue
        if expected is None:
            if stage != "consumidor" or "internal error" in detail or (
                    "ambiguous namespace state reference" not in detail and "conflicting" not in detail):
                raise AssertionError(f"{name}: se esperaba un rechazo del consumidor; etapa {stage}:\n{detail}")
            print(name, "rechazado", flush=True)
        elif stage != "ejecución" or detail != expected * 1001:
            raise AssertionError(f"{name} O{opt}: se esperaba {expected * 1001}; etapa {stage}:\n{detail}")
        else:
            print(name, f"O{opt}", "correcto", flush=True)
print("observación terminada" if observe else "Variantes de global externa compartida verificadas;", root)
