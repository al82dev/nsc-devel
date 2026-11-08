from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-cache-runtime-", dir=r"A:\nsc-interop-build"))
output = root / "output"
output.mkdir()
compiler = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
clang = Path(r"A:\al82dev-clang\bin\clang.exe")
linker = Path(r"A:\al82dev-clang\bin\lld-link.exe")
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"

def run(command):
    result = subprocess.run([str(x) for x in command], cwd=root, env=env, text=True,
                            encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise AssertionError("fallo: " + str(command) + "\n" + result.stdout)
    return result.stdout

def leaf(value):
    (root / "leaf.c").write_text(f"__declspec(dllexport) int leaf(void) {{ return {value}; }}\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "leaf.c", "-o", "leaf.obj"])
    run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:leaf.dll", "/implib:leaf.lib", "leaf.obj"])

leaf(7)
(root / "bridge.c").write_text('__declspec(dllimport) int leaf(void);\n'
                               '__declspec(dllexport) int bridge(void) { return leaf(); }\n', encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "bridge.c", "-o", "bridge.obj"])
run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:bridge.dll", "/implib:bridge.lib", "bridge.obj", "leaf.lib"])
(root / "Api.h").write_text("__declspec(dllimport) int bridge(void);\n", encoding="utf-8")
(root / "Main.ns").write_text("int Main() { return bridge(); }\n", encoding="utf-8")
(root / "Main.nsproj").write_text('''<Project><PropertyGroup><Name>RuntimeCache</Name></PropertyGroup><ItemGroup>
<Compile Include="Main.ns" /><CppLibraryReference Header="Api.h" Language="C" Standard="c11" Library="bridge.lib" Runtime="bridge.dll" />
</ItemGroup></Project>''', encoding="utf-8")
exe = output / "Main.exe"
command = [compiler, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", root / "Main.nsproj",
           "-O" + (sys.argv[1] if len(sys.argv) > 1 else "0"), "--time",
           "--artifact-cache=" + str(root / "cache"), "-o", exe]

def build(reason, expected):
    for name in ("Main.exe", "bridge.dll", "leaf.dll"):
        (output / name).unlink(missing_ok=True)
    log = run(command)
    if "compilation-bundle-cache: " + reason not in log:
        raise AssertionError("cache esperada " + reason + ":\n" + log)
    actual = subprocess.run([exe], cwd=output, env=env).returncode
    if actual != expected:
        raise AssertionError(f"resultado esperado {expected}, obtenido {actual}")

build("not-found", 7)
build("hit", 7)
leaf(9)
build("not-found", 9)
build("hit", 9)
print("Caché con DLL transitiva: invalidación y restauración correctas;", root)
