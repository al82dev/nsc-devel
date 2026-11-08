from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-dll-producto-", dir=r"A:\nsc-interop-build"))
compiler = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
clang = Path(r"A:\al82dev-clang\bin\clang.exe")
linker = Path(r"A:\al82dev-clang\bin\lld-link.exe")
out = root / "out"
out.mkdir()
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opt = sys.argv[1] if len(sys.argv) > 1 else "0"

def run_setup(command, cwd):
    result = subprocess.run([str(item) for item in command], cwd=cwd, env=env, text=True,
                            encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise AssertionError("falló preparación: " + str(command) + "\n" + result.stdout)

for directory, namespace, producer, initial in (("uno", "TlsLib", "TlsProducer", 3),
                                                 ("dos", "TlsLib2", "TlsProducer2", 7)):
    folder = root / directory
    folder.mkdir()
    (folder / "Api.c").write_text(
        f'__declspec(thread) int tls_exported = {initial};\n'
        '__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);\n'
        '__declspec(dllexport) int ensure_import(void) { return (int)GetCurrentThreadId(); }\n', encoding="utf-8")
    (folder / "Api.h").write_text("extern __declspec(thread) int tls_exported;\n", encoding="utf-8")
    run_setup([clang, "-c", "--target=x86_64-pc-windows-msvc", "Api.c", "-o", "Api.obj"], folder)
    run_setup([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/include:_tls_used",
        "/export:tls_exported,DATA", "/out:api.dll", "/implib:api.lib", "Api.obj",
        r"A:\vc2026-runtime\lib\x64\release\libcmt.lib",
        r"A:\vc2026-runtime\lib\x64\release\libcpmt.lib",
        r"A:\vc2026-runtime\lib\x64\release\libvcruntime.lib",
        r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib",
        r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"], folder)
    (folder / "Producer.ns").write_text(
        f'namespace {namespace} {{ public class Bridge {{ public static int Access<T>(T unused, int value) '
        '{ if (value != 0) tls_exported = value; return tls_exported; } } }\n', encoding="utf-8")
    (folder / "Producer.nsproj").write_text(
        f'<Project><PropertyGroup><Name>{producer}</Name><OutputType>Library</OutputType></PropertyGroup><ItemGroup>'
        '<Compile Include="Producer.ns" /><CppLibraryReference Header="Api.h" Language="C" Standard="c11" '
        'Library="api.lib" Runtime="api.dll" /></ItemGroup></Project>', encoding="utf-8")
    run_setup([compiler, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj",
               "-O" + opt, "--no-artifact-cache", "-o", producer + ".lib"], folder)
    (folder / "Api.h").unlink()
(root / "Consumer.ns").write_text('''public class Api {
  public static export extern C int Check() {
    if (TlsLib.Bridge.Access<int>(0, 0) != 3 || TlsLib2.Bridge.Access<int>(0, 0) != 7) return 1;
    if (TlsLib.Bridge.Access<int>(0, 41) != 41 || TlsLib2.Bridge.Access<int>(0, 51) != 51) return 2;
    return TlsLib.Bridge.Access<int>(0, 0) == 41 && TlsLib2.Bridge.Access<int>(0, 0) == 51 ? 0 : 3;
  }
  public static export extern C int Read() {
    return TlsLib.Bridge.Access<int>(0, 0) == 41 && TlsLib2.Bridge.Access<int>(0, 0) == 51 ? 0 : 4;
  }
}\n''', encoding="utf-8")
(root / "Consumer.nsproj").write_text('''<Project><PropertyGroup><Name>TlsConsumer</Name>
<OutputType>SharedLibrary</OutputType></PropertyGroup><ItemGroup>
<Compile Include="Consumer.ns" /><LibraryReference Include="uno/TlsProducer.lib" />
<LibraryReference Include="dos/TlsProducer2.lib" /></ItemGroup></Project>''', encoding="utf-8")
dll = out / "Consumer.dll"
command = [str(compiler), r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-dll",
           str(root / "Consumer.nsproj"), "-O" + opt, "--time",
           "--artifact-cache=" + str(root / "cache"), "-o", str(dll)]
for reason in ("not-found", "hit"):
    for file in out.iterdir():
        if file.is_file(): file.unlink()
    result = subprocess.run(command, cwd=root, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode or "compilation-bundle-cache: " + reason not in result.stdout:
        raise AssertionError("falló compilación/cache " + reason + ":\n" + result.stdout)
    if len(list(out.glob("__nsc_tls_*.dll"))) != 2:
        raise AssertionError("no se empaquetaron ambas DLL con identidad distinta")
    script = '''import ctypes, sys, threading
d = ctypes.WinDLL(sys.argv[1])
if d.Check(): sys.exit(1)
barrier = threading.Barrier(3)
results = []
def worker():
    barrier.wait()
    results.append(d.Check())
threads = [threading.Thread(target=worker) for _ in range(2)]
for t in threads: t.start()
barrier.wait()
for t in threads: t.join()
sys.exit(0 if results == [0, 0] and d.Read() == 0 else 2)
'''
    run = subprocess.run([sys.executable, "-c", script, str(dll)], cwd="A:/", env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    if run.returncode:
        raise AssertionError(f"falló invocación desde otro directorio ({run.returncode}):\n{run.stdout}")
print("DLL NativeSharp, dos runtimes homónimos y caché: correctos;", root)
