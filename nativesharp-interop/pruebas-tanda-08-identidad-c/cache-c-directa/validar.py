from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-cache-c-", dir=r"A:\nsc-interop-build"))
compiler = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
clang_root = r"A:\al82dev-clang"
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + ";" + clang_root + r"\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = clang_root + r"\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
(root / "Main.nsproj").write_text('''<Project><PropertyGroup><Name>CacheC</Name></PropertyGroup><ItemGroup>
<Compile Include="Main.ns" /><CppLibraryReference Header="Api.h" Language="C" Standard="c11" />
</ItemGroup></Project>''', encoding="utf-8")
(root / "Main.ns").write_text("int Main() { return getValue(); }\n", encoding="utf-8")
api = root / "Api.h"
api.write_text('''#if __has_include("optional.h")
#define BASE 13
#else
#define BASE 11
#endif
#include "nested.h"
static inline int getValue(void) { return BASE + NEST; }
''', encoding="utf-8")
nested = root / "nested.h"
nested.write_text("#define NEST 1\n", encoding="utf-8")
exe = root / "Main.exe"
command = [str(compiler), "--cpp-clang-root=" + clang_root, "--emit-project-oop-exe",
           str(root / "Main.nsproj"), "-O" + (sys.argv[1] if len(sys.argv) > 1 else "0"),
           "--time", "--artifact-cache=" + str(root / "cache"),
           "-o", str(exe)]

def build(reason, value):
    exe.unlink(missing_ok=True)
    result = subprocess.run(command, cwd=root, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode or "compilation-bundle-cache: " + reason not in result.stdout:
        raise AssertionError(f"expected cache {reason}, exit {result.returncode}:\n{result.stdout}")
    outcome = subprocess.run([str(exe)], cwd=root, env=env).returncode
    if outcome != value:
        raise AssertionError(f"expected program exit {value}, got {outcome}")

build("not-found", 12)
build("hit", 12)
(root / "optional.h").write_text("/* existence only */\n", encoding="utf-8")
build("not-found", 14)
build("hit", 14)
nested.write_text("#define NEST 3\n", encoding="utf-8")
build("not-found", 16)
build("hit", 16)
nested.write_text('#error invalid_transitive_header\n', encoding="utf-8")
failed = subprocess.run(command, cwd=root, env=env, text=True, encoding="utf-8",
                        errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
if failed.returncode == 0 or "invalid_transitive_header" not in failed.stdout:
    raise AssertionError("the cached result hid an invalid transitive header:\n" + failed.stdout)
nested.write_text("#define NEST 3\n", encoding="utf-8")
build("hit", 16)
with api.open("a", encoding="utf-8") as source:
    source.write("static const char *unused_time = __TIME__;\n")
build("disabled", 16)
build("disabled", 16)
print("Cache C directa: aciertos, dependencias y macro inestable correctos;", root)
