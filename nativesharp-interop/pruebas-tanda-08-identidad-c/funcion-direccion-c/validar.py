"""T08-002: address of an ordinary imported C function used as a C callback.

The conversion happens inside functions that are not the program entry point, so the backend
partitions the module and the coordinator unit must declare the callback targets. Each case is
built through the C header (direct path) and through a precompiled template library consumed
without the header, in O0-O3, and the program is executed. A Clang control computes the same
values.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-funcion-direccion-", dir=r"A:\nsc-interop-build"))
nsc = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
clang = Path(r"A:\al82dev-clang\bin\clang.exe")
linker = Path(r"A:\al82dev-clang\bin\lld-link.exe")
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = str(nsc.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opts = sys.argv[1:] or ["0", "1", "2", "3"]

HEADER = """typedef struct Payload { int value; } Payload;
typedef int (*PlainFn)(Payload*);
__declspec(dllimport) Payload* get_plain(void);
__declspec(dllimport) int take_plain(Payload* p);
__declspec(dllimport) int take_scaled(Payload* p);
__declspec(dllimport) int apply(PlainFn callback, Payload* p);
"""

SOURCE = """typedef struct Payload { int value; } Payload;
typedef int (*PlainFn)(Payload*);
static Payload plain = {2};
__declspec(dllexport) Payload* get_plain(void) { return &plain; }
__declspec(dllexport) int take_plain(Payload* p) { return p->value + 2; }
__declspec(dllexport) int take_scaled(Payload* p) { return p->value * 7; }
__declspec(dllexport) int apply(PlainFn callback, Payload* p) { return callback(p) * 10; }
"""

# (name, NativeSharp body, expected value). Every body runs in its own non-entry function.
CASES = [
    ("CaseInvokeImport", "PlainFn f = take_plain; return f(get_plain());", 4),
    ("CaseImportToC", "return apply(take_plain, get_plain());", 40),
    ("CaseTwoImports", "PlainFn f = take_plain; PlainFn g = take_scaled; return f(get_plain()) + g(get_plain());", 18),
    ("CaseLocalToC", "PlainFn f = take_scaled; return apply(f, get_plain());", 140),
]


def execute(command, folder):
    return subprocess.run([str(x) for x in command], cwd=folder, env=env, text=True,
                          encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)


def require(command, folder, label):
    result = execute(command, folder)
    if result.returncode:
        raise AssertionError(label + " falló:\n" + result.stdout)
    return result.stdout


def build_dll(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"],
            folder, "clang api.c")
    require([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:api.dll", "/implib:api.lib",
             "api.obj"], folder, "lld-link api.dll")


def c_control(folder):
    bodies = "".join(f"static int {name}(void) {{ {body} }}\n" for name, body, _ in CASES)
    checks = " || ".join(f"{name}() != {value}" for name, _, value in CASES)
    (folder / "control.c").write_text(
        '#include "api.h"\n' + bodies + "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
        f"void mainCRTStartup(void) {{ ExitProcess({checks}); }}\n", encoding="utf-8")
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
            folder, "clang control.c")
    require([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
             "/out:control.exe", "control.obj", "api.lib", sdk / "um/x64/kernel32.lib"], folder, "control link")
    require([folder / "control.exe"], folder, "control.exe")
    print("control C: valores esperados confirmados", flush=True)


def direct_source():
    functions = "".join(f"int {name}() {{ {body} }}\n" for name, body, _ in CASES)
    checks = "".join(f"    if ({name}() != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(CASES))
    return functions + "int Main()\n{\n" + checks + "    return 0;\n}\n"


def template_source():
    methods = "".join(f"        public static int {name}<T>(T unused) {{ {body} }}\n" for name, body, _ in CASES)
    return "namespace FunctionAddress\n{\n    public class Bridge\n    {\n" + methods + "    }\n}\n"


def consumer_source():
    checks = "".join(f"    if (FunctionAddress.Bridge.{name}<int>(0) != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(CASES))
    return "int Main()\n{\n" + checks + "    return 0;\n}\n"


def project(folder, name, source, library_kind):
    kind = "<OutputType>Library</OutputType>" if library_kind else ""
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>{kind}</PropertyGroup><ItemGroup>"
        f'<Compile Include="{source}" /><CppLibraryReference Header="api.h" Language="C" Standard="c11" '
        'Library="api.lib" Runtime="api.dll" /></ItemGroup></Project>', encoding="utf-8")


def direct(opt):
    folder = root / f"direct-O{opt}"
    folder.mkdir()
    build_dll(folder)
    (folder / "Main.ns").write_text(direct_source(), encoding="utf-8")
    project(folder, "Direct", "Main.ns", False)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Direct.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "out/Direct.exe"], folder, f"directo O{opt}")
    require([folder / "out" / "Direct.exe"], folder, f"ejecución directa O{opt}")


def precompiled(opt):
    folder = root / f"precompiled-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    build_dll(producer)
    (producer / "Producer.ns").write_text(template_source(), encoding="utf-8")
    project(producer, "ProducerAddress", "Producer.ns", True)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "ProducerAddress.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "ProducerAddress.lib"], producer, f"productor O{opt}")
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text(consumer_source(), encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
        '<LibraryReference Include="producer/ProducerAddress.lib" /></ItemGroup></Project>', encoding="utf-8")
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "out/Consumer.exe"], folder, f"consumidor O{opt}")
    require([folder / "out" / "Consumer.exe"], Path("A:/"), f"ejecución precompilada O{opt}")


control = root / "control"
control.mkdir()
build_dll(control)
c_control(control)
for opt in opts:
    direct(opt)
    precompiled(opt)
    print("dirección de función C importada como callback: directo y precompilado correctos en O" + opt, flush=True)
print("Direcciones de funciones C importadas verificadas;", root)
