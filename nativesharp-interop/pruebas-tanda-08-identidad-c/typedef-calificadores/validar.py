from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-typedef-qual-", dir=r"A:\nsc-interop-build"))
nsc = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
clang = Path(r"A:\al82dev-clang\bin\clang.exe")
env = os.environ.copy()
env["PATH"] = str(nsc.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opt = sys.argv[1] if len(sys.argv) > 1 else "0"
cases = [
    ("volatile_equal", "volatile CRecord *", "volatile CRecord *", True),
    ("volatile_missing", "volatile CRecord *", "CRecord *", False),
    ("both_equal", "const volatile CRecord *", "const volatile CRecord *", True),
    ("volatile_removed", "const volatile CRecord *", "const CRecord *", False),
    ("top_level_volatile", "CRecord * volatile", "CRecord *", True),
    ("top_level_const", "CRecord * const", "CRecord *", True),
]

def execute(command, folder):
    return subprocess.run([str(x) for x in command], cwd=folder, env=env, text=True,
                          encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)

for name, first, second, compatible in cases:
    folder = root / name
    folder.mkdir()
    control = folder / "control.c"
    control.write_text("typedef struct CRecord { int value; } CRecord;\n" +
        f"typedef int (*Callback)({first}, ...);\n" +
        f"typedef int (*Callback)({second}, ...);\n", encoding="utf-8")
    c = execute([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only", control], folder)
    if (c.returncode == 0) != compatible:
        raise AssertionError("Clang contradice el control " + name + ":\n" + c.stdout)
    for index, arg in ((1, first), (2, second)):
        producer = folder / str(index)
        producer.mkdir()
        (producer / "api.h").write_text("typedef struct CRecord { int value; } CRecord;\n" +
            f"typedef int (*Callback)({arg}, ...);\n", encoding="utf-8")
        (producer / "Producer.ns").write_text(
            f"namespace RecordAlias{index} {{ public class Bridge {{ public static int Test<T>(T unused) "
            "{ Callback callback; return 0; } } }\n", encoding="utf-8")
        (producer / "Producer.nsproj").write_text(
            f'<Project><PropertyGroup><Name>RecordAlias{index}</Name><OutputType>Library</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Producer.ns" /><CppLibraryReference Header="api.h" Language="C" '
            'Standard="c11" /></ItemGroup></Project>', encoding="utf-8")
        result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj",
                          "-O" + opt, "--no-artifact-cache", "-o", f"RecordAlias{index}.lib"], producer)
        if result.returncode: raise AssertionError("falló el productor " + name + ":\n" + result.stdout)
        (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text(
        "int Main() { return RecordAlias1.Bridge.Test<int>(0) + RecordAlias2.Bridge.Test<int>(0); }\n", encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
        '<LibraryReference Include="1/RecordAlias1.lib" /><LibraryReference Include="2/RecordAlias2.lib" />'
        '</ItemGroup></Project>', encoding="utf-8")
    exe = folder / "Consumer.exe"
    result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj",
                      "-O" + opt, "--no-artifact-cache", "-o", exe], folder)
    # T08-D10: each producer's template resolves `Callback` in its own catalogue, so the consumer composes both
    # producers whether or not Clang would accept the two typedefs in one translation unit (control above).
    # T08-D10: cada plantilla resuelve `Callback` en el catalogo de su productor; el consumidor compone los dos
    # productores tanto si Clang acepta los dos typedef en una unidad (control de arriba) como si no.
    if result.returncode: raise AssertionError("NSC rechazó " + name + ":\n" + result.stdout)
    if subprocess.run([exe], cwd=folder, env=env).returncode: raise AssertionError("ejecución falló: " + name)
    print(name, "compatible" if compatible else "incompatible en C, compuesto por productor", flush=True)
print("Calificadores de firmas C verificados contra Clang en O" + opt + ";", root)
