from pathlib import Path
import os
import subprocess
import sys

root = Path(r"A:\nsc-interop-build\t08-typedef-funcion-record")
root.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["PATH"] = r"A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
opt = sys.argv[1] if len(sys.argv) > 1 else "0"
incompatible = "--incompatible" in sys.argv or "--same-size-incompatible" in sys.argv
same_size = "--same-size-incompatible" in sys.argv
nested = "--nested" in sys.argv
changed_header = "--header-changed" in sys.argv
const_pointer = "--const-pointer" in sys.argv
const_mismatch = "--const-mismatch" in sys.argv
# T08-D10: each producer's template resolves `Callback` (and `CRecord`) in its own precompiled catalogue, so the
# incompatible, const-mismatch and changed-header variants, which were rejected with "conflicting C/C++ type alias
# 'Callback'", now compile and run like the compatible ones.
# T08-D10: la plantilla de cada productor resuelve `Callback` (y `CRecord`) en su propio catalogo precompilado; las
# variantes incompatible, de calificador distinto y de encabezado modificado, antes rechazadas, ahora compilan y
# se ejecutan como las compatibles.
variant = ("records incompatibles" if incompatible else "calificadores incompatibles" if const_mismatch else
           "encabezado modificado" if changed_header else "compatibles")

def run(args, folder):
    result = subprocess.run(args, cwd=folder, env=env, capture_output=True, text=True, errors="replace")
    if result.returncode:
        print(result.stdout + result.stderr)
        sys.exit("ERROR al ejecutar " + str(args))
    return result

for index in (1, 2):
    folder = root / str(index)
    folder.mkdir(exist_ok=True)
    field = ("float" if same_size else "long long") if incompatible and index == 2 else "int"
    record = (f"typedef struct Inner {{ {field} value; }} Inner; "
              "typedef struct CRecord { Inner inner; } CRecord;\n") if nested else (
              f"typedef struct CRecord {{ {field} value; }} CRecord;\n")
    (folder / "api.h").write_text(
        record + ("typedef int (*Callback)(const CRecord *, ...);\n"
                  if const_pointer or (const_mismatch and index == 1)
                  else "typedef int (*Callback)(CRecord *, ...);\n"), encoding="utf-8")
    (folder / "Producer.ns").write_text(f"""
namespace RecordAlias{index} {{
    public class Bridge {{
        public static int Test<T>(T unused) {{ Callback callback; return 0; }}
    }}
}}
""", encoding="utf-8")
    (folder / "Producer.nsproj").write_text(f"""<Project><PropertyGroup><Name>RecordAlias{index}</Name><OutputType>Library</OutputType></PropertyGroup><ItemGroup>
<Compile Include="Producer.ns" />
<CppLibraryReference Header="api.h" Language="C" Standard="c11" />
</ItemGroup></Project>""", encoding="utf-8")
    run([nsc, "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj",
         "-O" + opt, "--no-artifact-cache", "-o", f"RecordAlias{index}.lib"], folder)

if "--headers-hidden" in sys.argv:
    for index in (1, 2):
        (root / str(index) / "api.h").unlink()
if changed_header:
    (root / "1" / "api.h").write_text("/* changed after publication */\n", encoding="utf-8")

(root / "Consumer.ns").write_text("""
int Main() {
    return RecordAlias1.Bridge.Test<int>(0) + RecordAlias2.Bridge.Test<int>(0);
}
""", encoding="utf-8")
(root / "Consumer.nsproj").write_text("""<Project><PropertyGroup><Name>RecordConsumer</Name></PropertyGroup><ItemGroup>
<Compile Include="Consumer.ns" />
<LibraryReference Include="1/RecordAlias1.lib" />
<LibraryReference Include="2/RecordAlias2.lib" />
</ItemGroup></Project>""", encoding="utf-8")
exe = root / "Consumer.exe"
exe.unlink(missing_ok=True)
command = [nsc, "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj",
           "-O" + opt, "--no-artifact-cache", "-o", str(exe)]
result = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, errors="replace")
if result.returncode:
    print(result.stdout + result.stderr)
    sys.exit(result.returncode)
run([str(exe)], root)
print("Typedefs con puntero a record (" + variant + "): correcto O" + opt)
