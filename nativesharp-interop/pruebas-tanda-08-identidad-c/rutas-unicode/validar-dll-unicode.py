"""T08-D01: NativeSharp DLLs whose file name is not ASCII, bound through their UTF-16 path.

The Windows loader reads import-table DLL names through the ANSI code page, so such a DLL cannot be imported by
name. The producer publishes, under the import-library name, a loader library: every export keeps its import
shape (`X` and `__imp_X`) and is bound at its first call with LoadLibraryExW on the UTF-16 path beside the module
that links it, then GetProcAddress.

1. Five names (NFC, NFD, combining accent, U+6F22, Kelvin sign) with NSLink and with lld-link. The NativeSharp
   consumer calls methods with integer and floating arguments in registers and on the stack, a static field
   accessor and an exported C function, and runs. U+6F22 with NSLink runs in O0-O3 (producer and consumer).
2. A C consumer (Clang + lld-link) calls the exported C function through the plain declaration and through
   `__imp_`, from eight threads at once, so the first binding races.
3. Negative: without the DLL the consumer stops with STATUS_DLL_NOT_FOUND; with a DLL that lacks an export, the
   call to it stops with STATUS_ENTRYPOINT_NOT_FOUND after the earlier exports were bound.
4. The NativeSharp consumer build certifies the Runtime DLL by the UTF-16 name the loader carries.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unicodedata

sys.stdout.reconfigure(errors="backslashreplace")
root = Path(tempfile.mkdtemp(prefix="t08-dll-unicode-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lld = r"A:\al82dev-clang\bin\lld-link.exe"
kernel32 = r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
LINKERS = {"nslink": env, "lld": dict(env, NSC_LINKER=lld)}
NAMES = [
    ("nfc", unicodedata.normalize("NFC", "Lib\u00e9")),
    ("nfd", unicodedata.normalize("NFD", "Lib\u00e9")),
    ("combinante", "Liba\u0301"),
    ("cjk", "Lib\u6f22"),
    ("kelvin", "Lib\u212a"),
]
STATUS_DLL_NOT_FOUND = 0xC0000135
STATUS_ENTRYPOINT_NOT_FOUND = 0xC0000139

PRODUCER = """namespace Dl
{
    public class V
    {
        public static int Counter;
        public static int Get(int a, double b) { return a + (int)b; }
        public static double Mix(int a, double b, int c, double d, int e, double f) { return a + b + c + d + e + f; }
        public static export extern C int dl_add(int a, int b) { return a + b; }
    }
}
"""
PRODUCER_WITHOUT_MIX = PRODUCER.replace(
    "        public static double Mix(int a, double b, int c, double d, int e, double f) "
    "{ return a + b + c + d + e + f; }\n", "")
CONSUMER = """int Main()
{
    if (Dl.V.Get(40, 2.5) != 42) return 1;
    if (Dl.V.Mix(1, 1.5, 2, 2.5, 3, 3.5) != 13.5) return 2;
    Dl.V.Counter = 5;
    if (Dl.V.Counter != 5) return 3;
    if (Dl.V.dl_add(20, 22) != 42) return 4;
    return 0;
}
"""
C_CONSUMER = """int dl_add(int a, int b);
extern int (*__imp_dl_add)(int, int);
__declspec(dllimport) void* __stdcall CreateThread(void*, unsigned long long, unsigned long (__stdcall*)(void*),
                                                   void*, unsigned long, unsigned long*);
__declspec(dllimport) unsigned long __stdcall WaitForMultipleObjects(unsigned long, void* const*, int,
                                                                     unsigned long);
__declspec(dllimport) int __stdcall GetExitCodeThread(void*, unsigned long*);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
static unsigned long __stdcall worker(void* argument)
{
    int total = 0;
    for (int i = 0; i < 1000; ++i) total += (i & 1) ? dl_add(i, 1) : __imp_dl_add(i, 1);
    return total == 500500 ? 0 : 1;
}
void mainCRTStartup(void)
{
    void* threads[8];
    for (int i = 0; i < 8; ++i) threads[i] = CreateThread(0, 0, worker, 0, 0, 0);
    WaitForMultipleObjects(8, threads, 1, 0xFFFFFFFF);
    unsigned long failed = 0;
    for (int i = 0; i < 8; ++i)
    {
        unsigned long code = 1;
        GetExitCodeThread(threads[i], &code);
        failed |= code;
    }
    ExitProcess(failed);
}
"""


def run(args, cwd, environment=None):
    return subprocess.run([str(a) for a in args], cwd=cwd, env=environment or env, text=True, encoding="utf-8",
                          errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def write_producer(folder, text):
    (folder / "L.ns").write_text(text, encoding="utf-8")
    (folder / "L.nsproj").write_text('<Project><PropertyGroup><Name>Dl</Name><OutputType>SharedLibrary'
                                     '</OutputType></PropertyGroup><ItemGroup><Compile Include="L.ns" />'
                                     '</ItemGroup></Project>', encoding="utf-8")


def build_dll(folder, name, level, environment, text=PRODUCER):
    write_producer(folder, text)
    result = run([nsc, "--emit-project-oop-dll", "L.nsproj", level, "--no-artifact-cache", "-o",
                  f"out/{name}.dll"], folder, environment)
    dll = folder / "out" / (name + ".dll")
    if result.returncode or not dll.is_file() or not dll.with_suffix(".lib").is_file():
        raise AssertionError(f"DLL {folder.name} {level}:\n{result.stdout}")
    return dll


def build_consumer(folder, name, level, environment):
    (folder / "C.ns").write_text(CONSUMER, encoding="utf-8")
    (folder / "C.nsproj").write_text(
        '<Project><PropertyGroup><Name>C</Name></PropertyGroup><ItemGroup><Compile Include="C.ns" />'
        f'<LibraryReference Include="out/{name}.lib" Runtime="out/{name}.dll" /></ItemGroup></Project>',
        encoding="utf-8")
    result = run([nsc, "--emit-project-oop-exe", "C.nsproj", level, "--no-artifact-cache", "-o", "exe/C.exe"],
                 folder, environment)
    if result.returncode:
        raise AssertionError(f"consumidor {folder.name} {level}:\n{result.stdout}")
    if not (folder / "exe" / (name + ".dll")).is_file():
        raise AssertionError(f"consumidor {folder.name} {level}: la DLL no se publicó junto al ejecutable")
    return folder / "exe" / "C.exe"


def expect_exit(label, executable, expected):
    code = run([executable], Path("A:/")).returncode & 0xFFFFFFFF
    if code != expected:
        raise AssertionError(f"{label}: salida {code:#x}, se esperaba {expected:#x}")
    print(label, "correcto", flush=True)


def check_names():
    for linker, environment in LINKERS.items():
        for label, name in NAMES:
            levels = ("-O0", "-O1", "-O2", "-O3") if (linker, label) == ("nslink", "cjk") else ("-O0",)
            for level in levels:
                folder = root / f"{linker}-{label}{level}"
                folder.mkdir()
                build_dll(folder, name, level, environment)
                expect_exit(f"{linker} {label} {level}", build_consumer(folder, name, level, environment), 0)


def check_c_consumer():
    for linker, environment in LINKERS.items():
        name = "Lib\u6f22"
        folder = root / f"c-{linker}"
        folder.mkdir()
        dll = build_dll(folder, name, "-O2", environment)
        (folder / "c.c").write_text(C_CONSUMER, encoding="utf-8")
        compiled = run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-O2", "c.c", "-o", "c.obj"], folder)
        exe_dir = folder / "exe"
        exe_dir.mkdir()
        linked = run([lld, "/nologo", "/subsystem:console", "/entry:mainCRTStartup", "/nodefaultlib",
                      "/out:exe/c.exe", "c.obj", str(dll.with_suffix(".lib")), kernel32], folder)
        if compiled.returncode or linked.returncode:
            raise AssertionError(f"consumidor C {linker}:\n{compiled.stdout}\n{linked.stdout}")
        shutil.copy2(dll, exe_dir / dll.name)
        expect_exit(f"consumidor C con ocho hilos ({linker})", exe_dir / "c.exe", 0)


def check_negative():
    name = "Lib\u6f22"
    folder = root / "negativo"
    folder.mkdir()
    build_dll(folder, name, "-O0", env)
    consumer = build_consumer(folder, name, "-O0", env)
    published = consumer.parent / (name + ".dll")
    published.unlink()
    expect_exit("rechazo sin DLL (STATUS_DLL_NOT_FOUND)", consumer, STATUS_DLL_NOT_FOUND)
    other = root / "sin-mix"
    other.mkdir()
    reduced = build_dll(other, name, "-O0", env, PRODUCER_WITHOUT_MIX)
    shutil.copy2(reduced, published)
    expect_exit("rechazo sin exportacion (STATUS_ENTRYPOINT_NOT_FOUND)", consumer, STATUS_ENTRYPOINT_NOT_FOUND)


check_names()
check_c_consumer()
check_negative()
print("DLL NativeSharp con nombre no ASCII verificadas:", root)
