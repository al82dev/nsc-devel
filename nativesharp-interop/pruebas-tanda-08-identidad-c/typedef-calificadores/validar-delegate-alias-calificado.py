"""T08-002: named NativeSharp delegates declared with C pointer aliases whose pointee is `const` or
`volatile` (`typedef volatile Payload* VPayload;`, `delegate int VHandler(VPayload p);`).

Before, the delegate signature dropped the qualification: invoking a `VHandler` with a volatile
pointer was rejected, any method converted regardless of qualifiers, a `const` argument reached a
plain `Payload*` parameter, a lambda could write through a `CPayload`, and the volatile result of a
`VGetter` was read with ordinary loads. Positive cases compile and run with the header (direct path)
and through a precompiled template library consumed without the header, in O0-O3; negative cases are
rejected; the IR checks the volatile reads. A C control documents the equivalent function-pointer
contract. Since T08-D02 NSMETA keeps the pointee qualifiers of the public delegates, so the precompiled
path runs every positive and negative case (before, only the cases that did not depend on them).
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-delegate-alias-calificado-", dir=r"A:\nsc-interop-build"))
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
typedef volatile Payload* VPayload;
typedef const Payload* CPayload;
__declspec(dllimport) volatile Payload* get_v(void);
__declspec(dllimport) const Payload* get_c(void);
__declspec(dllimport) Payload* get_p(void);
__declspec(dllimport) int take_v(volatile Payload* p);
__declspec(dllimport) int take_c(const Payload* p);
__declspec(dllimport) int take_p(Payload* p);
"""

SOURCE = """typedef struct Payload { int value; } Payload;
static Payload cell = {40};
__declspec(dllexport) volatile Payload* get_v(void) { return &cell; }
__declspec(dllexport) const Payload* get_c(void) { return &cell; }
__declspec(dllexport) Payload* get_p(void) { return &cell; }
__declspec(dllexport) int take_v(volatile Payload* p) { return p->value + 2; }
__declspec(dllexport) int take_c(const Payload* p) { return p->value + 1; }
__declspec(dllexport) int take_p(Payload* p) { return p->value + 3; }
"""

DELEGATES = [
    "delegate int VHandler(VPayload p);",
    "delegate int CHandler(CPayload p);",
    "delegate int PHandler(Payload* p);",
    "delegate VPayload VGetter();",
]

POSITIVE = [
    ("CaseVolatileMethod", "VHandler h = take_v; return h(get_v());", 42),
    ("CaseConstMethod", "CHandler h = take_c; return h(get_c());", 41),
    ("CaseVolatileLambda", "VHandler h = (p) => p->value; return h(get_v());", 40),
    ("CaseConstLambda", "CHandler h = (p) => p->value + 1; return h(get_c());", 41),
    ("CaseVolatileGetter", "VGetter g = get_v; var p = g(); return p->value;", 40),
    ("CasePlain", "PHandler h = take_p; return h(get_p());", 43),
]

NO_MATCH = "no method in the method group exactly matches delegate signature"
DISCARD_VOLATILE = "indirect call argument would discard pointer pointee volatile qualification"
DISCARD_CONST = "indirect call argument would discard pointer pointee const qualification"

NEGATIVE = [
    ("RejectConstLambdaWrite", "CHandler h = (p) => { p->value = 1; return 0; }; return h(get_c());",
     "left side of assignment is not assignable"),
    ("RejectVolatileFromPlain", "VHandler h = take_p; return 0;", NO_MATCH),
    ("RejectConstFromPlain", "CHandler h = take_p; return 0;", NO_MATCH),
    ("RejectPlainFromVolatile", "PHandler h = take_v; return 0;", NO_MATCH),
    ("RejectPlainVolatileArgument", "PHandler h = take_p; return h(get_v());", DISCARD_VOLATILE),
    ("RejectPlainConstArgument", "PHandler h = take_p; return h(get_c());", DISCARD_CONST),
    ("RejectGetterDrop", "VGetter g = get_v; Payload* p = g(); return 0;",
     "initialization would discard pointer pointee volatile qualification"),
]

# Precompiled path. The consumer materializes the public delegates from metadata; since T08-D02 NSMETA keeps
# the pointee qualifiers of their parameters and result, so every case runs there too.
POSITIVE_PRECOMPILED = POSITIVE
NEGATIVE_PRECOMPILED = tuple(name for name, _, _ in NEGATIVE)

# C control: the same contract written with function pointers.
C_TYPES = """typedef int (*VHandler)(VPayload);
typedef int (*CHandler)(CPayload);
typedef int (*PHandler)(Payload*);
typedef VPayload (*VGetter)(void);
"""
C_POSITIVE = [
    ("VolatileMethod", "VHandler h = take_v; return h(get_v());", 42),
    ("ConstMethod", "CHandler h = take_c; return h(get_c());", 41),
    ("VolatileGetter", "VGetter g = get_v; VPayload p = g(); return p->value;", 40),
    ("Plain", "PHandler h = take_p; return h(get_p());", 43),
]
C_NEGATIVE = [
    ("VolatileFromPlain", "VHandler h = take_p; return 0;", "incompatible"),
    ("ConstFromPlain", "CHandler h = take_p; return 0;", "incompatible"),
    ("PlainFromVolatile", "PHandler h = take_v; return 0;", "incompatible"),
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
    checks = " || ".join(f"case_{name}() != {value}" for name, _, value in C_POSITIVE)
    bodies = "".join(f"static int case_{name}(void) {{ {body} }}\n" for name, body, _ in C_POSITIVE)
    (folder / "control.c").write_text(
        '#include "api.h"\n' + C_TYPES + bodies +
        "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
        f"void mainCRTStartup(void) {{ ExitProcess({checks}); }}\n", encoding="utf-8")
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
            folder, "clang control.c")
    require([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
             "/out:control.exe", "control.obj", "api.lib", sdk / "um/x64/kernel32.lib"], folder, "control link")
    require([folder / "control.exe"], folder, "control.exe")
    for name, body, needle in C_NEGATIVE:
        (folder / f"reject_{name}.c").write_text(
            '#include "api.h"\n' + C_TYPES + f"int probe(void) {{ {body} }}\n", encoding="utf-8")
        result = execute([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only",
                          f"reject_{name}.c"], folder)
        if result.returncode == 0 or needle not in result.stdout:
            raise AssertionError("Clang aceptó el control " + name + ":\n" + result.stdout)
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
    methods = "".join(f"        public static int {name}<T>(T unused) {{ {body} }}\n" for name, body, _ in cases)
    delegates = "".join(f"    public {declaration}\n" for declaration in DELEGATES)
    return "namespace QualifiedDelegates\n{\n" + delegates + "    public class Bridge\n    {\n" + methods + "    }\n}\n"


def direct_source(cases):
    functions = "".join(declaration + "\n" for declaration in DELEGATES)
    functions += "".join(f"int {name}() {{ {body} }}\n" for name, body, _ in cases)
    checks = "".join(f"    if ({name}() != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(cases))
    return functions + "int Main()\n{\n" + checks + "    return 0;\n}\n"


def consumer_source(cases):
    checks = "".join(f"    if (QualifiedDelegates.Bridge.{name}<int>(0) != {value}) return {index + 1};\n"
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
    (producer / "Producer.ns").write_text(template_source(POSITIVE_PRECOMPILED), encoding="utf-8")
    write_project(producer, "ProducerDelegate", ["Producer.ns"], True)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "ProducerDelegate.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "ProducerDelegate.lib"], producer, f"productor O{opt}")
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text(consumer_source(POSITIVE_PRECOMPILED), encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
        '<LibraryReference Include="producer/ProducerDelegate.lib" /></ItemGroup></Project>', encoding="utf-8")
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
        if name not in NEGATIVE_PRECOMPILED:
            print(name, "rechazado (directo)", flush=True)
            continue
        folder = root / f"reject-precompiled-{name}"
        producer = folder / "producer"
        producer.mkdir(parents=True)
        build_dll(producer)
        (producer / "Producer.ns").write_text(template_source(case), encoding="utf-8")
        write_project(producer, "ProducerDelegate", ["Producer.ns"], True)
        result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib",
                          "ProducerDelegate.nsproj", "-O0", "--no-artifact-cache", "-o", "ProducerDelegate.lib"],
                         producer)
        stage = "productor"
        if result.returncode == 0:
            (producer / "api.h").unlink()
            (folder / "Consumer.ns").write_text(consumer_source(case), encoding="utf-8")
            (folder / "Consumer.nsproj").write_text(
                '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>'
                '<Compile Include="Consumer.ns" /><LibraryReference Include="producer/ProducerDelegate.lib" />'
                '</ItemGroup></Project>', encoding="utf-8")
            result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe",
                              "Consumer.nsproj", "-O0", "--no-artifact-cache", "-o", "out/Consumer.exe"], folder)
            stage = "consumidor sin encabezado"
        expect_rejection(result, message, "precompilado " + name, folder / "out" / "Consumer.exe")
        print(name, "rechazado (directo y precompilado; etapa:", stage + ")", flush=True)


def volatile_reads(ir, pattern):
    bodies = re.findall(r"define [^\n]*" + pattern + r"[^\n]*\{(.*?)\n\}", ir, re.S)
    return sum(len(re.findall(r"load (?:atomic )?volatile", body)) for body in bodies)


def ir_contract():
    folder = root / "ir"
    folder.mkdir()
    build_dll(folder)
    (folder / "Main.ns").write_text(
        "".join(declaration + "\n" for declaration in DELEGATES) +
        "int ViaLambda() { VHandler h = (p) => p->value; return h(get_v()); }\n"
        "int ViaGetter() { VGetter g = get_v; var p = g(); return p->value; }\n"
        "int Main() { return ViaLambda() + ViaGetter() == 80 ? 0 : 1; }\n", encoding="utf-8")
    write_project(folder, "Ir", ["Main.ns"], True)
    result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], folder)
    if result.returncode:
        raise AssertionError("dump IR falló:\n" + result.stdout)
    (folder / "native.ll").write_text(result.stdout, encoding="utf-8")
    if volatile_reads(result.stdout, "lambda") == 0 or volatile_reads(result.stdout, "ViaGetter") == 0:
        raise AssertionError("falta una lectura volatile en la lambda o en el resultado del getter")
    print("IR: lecturas volatile en la lambda y en el resultado del getter", flush=True)


control = root / "control"
control.mkdir()
build_dll(control)
c_control(control)
ir_contract()
negative_cases()
for opt in opts:
    positive_direct(opt)
    positive_precompiled(opt)
    print("casos positivos correctos (directo y precompilado sin encabezado) en O" + opt, flush=True)
print("Delegates con nombre declarados con alias C calificados verificados;", root)
