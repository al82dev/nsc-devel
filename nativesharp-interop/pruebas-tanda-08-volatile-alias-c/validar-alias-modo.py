"""T08-D13 (T08-010-05a): a NativeSharp alias of C volatile storage follows the volatile mode of its C origin.

The mode is the one of the record, global or header reference the storage comes from (VolatileMode of the
CppLibraryReference), as for the direct access; it is never a mode of the consuming unit. Every case is a C function
and a NativeSharp method with the same body (`var` in NativeSharp, `volatile int *` in C). Their IR is compared with
Clang per function, at O1 on every path and at O0-O3 on the direct path:
- with VolatileMode Default and Microsoft, as Clang -fms-volatile: every volatile load is `load atomic volatile ...
  acquire` and every store `store atomic volatile ... release`;
- with VolatileMode ISO, as Clang -fno-ms-volatile: plain volatile accesses.
Cases: alias of a volatile flexible member and an alias of that alias, indexing, `*(p + i)`, reassignment, assignment
in a branch, a conditional between two aliases, compound assignment, alias of a C field `volatile int *` and of an
element of a field array of such pointers (and the direct access to that element), C globals `volatile int *`, an
array of them and `extern volatile int g_flex[]`, and the direct access to the flexible member as a baseline.
Also checked:
- two references to the same header with Microsoft and ISO in one project: the record and globals have no single
  mode, and the alias is plain volatile like the direct access (T08-010-07, T08-015-03d);
- the same cases from a precompiled producer consumed without the header (T08-015-05c);
- a graph with two live projects that import the same header in different modes, with typedefs of the records and
  with tags only: the code of each project keeps its own reference's mode, also for the C globals both declare, and
  the executable runs at O0-O3 (T08-015-03c);
- the artifact cache misses after the reference changes from Microsoft to ISO (T08-015-06a);
- the cases run at O0-O3 and return the value of a Clang control;
- rejections: a packed flexible member has no alias (T08-015-04e), a NativeSharp `int*` parameter does not receive
  the alias (T08-015-05a) and the project property CppConsumerVolatileMode is not supported (T08-015-08).
T08-D13: el alias NativeSharp de almacenamiento C volatile sigue el modo volatile de su origen C.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-volatile-alias-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER = """typedef struct VFlex { int n; volatile int items[]; } VFlex;
typedef struct Ptrs { volatile int *vp; volatile int *pvs[2]; } Ptrs;
#pragma pack(push, 1)
typedef struct PFlex { char c; volatile int items[]; } PFlex;
#pragma pack(pop)
extern volatile int *g_vp;
extern volatile int *g_pvs[2];
extern volatile int g_flex[];
VFlex *flex_a(void);
VFlex *flex_b(void);
PFlex *flex_packed(void);
Ptrs *ptrs(void);
int sample(int which);
"""
TAG_HEADER = (HEADER.replace("typedef struct VFlex {", "struct VFlex {").replace("} VFlex;", "};")
              .replace("typedef struct Ptrs {", "struct Ptrs {").replace("} Ptrs;", "};")
              .replace("typedef struct PFlex {", "struct PFlex {").replace("} PFlex;", "};")
              .replace("VFlex *", "struct VFlex *").replace("PFlex *", "struct PFlex *")
              .replace("Ptrs *", "struct Ptrs *"))
SOURCE = """#include "api.h"
typedef struct VFlex VFlex;
typedef struct PFlex PFlex;
typedef struct Ptrs Ptrs;
static struct { int n; int items[4]; } flex_storage_a = {4, {10, 11, 12, 13}};
static struct { int n; int items[4]; } flex_storage_b = {4, {20, 21, 22, 23}};
static unsigned char packed_storage[1 + 4 * 4];
static int pointees[4] = {30, 31, 32, 33};
static Ptrs pointers = {&pointees[0], {&pointees[1], &pointees[2]}};
volatile int *g_vp = &pointees[3];
volatile int *g_pvs[2] = {&pointees[0], &pointees[1]};
volatile int g_flex[3] = {40, 41, 42};
VFlex *flex_a(void) { return (VFlex *)&flex_storage_a; }
VFlex *flex_b(void) { return (VFlex *)&flex_storage_b; }
PFlex *flex_packed(void) { return (PFlex *)packed_storage; }
Ptrs *ptrs(void) { return &pointers; }
int sample(int which) { return which < 4 ? pointees[which] : flex_storage_a.items[which - 4]; }
"""
# Name, parameters and body; the same text is NativeSharp and C (`var` becomes `volatile int*` in C).
# NativeSharp has no spelling for `int volatile*`: an alias is always `var` (T08-010-05).
# Nombre, parametros y cuerpo; el mismo texto es NativeSharp y C.
CASES = [
    ("Kflex", "VFlex* v", "var p = v->items; p[0] = 1; *p = 2; return p[1];"),
    ("Kchain", "VFlex* v", "var p = v->items; var q = p + 1; *q = 3; return q[0];"),
    ("Karith", "VFlex* v, int i", "var p = v->items; *(p + i) = 5; return *(p + i);"),
    ("Kreassigned", "VFlex* a, VFlex* b",
     "var p = a->items; p[0] = 6; p = b->items; p[0] = 7; return *p;"),
    ("Kconditional", "VFlex* a, VFlex* b, int c",
     "var pa = a->items; var pb = b->items; var p = c != 0 ? pa : pb; *p = 8; return *p;"),
    ("Kbranches", "VFlex* a, VFlex* b, int c",
     "var p = a->items; if (c != 0) { p = b->items; } p[1] = 4; return *p;"),
    ("Kcompound", "VFlex* v", "var p = v->items; *p += 3; p[1] -= 1; return *p;"),
    ("Kfield", "Ptrs* s", "var q = s->vp; *q = 4; q[0] = 5; return *q;"),
    ("Kelement", "Ptrs* s", "var e = s->pvs[1]; *e = 6; *s->pvs[0] = 7; return *e;"),
    ("Kglobal", "", "var g = g_vp; *g = 8; return *g;"),
    ("Kgarray", "", "var g = g_pvs[0]; *g = 9; return *g_pvs[1];"),
    ("Kgflex", "", "var f = g_flex; f[0] = 1; return f[2];"),
    ("Kdirect", "VFlex* v", "v->items[0] = 1; return v->items[1];"),
]
DRIVE = """    VFlex* a = flex_a();
    VFlex* b = flex_b();
    Ptrs* s = ptrs();
    int total = 0;
    total += Kflex(a);
    total += Kchain(a);
    total += Karith(a, 2);
    total += Kreassigned(a, b);
    total += Kconditional(a, b, 1);
    total += Kconditional(a, b, 0);
    total += Kbranches(a, b, 1);
    total += Kbranches(a, b, 0);
    total += Kcompound(a);
    total += Kfield(s);
    total += Kelement(s);
    total += Kglobal();
    total += Kgarray();
    total += Kgflex();
    total += Kdirect(b);
    total += sample(0) + sample(1) + sample(2) + sample(3) + sample(4) + sample(5);
    return total;
"""


def c_text(text):
    return text.replace("var ", "volatile int* ")


def c_parameters(parameters):
    return parameters or "void"


def ns_methods(generic):
    """EN: The cases as static methods; generic ones are templates lowered by the consumer.
    ES: Los casos como metodos estaticos; los genericos son plantillas que baja el consumidor."""
    text = ""
    for name, parameters, body in CASES:
        head = f"{name}<T>" if generic else name
        text += f"    public static int {head}({parameters}) {{ {body} }}\n"
    return text


def drive(generic):
    text = DRIVE
    if generic:
        for name, _, _ in CASES:
            text = text.replace(f" {name}(", f" {name}<T>(")
        return "    public static int Drive<T>(T unused)\n    {\n" + text + "    }\n"
    return "    public static int Drive()\n    {\n" + text + "    }\n"


def program(namespace, generic=False):
    return (f"namespace {namespace} {{ public class V {{\n" + ns_methods(generic) + drive(generic) +
            "} }\n")


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder, header=HEADER):
    (folder / "api.h").write_text(header, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:valias.lib", "api.obj"], folder)


def reference(mode, header="api.h", library=True):
    return (f'<CppLibraryReference Header="{header}" Language="C" Standard="c11"' +
            (' Library="valias.lib"' if library else "") + f' VolatileMode="{mode}" />')


def project(folder, name, source, refs, library=False, properties=""):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        properties +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def blocks(text):
    """EN: Volatile load/store lines of each defined function. ES: lineas volatile de cada funcion definida."""
    found = {}
    current = None
    for line in text.splitlines():
        if line.startswith("define "):
            current = line
            found[current] = []
        elif line.startswith("}"):
            current = None
        elif current is not None and " volatile " in line and ("load " in line or "store " in line):
            found[current].append(line.strip())
    return found


def is_atomic(line):
    return " atomic volatile " in line and (" acquire" in line or " release" in line)


def function_accesses(text, name, owner=""):
    found = blocks(text)
    matches = [k for k in found if owner in k and re.search(r"[^A-Za-z0-9]" + name + r"[^A-Za-z0-9]", k)]
    if len(matches) != 1:
        raise AssertionError(f"{name}: {len(matches)} definiciones en el IR")
    lines = found[matches[0]]
    return len(lines), len([l for l in lines if is_atomic(l)])


def clang_ir(folder, flag, opt="1"):
    body = "".join(f"int {n}({c_parameters(p)}) {{ {c_text(b)} }}\n" for n, p, b in CASES)
    (folder / "cases.c").write_text('#include "api.h"\n' + body, encoding="utf-8")
    out = folder / f"clang{flag}-O{opt}.ll"
    run([clang, "-S", "-emit-llvm", "--target=x86_64-pc-windows-msvc", "-std=c11", "-O" + opt, flag, "cases.c", "-o",
         out], folder)
    return out.read_text(encoding="utf-8")


def compare(label, ns_text, clang_text, atomic_expected, owner=""):
    """EN: Per case, the same number of volatile accesses as Clang, all atomic or none.
    ES: Por caso, el mismo numero de accesos volatile que Clang, todos atomicos o ninguno."""
    bad = []
    for name, _, _ in CASES:
        ns_total, ns_atomic = function_accesses(ns_text, name, owner)
        c_total, c_atomic = function_accesses(clang_text, name)
        expected_atomic = ns_total if atomic_expected else 0
        clang_ok = c_atomic == (c_total if atomic_expected else 0)
        if observe:
            print(label, name, "NSC", ns_total, ns_atomic, "Clang", c_total, c_atomic, flush=True)
        if ns_total != c_total or ns_atomic != expected_atomic or not clang_ok:
            bad.append((name, ns_total, ns_atomic, c_total, c_atomic))
    if observe:
        return
    if bad:
        raise AssertionError(f"{label}: casos distintos de Clang (caso, NSC total, NSC atomicos, Clang total, "
                             f"Clang atomicos) {bad}")
    print(label, "correcto", flush=True)


def compare_total(label, ns_text, atomic_expected):
    """EN: A consumer inlines the templates: the whole module is all atomic or none.
    ES: Un consumidor puede expandir las plantillas: el modulo entero es todo atomico o nada."""
    lines = [l for v in blocks(ns_text).values() for l in v]
    atomics = len([l for l in lines if is_atomic(l)])
    if observe:
        print(label, len(lines), atomics, flush=True)
        return
    if len(lines) < len(CASES) or atomics != (len(lines) if atomic_expected else 0):
        raise AssertionError(f"{label}: {len(lines)} accesos volatile, {atomics} atomicos:\n{ns_text}")
    print(label, "correcto", flush=True)


def dump(folder, name, opt="1"):
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", f"{name}.nsproj",
                  "-O" + opt], folder, check=False)
    if result.returncode:
        raise AssertionError(f"IR {folder}:\n{result.stdout}")
    return result.stdout


def build(folder, name, opt, kind, output, cache=None):
    args = [nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj", "-O" + opt]
    args += ["--time", "--artifact-cache=" + str(cache)] if cache else ["--no-artifact-cache"]
    return run(args + ["-o", output], folder, check=False)


MAIN = "int Main() { return Va.V.Drive() != 0 ? 0 : 1; }\n"


def ir_checks():
    clang_folder = root / "clang"
    clang_folder.mkdir()
    (clang_folder / "api.h").write_text(HEADER, encoding="utf-8")
    ms = clang_ir(clang_folder, "-fms-volatile")
    iso = clang_ir(clang_folder, "-fno-ms-volatile")
    for mode in ("Default", "Microsoft", "ISO"):
        direct = root / f"ir-{mode}"
        direct.mkdir()
        c_library(direct)
        (direct / "M.ns").write_text(program("Va") + MAIN, encoding="utf-8")
        project(direct, "M", "M.ns", reference(mode))
        compare(f"IR directo {mode}", dump(direct, "M"), ms if mode != "ISO" else iso, mode != "ISO")
        for opt in (o for o in opts if o != "1"):
            flag = "-fno-ms-volatile" if mode == "ISO" else "-fms-volatile"
            compare(f"IR directo {mode} O{opt}", dump(direct, "M", opt), clang_ir(clang_folder, flag, opt),
                    mode != "ISO")
        producer = root / f"productor-{mode}"
        producer.mkdir()
        c_library(producer)
        (producer / "P.ns").write_text(program("Vp", True), encoding="utf-8")
        project(producer, "VaPub", "P.ns", reference(mode), library=True)
        result = build(producer, "VaPub", "1", "lib", "VaPub.lib")
        if result.returncode:
            raise AssertionError(f"productor {mode}:\n{result.stdout}")
        (producer / "api.h").unlink()
        consumer = root / f"consumidor-{mode}"
        consumer.mkdir()
        (consumer / "C.ns").write_text("int Main() { return Vp.V.Drive<int>(0) != 0 ? 0 : 1; }\n", encoding="utf-8")
        project(consumer, "C", "C.ns", f'<LibraryReference Include="../productor-{mode}/VaPub.lib" />')
        compare_total(f"IR precompilado sin encabezado {mode}", dump(consumer, "C"), mode != "ISO")
    # EN: two references to the same header, Microsoft and ISO: no single mode, plain volatile everywhere.
    # ES: dos referencias al mismo encabezado, Microsoft e ISO: sin modo unico, volatile simple en todo.
    mixed = root / "ir-mixto"
    (mixed / "second").mkdir(parents=True)
    c_library(mixed)
    shutil.copy(mixed / "api.h", mixed / "second" / "api.h")
    (mixed / "M.ns").write_text(program("Va") + MAIN, encoding="utf-8")
    project(mixed, "M", "M.ns", reference("Microsoft") + reference("ISO", "second/api.h", False))
    compare("IR referencias mixtas", dump(mixed, "M"), iso, False)
    # EN: a graph: the executable (ISO) uses a library project (Microsoft) and both import the same header; the
    #     code of each project keeps its own reference's mode, also for the C globals both declare. The header is
    #     tried with typedefs of the records and with their tags only.
    # ES: un grafo: el ejecutable (ISO) usa un proyecto biblioteca (Microsoft) y los dos importan el mismo
    #     encabezado; el codigo de cada proyecto conserva el modo de su referencia, tambien en las globales C que
    #     declaran los dos. El encabezado se prueba con typedef de los records y solo con sus etiquetas.
    for label, header in (("typedef", HEADER), ("etiqueta", TAG_HEADER)):
        graph = root / f"grafo-{label}"
        (graph / "side").mkdir(parents=True)
        c_library(graph / "side", header)
        (graph / "side" / "S.ns").write_text(program("Vs"), encoding="utf-8")
        project(graph / "side", "Side", "S.ns", reference("Microsoft"), library=True)
        compare(f"IR grafo {label}, proyecto Microsoft", dump(graph / "side", "Side"), ms, True)
        c_library(graph, header)
        (graph / "M.ns").write_text(program("Vm") + "int Main() { return Vm.V.Drive() + Vs.V.Drive() != 0 ? 0 : 1; }\n",
                                    encoding="utf-8")
        # EN: the executable imports the header without the library: the library project already links it.
        # ES: el ejecutable importa el encabezado sin la biblioteca: ya la enlaza el proyecto biblioteca.
        project(graph, "M", "M.ns", reference("ISO", library=False) + '<ProjectReference Include="side/Side.nsproj" />')
        whole = dump(graph, "M")
        compare(f"IR grafo {label}, ejecutable ISO", whole, iso, False, "Vm")
        compare(f"IR grafo {label}, biblioteca Microsoft en el ejecutable", whole, ms, True, "Vs")
        for opt in opts:
            result = build(graph, "M", opt, "exe", f"out{opt}/M.exe")
            code = run([graph / f"out{opt}" / "M.exe"], graph, check=False).returncode if not result.returncode \
                else None
            if observe:
                print(f"ejecucion grafo {label} O{opt}", result.returncode, code, flush=True)
            elif result.returncode or code != 0:
                raise AssertionError(f"ejecucion grafo {label} O{opt}: build {result.returncode}, salida {code}:\n"
                                     f"{result.stdout}")
            else:
                print(f"ejecucion grafo {label} O{opt} correcto", flush=True)


def cache_check():
    folder = root / "cache"
    folder.mkdir()
    c_library(folder)
    (folder / "M.ns").write_text(program("Va") + MAIN, encoding="utf-8")
    hits = []
    for step, mode in enumerate(("Microsoft", "Microsoft", "ISO", "ISO")):
        project(folder, "M", "M.ns", reference(mode))
        result = build(folder, "M", "1", "exe", f"out{step}/M.exe", folder / "cache")
        if result.returncode:
            raise AssertionError(f"cache paso {step}:\n{result.stdout}")
        hits.append("compilation-bundle-cache: hit" in result.stdout)
    expected = [False, True, False, True]
    if observe:
        print("cache", hits, flush=True)
    elif hits != expected:
        raise AssertionError(f"cache: aciertos {hits}, se esperaba {expected}")
    else:
        print("cache Microsoft/ISO correcto", flush=True)


def control():
    folder = root / "control"
    folder.mkdir()
    c_library(folder)
    body = "".join(f"static int {n}({c_parameters(p)}) {{ {c_text(b)} }}\n" for n, p, b in CASES)
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n' + body +
        "static int drive(void)\n{\n" + DRIVE + "}\n"
        "void mainCRTStartup(void) { ExitProcess((unsigned long)drive()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "valias.lib", sdk / "um/x64/kernel32.lib"], folder)
    return run([folder / "control.exe"], folder, check=False).returncode


def execution(expected):
    for opt in opts:
        for mode in ("Microsoft", "ISO"):
            folder = root / f"ejecucion-{mode}-O{opt}"
            folder.mkdir()
            c_library(folder)
            (folder / "M.ns").write_text(program("Va") + "int Main() { return Va.V.Drive(); }\n", encoding="utf-8")
            project(folder, "M", "M.ns", reference(mode))
            result = build(folder, "M", opt, "exe", "out/M.exe")
            code = run([folder / "out" / "M.exe"], folder, check=False).returncode if not result.returncode else None
            if observe:
                print("ejecucion", mode, opt, result.returncode, code, expected, flush=True)
            elif result.returncode or code != expected:
                raise AssertionError(f"ejecucion {mode} O{opt}: build {result.returncode}, salida {code}, "
                                     f"control {expected}:\n{result.stdout}")
            else:
                print(f"ejecucion {mode} O{opt} correcto", flush=True)


REJECTIONS = [
    # EN: a packed flexible member proves no alignment: it has no alias (T08-015-04e).
    # ES: un miembro flexible empaquetado no demuestra alineacion: no tiene alias (T08-015-04e).
    ("empaquetado", "int Main() { var p = flex_packed()->items; p[0] = 1; return p[0]; }\n",
     "cannot infer a pointer from this C flexible array member", ""),
    # EN: a NativeSharp parameter cannot receive the alias without discarding volatile (T08-015-05a).
    # ES: un parametro NativeSharp no recibe el alias sin descartar volatile (T08-015-05a).
    ("parametro-ns", "int Use(int* p) { return *p; }\nint Main() { var p = flex_a()->items; return Use(p); }\n",
     "no viable overload of namespace function 'Use'", ""),
    # EN: the mode is the reference's, not the consuming project's: the project property is rejected (T08-015-03a,
    #     T08-015-08).
    # ES: el modo es el de la referencia, no el del proyecto consumidor: la propiedad del proyecto se rechaza
    #     (T08-015-03a, T08-015-08).
    ("modo-consumidor", "int Main() { var p = flex_a()->items; *p = 1; return 0; }\n",
     "CppConsumerVolatileMode is not supported", "<CppConsumerVolatileMode>ISO</CppConsumerVolatileMode>"),
]


def rejections():
    for name, text, expected, properties in REJECTIONS:
        folder = root / f"rechazo-{name}"
        folder.mkdir()
        c_library(folder)
        (folder / "M.ns").write_text(text, encoding="utf-8")
        project(folder, "M", "M.ns", reference("Microsoft"), properties=properties)
        result = build(folder, "M", "0", "exe", "out/M.exe")
        if observe:
            print("rechazo", name, result.returncode, [l for l in result.stdout.splitlines() if "error" in l][:2],
                  flush=True)
        elif result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
        else:
            print("rechazo", name, "correcto", flush=True)


expected = control()
print("control Clang", expected, flush=True)
rejections()
ir_checks()
cache_check()
execution(expected)
print("observación terminada" if observe else "Alias C volatile verificados:", root)
