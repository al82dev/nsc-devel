from pathlib import Path
import os
import subprocess
import sys
import uuid

root = Path(r"A:\nsc-interop-build\t08-tls-variadicas")
root.mkdir(parents=True, exist_ok=True)
clang = r"A:\al82dev-clang\bin\clang.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
env = os.environ.copy()
env["PATH"] = r"A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
crt = Path(r"A:\vc2026-runtime\lib\x64\release")
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
opt = sys.argv[1] if len(sys.argv) > 1 else "0"

def run(args, cwd):
    result = subprocess.run(args, cwd=cwd, env=env, capture_output=True, text=True, errors="replace")
    if result.returncode:
        print(result.stdout + result.stderr)
        print("ERROR", result.returncode, args)
        sys.exit(result.returncode)
    return result.stdout + result.stderr

for index, initial in ((1, 3), (2, 7)):
    folder = root / str(index)
    folder.mkdir(exist_ok=True)
    (folder / "api.h").write_text(
        "extern __declspec(thread) int tls_exported;\n"
        "__declspec(dllimport) int var_read(int count, ...);\n"
        "__declspec(dllimport) int var_mix(int marker, ...);\n", encoding="utf-8")
    (folder / "api.c").write_text(f"""
__declspec(thread) int tls_exported = {initial};
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllexport) int var_read(int count, ...) {{
    __builtin_va_list ap;
    __builtin_va_start(ap, count);
    int result = GetCurrentThreadId() ? tls_exported : -1000;
    for (int i = 0; i < count; ++i) result += __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return result;
}}
__declspec(dllexport) int var_mix(int marker, ...) {{
    __builtin_va_list ap;
    __builtin_va_start(ap, marker);
    int a = __builtin_va_arg(ap, int);
    double b = __builtin_va_arg(ap, double);
    int c = __builtin_va_arg(ap, int);
    double d = __builtin_va_arg(ap, double);
    double e = __builtin_va_arg(ap, double);
    __builtin_va_end(ap);
    return marker == {index} && a == 5 && b == 1.5 && c == 6 && d == 2.25 && e == 3.5
        ? tls_exported : -100;
}}
""", encoding="utf-8")
    (folder / "Producer.ns").write_text(f"""
namespace Variadic{index} {{
    public class Bridge {{
        public static int Access<T>(T unused, int value) {{ tls_exported = value; return tls_exported; }}
        public static int Read<T>(T unused) {{ return var_read(6, {index * 10 + 1}, {index * 10 + 2}, {index * 10 + 3}, {index * 10 + 4}, {index * 10 + 5}, {index * 10 + 6}); }}
        public static int Mix<T>(T unused) {{ return var_mix({index}, (byte)5, (float)1.5, (short)6, 2.25, (float)3.5); }}
    }}
}}
""", encoding="utf-8")
    (folder / "Producer.nsproj").write_text(f"""<Project><PropertyGroup><Name>Variadic{index}</Name><OutputType>Library</OutputType></PropertyGroup><ItemGroup>
<Compile Include="Producer.ns" />
<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="api.lib" Runtime="api.dll" />
</ItemGroup></Project>""", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/include:_tls_used", "/export:tls_exported,DATA",
         "/out:api.dll", "/implib:api.lib", "api.obj", str(crt / "libcmt.lib"), str(crt / "libcpmt.lib"),
         str(crt / "libvcruntime.lib"), str(sdk / "ucrt/x64/libucrt.lib"), str(sdk / "um/x64/kernel32.lib")], folder)
    values = ", ".join(str(index * 10 + i) for i in range(1, 7))
    expected = initial + sum(index * 10 + i for i in range(1, 7))
    (folder / "Control.c").write_text(
        "__declspec(dllimport) int var_read(int, ...);\n"
        "__declspec(dllimport) int var_mix(int, ...);\n"
        "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
        "int _fltused = 0;\n"
        f"void mainCRTStartup(void) {{ ExitProcess(var_read(6, {values}) != {expected} || var_mix({index}, (unsigned char)5, 1.5f, (short)6, 2.25, 3.5f) != {initial}); }}\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
         "Control.obj", "api.lib", str(sdk / "um/x64/kernel32.lib")], folder)
    run([str(folder / "Control.exe")], folder)
    run([nsc, "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj",
         "-O" + opt, "--no-artifact-cache", "-o", f"Variadic{index}.lib"], folder)

(root / "Consumer.ns").write_text("""
int Main() {
    if (Variadic1.Bridge.Access<int>(0, 41) != 41 || Variadic2.Bridge.Access<int>(0, 51) != 51) return 1;
    if (Variadic1.Bridge.Read<int>(0) != 122 || Variadic2.Bridge.Read<int>(0) != 192) return 2;
    if (Variadic1.Bridge.Mix<int>(0) != 41 || Variadic2.Bridge.Mix<int>(0) != 51) return 3;
    return 0;
}
""", encoding="utf-8")
(root / "Consumer.nsproj").write_text("""<Project><PropertyGroup><Name>VariadicConsumer</Name></PropertyGroup><ItemGroup>
<Compile Include="Consumer.ns" />
<LibraryReference Include="1/Variadic1.lib" />
<LibraryReference Include="2/Variadic2.lib" />
</ItemGroup></Project>""", encoding="utf-8")
out = root / "out"
out.mkdir(exist_ok=True)
cache = Path(r"A:\t8-cache") / uuid.uuid4().hex[:12]
if "--cache-larga" in sys.argv:
    cache = Path(r"A:\nsc-interop-build\cache-ruta-larga-para-reproducir-el-fallo-de-publicacion") / uuid.uuid4().hex[:12]
command = [nsc, "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj",
           "-O" + opt, "--time", "--artifact-cache=" + str(cache), "-o", "out/Consumer.exe"]
for attempt in range(2):
    (out / "Consumer.exe").unlink(missing_ok=True)
    for alias in out.glob("__nsc_tls_*.dll"):
        alias.unlink()
    log = run(command, root)
    marker = "compilation-bundle-cache: not-found" if attempt == 0 else "compilation-bundle-cache: hit"
    if marker not in log:
        sys.exit("ERROR: estado inesperado de cache\n" + log)
    if len(list(out.glob("__nsc_tls_*.dll"))) != 2:
        sys.exit("ERROR: no se publicaron dos DLL TLS")
    run([str(out / "Consumer.exe")], Path("A:/"))
print("Dos funciones variadicas homonimas con TLS y cache: correcto O" + opt)
