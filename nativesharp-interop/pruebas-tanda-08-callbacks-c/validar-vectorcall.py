"""T08-D22 (T08-017-03d1): `__vectorcall` C functions and callbacks.

NativeSharp calls `__vectorcall` C functions directly (static library and DLL) and through `__vectorcall`
function pointers, passes C functions and its own methods to C as `__vectorcall` callbacks, and keeps the C
function's address (T08-D18). The signatures cover Clang's x64 vectorcall classification (WinX86_64ABIInfo):
- more than six floating arguments (only the first six positions use XMM registers);
- homogeneous float/double aggregates (HFA) of two to four members, in registers and returned in registers;
- an 8-byte HFA (`F2`), which goes by reference when no registers are left;
- the third `D2` of `v_many`, which goes by reference because the registers run out;
- nested records and fixed arrays inside an HFA (`Nest`, `Arr`);
- non-homogeneous records (`Mixed`, `Big`), which keep the ordinary Win64 rule, and a `Big` result (sret);
- records by value through an ordinary C callback (`PlainD2Fn`), newly mapped for C function pointers;
- a C function passed where the other convention is expected is rejected (cdecl and `__vectorcall`).
The body also runs in a Clang control. Positive cases run in O0-O3, directly (static library and DLL) and
from a precompiled producer consumed with and without the header; rejections run in O0.
T08-D22: funciones y callbacks C `__vectorcall`, con el control de Clang como referencia.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-vectorcall-", dir=r"A:\nsc-interop-build"))
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

HEADER = """#ifdef VC_DLL_BUILD
#define VC_API __declspec(dllexport)
#else
#define VC_API
#endif
typedef struct D2 { double x, y; } D2;
typedef struct F2 { float a, b; } F2;
typedef struct F4 { float a, b, c, d; } F4;
typedef struct D4 { double a, b, c, d; } D4;
typedef struct Nest { D2 p; double z; } Nest;
typedef struct Arr { float v[3]; } Arr;
typedef struct Mixed { double x; float y; } Mixed;
typedef struct Big { long long a, b, c; } Big;
typedef double (__vectorcall *VecFn)(double, double);
typedef double (__vectorcall *ScalarFn)(double, float, int, long long, double, double, double, double);
typedef D2 (__vectorcall *D2Fn)(D2, int, D2);
typedef double (__vectorcall *ManyFn)(D2, D2, D2, double, double);
typedef F4 (__vectorcall *F4Fn)(F4, F2);
typedef double (__vectorcall *F2sFn)(F4, F2, F2);
typedef F2 (__vectorcall *RF2Fn)(float);
typedef double (*PlainFn)(double, double);
typedef D2 (*PlainD2Fn)(D2, F2);
VC_API double __vectorcall vec_add(double a, double b);
VC_API double __vectorcall v_scalar(double a, float b, int c, long long d, double e, double f, double g, double h);
VC_API D2 __vectorcall v_d2(D2 p, int k, D2 q);
VC_API double __vectorcall v_many(D2 a, D2 b, D2 c, double d, double e);
VC_API F4 __vectorcall v_f4(F4 p, F2 q);
VC_API D4 __vectorcall v_d4(D4 p, double k);
VC_API double __vectorcall v_nest(Nest n, Arr a);
VC_API double __vectorcall v_mixed(Mixed m, Big b);
VC_API Big __vectorcall v_big(long long x);
VC_API double __vectorcall v_f2s(F4 a, F2 b, F2 c);
VC_API F2 __vectorcall v_rf2(float x);
VC_API double apply_f2s(F2sFn fn);
VC_API double apply_rf2(RF2Fn fn);
VC_API double plain_add(double a, double b);
VC_API D2 plain_d2(D2 p, F2 q);
VC_API double apply_plain_d2(PlainD2Fn fn);
VC_API double apply_vec(VecFn fn, double a, double b);
VC_API double apply_scalar(ScalarFn fn);
VC_API double apply_d2(D2Fn fn);
VC_API double apply_many(ManyFn fn);
VC_API double apply_f4(F4Fn fn);
VC_API VecFn vec_address(void);
VC_API int same_vec(VecFn fn);
"""
SOURCE = """#include "api.h"
#define VC_EXPORT
#ifdef VC_DLL_BUILD
int _fltused = 0;
#endif
VC_EXPORT double __vectorcall vec_add(double a, double b) { return a + b; }
VC_EXPORT double __vectorcall v_scalar(double a, float b, int c, long long d, double e, double f, double g,
                                       double h)
{ return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8; }
VC_EXPORT D2 __vectorcall v_d2(D2 p, int k, D2 q) { D2 r = {p.x * k + q.x, p.y * k + q.y}; return r; }
VC_EXPORT double __vectorcall v_many(D2 a, D2 b, D2 c, double d, double e)
{ return a.x + a.y * 2 + b.x * 3 + b.y * 4 + c.x * 5 + c.y * 6 + d * 7 + e * 8; }
VC_EXPORT F4 __vectorcall v_f4(F4 p, F2 q) { F4 r = {p.a + q.a, p.b + q.b, p.c * q.a, p.d * q.b}; return r; }
VC_EXPORT D4 __vectorcall v_d4(D4 p, double k) { D4 r = {p.a * k, p.b * k, p.c * k, p.d * k}; return r; }
VC_EXPORT double __vectorcall v_nest(Nest n, Arr a)
{ return n.p.x + n.p.y * 2 + n.z * 3 + a.v[0] + a.v[1] * 2 + a.v[2] * 3; }
VC_EXPORT double __vectorcall v_mixed(Mixed m, Big b)
{ return m.x + m.y * 2 + (double)(b.a + b.b * 2 + b.c * 3); }
VC_EXPORT Big __vectorcall v_big(long long x) { Big r = {x, x + 1, x + 2}; return r; }
VC_EXPORT double __vectorcall v_f2s(F4 a, F2 b, F2 c)
{ return a.a + a.d * 2 + b.a * 3 + b.b * 4 + c.a * 5 + c.b * 6; }
VC_EXPORT F2 __vectorcall v_rf2(float x) { F2 r = {x, x * 2}; return r; }
VC_EXPORT double apply_f2s(F2sFn fn) { F4 a = {1, 2, 3, 4}; F2 b = {5, 6}; F2 c = {7, 8}; return fn(a, b, c); }
VC_EXPORT double apply_rf2(RF2Fn fn) { F2 r = fn(3); return r.a * 10 + r.b; }
VC_EXPORT double plain_add(double a, double b) { return a + b; }
VC_EXPORT D2 plain_d2(D2 p, F2 q) { D2 r = {p.x + q.a, p.y * q.b}; return r; }
VC_EXPORT double apply_plain_d2(PlainD2Fn fn)
{ D2 p = {1, 2}; F2 q = {3, 4}; D2 r = fn(p, q); return r.x * 100 + r.y; }
VC_EXPORT double apply_vec(VecFn fn, double a, double b) { return fn(a, b); }
VC_EXPORT double apply_scalar(ScalarFn fn) { return fn(1, 2, 3, 4, 5, 6, 7, 8); }
VC_EXPORT double apply_d2(D2Fn fn)
{ D2 p = {1, 2}; D2 q = {3, 4}; D2 r = fn(p, 5, q); return r.x * 100 + r.y; }
VC_EXPORT double apply_many(ManyFn fn)
{ D2 a = {1, 2}; D2 b = {3, 4}; D2 c = {5, 6}; return fn(a, b, c, 7, 8); }
VC_EXPORT double apply_f4(F4Fn fn)
{ F4 p = {1, 2, 3, 4}; F2 q = {5, 6}; F4 r = fn(p, q); return r.a * 1000 + r.b * 100 + r.c * 10 + r.d; }
VC_EXPORT VecFn vec_address(void) { return vec_add; }
VC_EXPORT int same_vec(VecFn fn) { return fn == vec_add; }
"""
# NativeSharp callbacks, also compiled by Clang (NSVC is __vectorcall there).
# Callbacks NativeSharp, tambien compilados por Clang (NSVC es __vectorcall alli).
CALLBACKS = """NSVC double NsAdd(double a, double b) { return a * 10.0 + b; }
NSVC double NsScalar(double a, float b, int c, long d, double e, double f, double g, double h)
{ return a + b * 2.0 + c * 3.0 + d * 4.0 + e * 5.0 + f * 6.0 + g * 7.0 + h * 8.0 + 1000.0; }
NSVC D2 NsD2(D2 p, int k, D2 q) { D2 r; r.x = p.x + q.x * k; r.y = p.y + q.y; return r; }
NSVC double NsMany(D2 a, D2 b, D2 c, double d, double e)
{ return a.x * 2.0 + a.y + b.x + b.y + c.x * 100.0 + c.y * 1000.0 + d + e; }
NSVC F4 NsF4(F4 p, F2 q) { F4 r; r.a = q.a; r.b = q.b; r.c = p.c; r.d = p.d; return r; }
NSVC double NsF2s(F4 a, F2 b, F2 c) { return c.a * 100.0 + c.b + b.a; }
NSVC F2 NsRf2(float x) { F2 r; r.a = x + 1.0f; r.b = x; return r; }
NSC D2 NsPlainD2(D2 p, F2 q) { D2 r; r.x = p.x * q.a; r.y = p.y + q.b; return r; }
"""
BODY = """    // Direct calls / llamadas directas.
    if (vec_add(1.0, 2.0) != 3.0) return 1;
    if (v_scalar(1.0, 2.0f, 3, 4, 5.0, 6.0, 7.0, 8.0) != 204.0) return 2;
    D2 p;
    p.x = 1.0;
    p.y = 2.0;
    D2 q;
    q.x = 10.0;
    q.y = 20.0;
    D2 r = v_d2(p, 3, q);
    if (r.x != 13.0 || r.y != 26.0) return 3;
    D2 c;
    c.x = 5.0;
    c.y = 6.0;
    if (v_many(p, q, c, 7.0, 8.0) != 289.0) return 4;
    F4 f;
    f.a = 1.0f;
    f.b = 2.0f;
    f.c = 3.0f;
    f.d = 4.0f;
    F2 g;
    g.a = 5.0f;
    g.b = 6.0f;
    F4 fr = v_f4(f, g);
    if (fr.a != 6.0f || fr.b != 8.0f || fr.c != 15.0f || fr.d != 24.0f) return 5;
    D4 d;
    d.a = 1.0;
    d.b = 2.0;
    d.c = 3.0;
    d.d = 4.0;
    D4 dr = v_d4(d, 2.0);
    if (dr.a != 2.0 || dr.b != 4.0 || dr.c != 6.0 || dr.d != 8.0) return 6;
    Nest n;
    n.p = p;
    n.z = 3.0;
    Arr a;
    a.v[0] = 1.0f;
    a.v[1] = 2.0f;
    a.v[2] = 3.0f;
    if (v_nest(n, a) != 28.0) return 7;
    Mixed m;
    m.x = 1.0;
    m.y = 2.0f;
    Big b = v_big(4);
    if (b.a != 4 || b.b != 5 || b.c != 6) return 8;
    if (v_mixed(m, b) != 37.0) return 9;
    // __vectorcall function pointers and C callbacks / punteros a funcion y callbacks C.
    VecFn v = vec_add;
    if (v(2.0, 3.0) != 5.0 || apply_vec(v, 4.0, 5.0) != 9.0 || apply_vec(vec_add, 1.0, 1.0) != 2.0) return 10;
    if (same_vec(v) != 1 || same_vec(vec_add) != 1 || vec_address() != v) return 11;
    VecFn fromC = vec_address();
    if (fromC(6.0, 7.0) != 13.0) return 12;
    D2Fn d2f = v_d2;
    D2 r2 = d2f(p, 2, q);
    if (r2.x != 12.0 || r2.y != 24.0 || apply_d2(v_d2) != 814.0) return 13;
    ManyFn mf = v_many;
    if (mf(p, q, c, 7.0, 8.0) != 289.0 || apply_many(v_many) != 204.0) return 14;
    F4Fn f4f = v_f4;
    F4 fr2 = f4f(f, g);
    if (fr2.a != 6.0f || fr2.d != 24.0f || apply_f4(v_f4) != 6974.0) return 15;
    ScalarFn sf = v_scalar;
    if (sf(1.0, 2.0f, 3, 4, 5.0, 6.0, 7.0, 8.0) != 204.0 || apply_scalar(v_scalar) != 204.0) return 16;
    // NativeSharp (or C control) callbacks called by C / callbacks propios llamados por C.
    if (apply_vec(CB(NsAdd), 1.0, 2.0) != 12.0) return 17;
    if (apply_scalar(CB(NsScalar)) != 1204.0) return 18;
    if (apply_d2(CB(NsD2)) != 1606.0) return 19;
    if (apply_many(CB(NsMany)) != 6526.0) return 20;
    if (apply_f4(CB(NsF4)) != 5634.0) return 21;
    VecFn mine = CB(NsAdd);
    if (mine(1.0, 2.0) != 12.0) return 22;
    // Records by value through an ordinary C callback / records por valor en un callback C ordinario.
    PlainD2Fn pd = plain_d2;
    D2 pr = pd(p, g);
    if (pr.x != 6.0 || pr.y != 12.0 || apply_plain_d2(plain_d2) != 408.0) return 23;
    if (apply_plain_d2(CB(NsPlainD2)) != 306.0) return 24;
    // An 8-byte HFA by reference when the registers run out, and returned in registers.
    // Una HFA de 8 bytes por referencia cuando se agotan los registros, y devuelta en registros.
    F2 h;
    h.a = 7.0f;
    h.b = 8.0f;
    F2sFn f2s = v_f2s;
    if (v_f2s(f, g, h) != 131.0 || f2s(f, g, h) != 131.0 || apply_f2s(v_f2s) != 131.0) return 25;
    if (apply_f2s(CB(NsF2s)) != 713.0) return 26;
    F2 rf = v_rf2(3.0f);
    if (rf.a != 3.0f || rf.b != 6.0f || apply_rf2(v_rf2) != 36.0 || apply_rf2(CB(NsRf2)) != 43.0) return 27;
    return 0;
"""
REJECTIONS = [
    ("convencion-c-a-vectorcall", "VecFn v = plain_add; return 0;",
     "C function 'plain_add' uses the 'cdecl' calling convention"),
    ("convencion-vectorcall-a-c", "PlainFn v = vec_add; return 0;",
     "C function 'vec_add' uses the 'vectorcall' calling convention"),
    ("convencion-c-a-vectorcall-argumento", "return apply_vec(plain_add, 1.0, 2.0) == 3.0 ? 0 : 1;",
     "C function 'plain_add' uses the 'cdecl' calling convention"),
]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def ns_text(text, owner):
    text = text.replace("NSVC ", "").replace("NSC ", "")
    return re.sub(r"CB\((\w+)\)", (owner + r".\1") if owner else r"\1", text)


def c_text(text):
    text = text.replace("NSVC ", "__vectorcall ").replace("NSC ", "").replace("long d,", "long long d,")
    return re.sub(r"CB\((\w+)\)", r"\1", text)


def c_library(folder, dll=False):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    defines = ["-DVC_DLL_BUILD"] if dll else []
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", *defines, "api.c", "-o", "api.obj"], folder)
    if dll:
        run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:vcapi.dll", "/implib:vcapi.lib",
             "api.obj"], folder)
        # EN: the consumer sees `dllimport`, as C needs to keep the DLL's own address (T08-D18).
        # ES: el consumidor ve `dllimport`, como C lo necesita para conservar la direccion de la DLL.
        (folder / "api.h").write_text(HEADER.replace(
            "#define VC_API\n#endif", "#define VC_API __declspec(dllimport)\n#endif"), encoding="utf-8")
    else:
        run([lib, "/nologo", "/out:vcapi.lib", "api.obj"], folder)


def reference(dll=False):
    runtime = ' Runtime="vcapi.dll"' if dll else ""
    return '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="vcapi.lib"' + runtime + " />"


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def expect_run(label, folder, name, opt, cwd=None):
    result = build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], cwd or Path("A:/"), check=False).returncode \
        if result.returncode == 0 else None
    if observe:
        print(label, result.returncode, code, errors(result), flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def control():
    folder = root / "control"
    folder.mkdir()
    c_library(folder)
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\nint _fltused = 0;\n' +
        c_text(CALLBACKS) + f"static int run(void)\n{{\n{c_text(BODY)}}}\n"
        "void mainCRTStartup(void) { ExitProcess(run()); }\n", encoding="utf-8")
    result = run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
                 folder, check=False)
    if result.returncode:
        raise AssertionError(f"control Clang:\n{result.stdout}")
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "vcapi.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    print("control Clang correcto", flush=True)


def class_source(namespace, generic):
    methods = ns_text("\n".join(("public static " + l) if l.startswith(("NSVC ", "NSC ")) else l
                                for l in CALLBACKS.splitlines()), "") + "\n"
    run_method = ("public static int Run<T>(T unused)\n{\n" if generic else "public static int Run()\n{\n")
    return (f"namespace {namespace} {{ public class Probe {{\n" + methods + run_method +
            ns_text(BODY, "Probe") + "} } }\n")


def paths(opt):
    for dll in (False, True):
        kind = "dll" if dll else "lib"
        direct = root / f"directo-{kind}-O{opt}"
        direct.mkdir()
        c_library(direct, dll)
        (direct / "Main.ns").write_text(class_source("Vc", False) + "int Main() { return Vc.Probe.Run(); }\n",
                                        encoding="utf-8")
        project(direct, "Direct", "Main.ns", reference(dll))
        expect_run(f"directo {kind} O{opt}", direct, "Direct", opt, direct)
    base = root / f"precompilado-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text(class_source("Vectorcall", True), encoding="utf-8")
    project(producer, "VcPub", "P.ns", reference(), library=True)
    result = build(producer, "VcPub", opt, "lib", "VcPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    with_header = base / "con-encabezado"
    with_header.mkdir()
    (with_header / "C.ns").write_text("int Main() { return Vectorcall.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(with_header, "C", "C.ns", '<LibraryReference Include="../producer/VcPub.lib" />'
            '<CppLibraryReference Header="../producer/api.h" Language="C" Standard="c11" />')
    expect_run(f"precompilado con encabezado O{opt}", with_header, "C", opt)
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Vectorcall.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/VcPub.lib" />')
    expect_run(f"precompilado sin encabezado O{opt}", base, "C", opt)


def rejections():
    for name, body, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder)
        (folder / "Main.ns").write_text("int Main() { " + body + " }\n", encoding="utf-8")
        project(folder, "N", "Main.ns", reference())
        result = build(folder, "N", "0", "exe", "out/N.exe")
        if observe:
            print("rechazo", name, result.returncode, errors(result), flush=True)
        elif result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{text}»:\n{result.stdout}")
        else:
            print("rechazo", name, "correcto", flush=True)


control()
for opt in opts:
    paths(opt)
rejections()
print("observación terminada" if observe else "Vectorcall C verificado:", root)
