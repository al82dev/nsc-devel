"""T08-002: volatile pointees in C callback signatures.

Positive cases must compile and run with the header (direct path) and through a precompiled
template library consumed without the header, in O0-O3. Negative cases must be rejected with the
diagnostic that names the lost qualification; they are frontend checks and run in O0.
A C control built with Clang documents the same contract: equal callback signatures compile and
return the expected values, mismatched callback assignments are errors.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-callback-volatile-", dir=r"A:\nsc-interop-build"))
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
typedef int (*TakeFn)(volatile Payload*);
typedef int (*PlainFn)(Payload*);
typedef volatile Payload* (*GetFn)(void);
typedef int (*VarFn)(volatile Payload*, int, ...);
__declspec(dllimport) volatile Payload* get_payload(void);
__declspec(dllimport) Payload* get_plain(void);
__declspec(dllimport) int take_volatile(volatile Payload* p);
__declspec(dllimport) int take_plain(Payload* p);
__declspec(dllimport) int var_read(volatile Payload* p, int count, ...);
"""

SOURCE = """typedef struct Payload { int value; } Payload;
static volatile Payload payload = {40};
static Payload plain = {2};
__declspec(dllexport) volatile Payload* get_payload(void) { return &payload; }
__declspec(dllexport) Payload* get_plain(void) { return &plain; }
__declspec(dllexport) int take_volatile(volatile Payload* p) { return p->value + 1; }
__declspec(dllexport) int take_plain(Payload* p) { return p->value + 2; }
__declspec(dllexport) int var_read(volatile Payload* p, int count, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, count);
    int result = p->value;
    for (int i = 0; i < count; ++i) result += __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return result;
}
"""

# Positive cases: (name, NativeSharp body, expected value).
POSITIVE = [
    ("CaseInvoke", "TakeFn t = take_volatile; return t(get_payload());", 41),
    ("CaseResult", "GetFn g = get_payload; var p = g(); return p->value;", 40),
    ("CaseNatural", "var f = take_volatile; return f(get_payload());", 41),
    ("CaseVariadic", "VarFn v = var_read; return v(get_payload(), 2, 1, 2);", 43),
    ("CaseAddQualifier", "TakeFn t = take_volatile; return t(get_plain());", 3),
    ("CasePlain", "PlainFn f = take_plain; return f(get_plain());", 4),
]

DISCARD_ARGUMENT = "indirect call argument would discard pointer pointee volatile qualification"
NO_MATCH = "no method in the method group exactly matches delegate signature"
DISCARD_RESULT = "initialization would discard pointer pointee volatile qualification"

# Negative cases: (name, NativeSharp body, expected diagnostic).
NEGATIVE = [
    ("RejectPlainArgument", "PlainFn f = take_plain; return f(get_payload());", DISCARD_ARGUMENT),
    ("RejectDropParameter", "PlainFn f = take_volatile; return 0;", NO_MATCH),
    ("RejectAddParameter", "TakeFn t = take_plain; return 0;", NO_MATCH),
    ("RejectDropResult", "GetFn g = get_payload; Payload* p = g(); return p->value;", DISCARD_RESULT),
    ("RejectCallbackToPlain", "TakeFn t = take_volatile; PlainFn f = t; return 0;", "cannot initialize local 'f'"),
    ("RejectVariadicSlot", "VarFn v = var_read; return v(get_payload(), 1, get_payload());", DISCARD_ARGUMENT),
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
    checks = " || ".join(f"case_{name}() != {value}" for name, _, value in POSITIVE)
    bodies = ""
    for name, body, _ in POSITIVE:
        c_body = body.replace("var p = g();", "volatile Payload* p = g();")
        c_body = c_body.replace("var f = take_volatile;", "TakeFn f = take_volatile;")
        bodies += f"static int case_{name}(void) {{ {c_body} }}\n"
    (folder / "control.c").write_text(
        '#include "api.h"\n' + bodies +
        "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
        f"void mainCRTStartup(void) {{ ExitProcess({checks}); }}\n", encoding="utf-8")
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
            folder, "clang control.c")
    require([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
             "/out:control.exe", "control.obj", "api.lib", sdk / "um/x64/kernel32.lib"], folder, "control link")
    require([folder / "control.exe"], folder, "control.exe")
    for name, body in (("RejectDropParameter", "PlainFn f = take_volatile;"),
                       ("AddParameter", "TakeFn t = take_plain;"),
                       ("CallbackToPlain", "TakeFn t = take_volatile; PlainFn f = t;")):
        (folder / f"reject_{name}.c").write_text(
            '#include "api.h"\n' + f"int probe(void) {{ {body} return 0; }}\n", encoding="utf-8")
        result = execute([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only",
                          f"reject_{name}.c"], folder)
        if result.returncode == 0 or "incompatible" not in result.stdout:
            raise AssertionError("Clang aceptó el control " + name + ":\n" + result.stdout)
    (folder / "warn_result.c").write_text(
        '#include "api.h"\nint probe(void) { GetFn g = get_payload; Payload* p = g(); return p->value; }\n',
        encoding="utf-8")
    result = execute([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only", "warn_result.c"],
                     folder)
    if "discards qualifiers" not in result.stdout:
        raise AssertionError("Clang no informó la pérdida del calificador del resultado:\n" + result.stdout)
    print("control C: valores correctos; conversiones incompatibles rechazadas", flush=True)


def write_project(folder, name, sources, header):
    items = "".join(f'<Compile Include="{source}" />' for source in sources)
    if header:
        items += ('<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="api.lib" '
                  'Runtime="api.dll" />')
    kind = "<OutputType>Library</OutputType>" if name.startswith("Producer") else ""
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>{kind}</PropertyGroup><ItemGroup>{items}"
        "</ItemGroup></Project>", encoding="utf-8")


def template_source(cases):
    methods = "".join(f"        public static int {name}<T>(T unused) {{ {body} }}\n"
                      for name, body, _ in cases)
    return "namespace VolatileCallbacks\n{\n    public class Bridge\n    {\n" + methods + "    }\n}\n"


def direct_source(cases):
    functions = "".join(f"int {name}() {{ {body} }}\n" for name, body, _ in cases)
    checks = "".join(f"    if ({name}() != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(cases))
    return functions + "int Main()\n{\n" + checks + "    return 0;\n}\n"


def consumer_source(cases):
    checks = "".join(f"    if (VolatileCallbacks.Bridge.{name}<int>(0) != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(cases))
    return "int Main()\n{\n" + checks + "    return 0;\n}\n"


def positive_direct(opt):
    folder = root / f"direct-O{opt}"
    folder.mkdir()
    build_dll(folder)
    (folder / "Main.ns").write_text(direct_source(POSITIVE), encoding="utf-8")
    write_project(folder, "Direct", ["Main.ns"], True)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Direct.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "out/Direct.exe"], folder, f"directo O{opt}")
    require([folder / "out" / "Direct.exe"], folder, f"ejecución directa O{opt}")


def positive_precompiled(opt):
    folder = root / f"precompiled-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    build_dll(producer)
    (producer / "Producer.ns").write_text(template_source(POSITIVE), encoding="utf-8")
    write_project(producer, "ProducerVolatile", ["Producer.ns"], True)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "ProducerVolatile.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "ProducerVolatile.lib"], producer, f"productor O{opt}")
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text(consumer_source(POSITIVE), encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
        '<LibraryReference Include="producer/ProducerVolatile.lib" /></ItemGroup></Project>', encoding="utf-8")
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "out/Consumer.exe"], folder, f"consumidor O{opt}")
    require([folder / "out" / "Consumer.exe"], Path("A:/"), f"ejecución precompilada O{opt}")


def expect_rejection(result, message, label, artifact):
    if result.returncode == 0 or message not in result.stdout or artifact.exists():
        raise AssertionError(label + " no produjo el rechazo esperado '" + message + "':\n" + result.stdout)


def negative_cases():
    for name, body, message in NEGATIVE:
        case = [(name, body, 0)]
        direct = root / f"reject-direct-{name}"
        direct.mkdir()
        build_dll(direct)
        (direct / "Main.ns").write_text(direct_source(case), encoding="utf-8")
        write_project(direct, "Direct", ["Main.ns"], True)
        result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Direct.nsproj",
                          "-O0", "--no-artifact-cache", "-o", "out/Direct.exe"], direct)
        expect_rejection(result, message, "directo " + name, direct / "out" / "Direct.exe")
        folder = root / f"reject-precompiled-{name}"
        producer = folder / "producer"
        producer.mkdir(parents=True)
        build_dll(producer)
        (producer / "Producer.ns").write_text(template_source(case), encoding="utf-8")
        write_project(producer, "ProducerVolatile", ["Producer.ns"], True)
        result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib",
                          "ProducerVolatile.nsproj", "-O0", "--no-artifact-cache", "-o", "ProducerVolatile.lib"],
                         producer)
        stage = "productor"
        if result.returncode == 0:
            (producer / "api.h").unlink()
            (folder / "Consumer.ns").write_text(consumer_source(case), encoding="utf-8")
            (folder / "Consumer.nsproj").write_text(
                '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>'
                '<Compile Include="Consumer.ns" /><LibraryReference Include="producer/ProducerVolatile.lib" />'
                '</ItemGroup></Project>', encoding="utf-8")
            result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe",
                              "Consumer.nsproj", "-O0", "--no-artifact-cache", "-o", "out/Consumer.exe"], folder)
            stage = "consumidor sin encabezado"
        expect_rejection(result, message, "precompilado " + name, folder / "out" / "Consumer.exe")
        print(name, "rechazado (directo y precompilado; etapa:", stage + ")", flush=True)


def volatile_loads(ir, function):
    match = re.search(r"define [^\n]*" + re.escape(function) + r"[^\n]*\{(.*?)\n\}", ir, re.S)
    if not match:
        raise AssertionError("no se encontró la función " + function + " en el IR")
    return len(re.findall(r"load (?:atomic )?volatile", match.group(1)))


def ir_result_contract():
    folder = root / "ir"
    folder.mkdir()
    build_dll(folder)
    (folder / "Main.ns").write_text(
        "int ReadDirect() { var p = get_payload(); return p->value; }\n"
        "int ReadThroughCallback() { GetFn g = get_payload; var p = g(); return p->value; }\n"
        "int Main() { return ReadDirect() + ReadThroughCallback() == 80 ? 0 : 1; }\n", encoding="utf-8")
    write_project(folder, "Ir", ["Main.ns"], True)
    result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], folder)
    if result.returncode:
        raise AssertionError("dump IR falló:\n" + result.stdout)
    (folder / "native.ll").write_text(result.stdout, encoding="utf-8")
    direct = volatile_loads(result.stdout, "ReadDirect")
    indirect = volatile_loads(result.stdout, "ReadThroughCallback")
    if direct == 0 or direct != indirect:
        raise AssertionError(f"lecturas volatile distintas: directa {direct}, indirecta {indirect}")
    print("IR: lectura volatile del resultado indirecto igual a la directa", flush=True)


control = root / "control"
control.mkdir()
build_dll(control)
c_control(control)
ir_result_contract()
negative_cases()
for opt in opts:
    positive_direct(opt)
    positive_precompiled(opt)
    print("casos positivos correctos (directo y precompilado sin encabezado) en O" + opt, flush=True)
print("Callbacks C con datos apuntados volatile verificados;", root)
