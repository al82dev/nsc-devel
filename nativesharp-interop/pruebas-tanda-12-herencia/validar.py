"""T12-001 a T12-011: herencia, despacho virtual y tipos dinamicos C++ consumidos desde NativeSharp.

La biblioteca declara bases publicas no polimorficas (A, B2) y una herencia multiple (M : A, B2), una
jerarquia polimorfica con destructor virtual (V, W : V), una herencia multiple con la base polimorfica
no primaria segun el ABI de Microsoft (X : A, W) y una herencia virtual (VD : virtual VB). El consumidor
NativeSharp:

- lee y escribe campos de las bases en el objeto derivado (T12-001);
- llama a metodos de la primera y la segunda base, con el ajuste de this (T12-002, T12-004, T12-008);
- convierte punteros de derivado a base, tambien nulos, y los pasa a funciones C++; un cast explicito
  entre punteros relacionados es un static_cast, hacia la base o hacia el derivado (T12-003);
- construye objetos polimorficos y llama a sus metodos virtuales, propios y heredados, directamente y
  mediante un puntero a la base (T12-005, T12-006);
- destruye mediante la base con destructor virtual, en la biblioteca (T12-007);
- llama a metodos de una base virtual (T12-009);
- con --dll, usa typeid y dynamic_cast en la biblioteca, que enlaza su CRT (T12-010, T12-011).

Los metodos heredados y virtuales se llaman mediante adaptadores que genera Clang. La biblioteca estatica
no tiene CRT: se compila sin RTTI y define sus propios operator delete, que nunca liberan. Compara un
control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3; cada declaracion C++ que NSC
llama directamente tiene los tipos fisicos de Clang. Controles negativos: base privada, campo de una
base virtual, base ambigua, conversion implicita de base a derivado y construccion de un record con base
virtual.
"""
import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t12-herencia-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T12_EXPORT)
#define API __declspec(dllexport)
#elif defined(T12_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T12 {
struct A { int a; API int getA() const noexcept; };
struct B2 { int b; API int getB() const noexcept; API void setB(int v) noexcept; };
struct M : A, B2 { int m; API M(int x) noexcept; API int sum() const noexcept; };
struct V {
    int tag;
    API V() noexcept;
    API virtual ~V();
    API virtual int f(int x) const noexcept;
    API virtual int g() const noexcept;
    API int callF(int x) const noexcept;
};
struct W : V { int w; API W(int x) noexcept; API ~W() override; API int f(int x) const noexcept override; };
struct X : A, W { API X(int x) noexcept; API int f(int x) const noexcept override; };
struct VB { int vb; API int getVB() const noexcept; API virtual int h() const noexcept; };
struct VD : virtual VB { int vd; API VD(int x) noexcept; API int h() const noexcept override; };
API int dead() noexcept;
API M* makeM(int x) noexcept;
API V* makeV() noexcept;
API V* makeW(int x) noexcept;
API X* makeX(int x) noexcept;
API VD* makeVD(int x) noexcept;
API void release(V* v) noexcept;
API int viaA(const A* p) noexcept;
API int viaB2(B2* p) noexcept;
API int viaV(const V* p, int x) noexcept;
''' + ('''API int typeCode(const V* p) noexcept;
API W* asW(V* p) noexcept;
''' if args.dll else '') + '''}
'''
# Without --dll the static library has no CRT: objects live in pools of the library, release destroys them
# through the virtual destructor, and the operator delete that the deleting destructors name never frees.
STATIC_ALLOCATION = '''inline void* operator new(decltype(sizeof(0)), void* place) noexcept { return place; }
void operator delete(void*) noexcept {}
void operator delete(void*, decltype(sizeof(0))) noexcept {}
namespace T12 {
template <class T> struct Pool { alignas(T) unsigned char slots[4][sizeof(T)]; bool used[4]; };
static Pool<M> mPool; static Pool<V> vPool; static Pool<W> wPool; static Pool<X> xPool; static Pool<VD> vdPool;
template <class T> void* take(Pool<T>& pool) noexcept {
    for (int i = 0; i < 4; ++i) if (!pool.used[i]) { pool.used[i] = true; return pool.slots[i]; }
    return nullptr;
}
M* makeM(int x) noexcept { return new (take(mPool)) M(x); }
V* makeV() noexcept { return new (take(vPool)) V(); }
V* makeW(int x) noexcept { return new (take(wPool)) W(x); }
X* makeX(int x) noexcept { return new (take(xPool)) X(x); }
VD* makeVD(int x) noexcept { return new (take(vdPool)) VD(x); }
// The pools hold complete objects; release finds the slot whose V subobject is v and destroys the object
// through the virtual destructor, without RTTI.
void release(V* v) noexcept {
    for (int i = 0; i < 4; ++i) {
        if (vPool.used[i] && reinterpret_cast<V*>(vPool.slots[i]) == v) { v->~V(); vPool.used[i] = false; return; }
        if (wPool.used[i] && static_cast<V*>(reinterpret_cast<W*>(wPool.slots[i])) == v) { v->~V(); wPool.used[i] = false; return; }
        if (xPool.used[i] && static_cast<V*>(reinterpret_cast<X*>(xPool.slots[i])) == v) { v->~V(); xPool.used[i] = false; return; }
    }
}
}
'''
DLL_ALLOCATION = '''#include <typeinfo>
namespace T12 {
M* makeM(int x) noexcept { return new M(x); }
V* makeV() noexcept { return new V(); }
V* makeW(int x) noexcept { return new W(x); }
X* makeX(int x) noexcept { return new X(x); }
VD* makeVD(int x) noexcept { return new VD(x); }
void release(V* v) noexcept { delete v; }
int typeCode(const V* p) noexcept { return typeid(*p) == typeid(W) ? 2 : typeid(*p) == typeid(V) ? 1 : 0; }
W* asW(V* p) noexcept { return dynamic_cast<W*>(p); }
}
'''
SOURCE = '''#include "api.h"
namespace T12 {
static int deaths = 0;
int A::getA() const noexcept { return a; }
int B2::getB() const noexcept { return b; }
void B2::setB(int v) noexcept { b = v; }
M::M(int x) noexcept { a = x; b = 2 * x; m = 3 * x; }
int M::sum() const noexcept { return a + b + m; }
V::V() noexcept : tag(1) {}
V::~V() { ++deaths; }
int V::f(int x) const noexcept { return x + tag; }
int V::g() const noexcept { return tag + 100; }
int V::callF(int x) const noexcept { return f(x) + 1000; }
W::W(int x) noexcept : w(x) { tag = 2; }
W::~W() { ++deaths; }
int W::f(int x) const noexcept { return x * 10 + w; }
X::X(int x) noexcept : W(x) { a = 7; }
int X::f(int x) const noexcept { return x * 100 + a; }
int VB::getVB() const noexcept { return vb; }
int VB::h() const noexcept { return vb; }
VD::VD(int x) noexcept { vb = x; vd = 2 * x; }
int VD::h() const noexcept { return vb + vd + 50; }
int dead() noexcept { return deaths; }
int viaA(const A* p) noexcept { return p->a; }
int viaB2(B2* p) noexcept { return p->getB() * 2; }
int viaV(const V* p, int x) noexcept { return p->f(x); }
}
''' + (DLL_ALLOCATION if args.dll else STATIC_ALLOCATION)
CONTROL = '''#include "api.h"
int main() {
    using namespace T12;
    int r = 0;
    M m(5);
    r += m.a + m.b + m.m;
    m.b = 7;
    r += m.getB() + m.getA() + m.sum();
    m.setB(8);
    r += m.b;
    M* pm = makeM(2);
    A* pa = pm;
    B2* pb = pm;
    r += viaA(pa) + viaB2(pb) + viaB2(pm);
    r += pb->getB() + pm->getB();
    M* nm = nullptr; B2* nb = nm; if (nb == nullptr) { r += 1; }
    W w(3);
    r += w.f(4) + w.g() + w.callF(2) + w.tag;
    V* v = makeW(6);
    r += v->f(1) + v->g() + viaV(v, 2);
    release(v);
    r += dead() * 1000;
    X x(4);
    r += x.f(3) + x.getA() + x.g() + x.a + x.w;
    X* px = makeX(5);
    V* xv = px;
    A* xa = px;
    r += viaV(xv, 1) + viaA(xa);
    B2* eb = (B2*)pm; M* em = (M*)eb; r += eb->b + em->m;
    M* en = (M*)nb; if (en == nullptr) { r += 1; }
    A* xa2 = (A*)px; X* xb = (X*)xa2; r += xb->a;
    VD* d = makeVD(4);
    r += d->getVB() + d->h() + d->vd;
''' + ('''    V* t = makeW(1);
    r += typeCode(t) * 10000;
    W* tw = asW(t);
    if (tw != nullptr) { r += tw->w * 100000; }
    release(t);
    V* base = makeV();
    if (asW(base) == nullptr) { r += 3; }
    r += typeCode(base);
    release(base);
''' if args.dll else '') + '''    return r;
}
'''
NS_BODY = ('int r = 0; '
           'T12.M m = new T12.M(5); '
           'r += m.a + m.b + m.m; '
           'm.b = 7; '
           'r += m.getB() + m.getA() + m.sum(); '
           'm.setB(8); '
           'r += m.b; '
           'T12.M* pm = T12.makeM(2); '
           'T12.A* pa = pm; '
           'T12.B2* pb = pm; '
           'r += T12.viaA(pa) + T12.viaB2(pb) + T12.viaB2(pm); '
           'r += pb->getB() + pm->getB(); '
           'T12.M* nm = null; T12.B2* nb = nm; if (nb == null) { r += 1; } '
           'T12.W w = new T12.W(3); '
           'r += w.f(4) + w.g() + w.callF(2) + w.tag; '
           'T12.V* v = T12.makeW(6); '
           'r += v->f(1) + v->g() + T12.viaV(v, 2); '
           'T12.release(v); '
           'r += T12.dead() * 1000; '
           'T12.X x = new T12.X(4); '
           'r += x.f(3) + x.getA() + x.g() + x.a + x.w; '
           'T12.X* px = T12.makeX(5); '
           'T12.V* xv = px; '
           'T12.A* xa = px; '
           'r += T12.viaV(xv, 1) + T12.viaA(xa); '
           'T12.B2* eb = (T12.B2*)pm; T12.M* em = (T12.M*)eb; r += eb->b + em->m; '
           'T12.M* en = (T12.M*)nb; if (en == null) { r += 1; } '
           'T12.A* xa2 = (T12.A*)px; T12.X* xb = (T12.X*)xa2; r += xb->a; '
           'T12.VD* d = T12.makeVD(4); '
           'r += d->getVB() + d->h() + d->vd; '
           + ('T12.V* t = T12.makeW(1); '
              'r += T12.typeCode(t) * 10000; '
              'T12.W* tw = T12.asW(t); '
              'if (tw != null) { r += tw->w * 100000; } '
              'T12.release(t); '
              'T12.V* b = T12.makeV(); '
              'if (T12.asW(b) == null) { r += 3; } '
              'r += T12.typeCode(b); '
              'T12.release(b); ' if args.dll else '')
           + 'return r;')
EXPECTED = 4051 + (120004 if args.dll else 0)
# C++ declarations NSC calls directly: 3 constructors, 2 destructors, the free functions and the own
# non-virtual methods M::sum and B2::getB. Inherited and virtual methods go through 13 Clang adapters.
DIRECT = 16 + (3 if args.dll else 0)
ADAPTERS = 13
CRT = Path('A:/vc2026-runtime/lib/x64/release')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib')
CRT_LIBRARIES = [CRT / 'libcmt.lib', CRT / 'libvcruntime.lib', SDK / 'ucrt/x64/libucrt.lib', SDK / 'um/x64/kernel32.lib']
RTTI = [] if args.dll else ['-fno-rtti']


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


def project(name, references, source, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def declarations(ir):
    """Physical parameter and return types of every T12 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T12@@[^"(]*)"?\((.*)\)', line)
        if not match:
            continue
        head, symbol, parameters = match.groups()
        result = [w for w in head.split() if w not in ('dso_local', 'dllimport', 'noundef', 'nonnull', 'zeroext',
                                                       'signext') and not w.startswith(('dereferenceable', 'align'))]
        shapes = []
        for parameter in parameters.split(', ') if parameters.strip() else []:
            words = parameter.split()
            shapes.append(words[0])
        found[symbol] = (result[-1] if result else '', tuple(shapes))
    return found


def build_library(case, opt):
    case.mkdir(parents=True)
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt), *RTTI]
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT12_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                                 *[str(path) for path in CRT_LIBRARIES]], case)
        (case / 'api.h').write_text('#define T12_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    control_ir = run(case.name + '-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                 *RTTI, '-O0', '-S', '-emit-llvm', 'Control.cpp', '-o', '-'],
                     case).stdout
    run(case.name + '-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(case.name + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                      '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    control = run(case.name + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'{case.name}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    return declarations(control_ir)


def compare(tag, clang, ir):
    """Every C++ symbol NSC calls directly is one Clang's control declares, with the same physical types;
    every inherited or virtual call goes through a Clang adapter."""
    nsc = declarations(ir)
    called = set(re.findall(r'(?:call|invoke) [^@]*@"(\?[^"]*T12@@[^"]*)"\(', ir))
    for symbol in called:
        if symbol not in clang:
            raise SystemExit(f'{tag}: NSC llama a {symbol}, que el control de Clang no declara; evidencia: {root}')
        if clang[symbol] != nsc.get(symbol):
            raise SystemExit(f'{tag}: {symbol} tiene {nsc.get(symbol)} en NSC y {clang[symbol]} en Clang; '
                             f'evidencia: {root}')
    if len(called) != DIRECT:
        raise SystemExit(f'{tag}: se esperaban {DIRECT} llamadas C++ directas en NSC, hay {len(called)}; '
                         f'evidencia: {root}')
    adapters = set(re.findall(r'(?:call|invoke) [^@]*@(__nsc_cpp_call_[0-9a-f]{16})\(', ir))
    if len(adapters) != ADAPTERS:
        raise SystemExit(f'{tag}: se esperaban {ADAPTERS} adaptadores de Clang, hay {len(adapters)}; evidencia: {root}')
    return len(called), len(adapters)


def build_and_run(tag, folder, opt, expected=EXPECTED):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'directo-O{opt}'
    clang = build_library(case, opt)
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T12App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    direct, adapters = compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden ({direct} declaraciones directas iguales a las de '
          f'Clang, {adapters} adaptadores)', flush=True)


negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('''#pragma once
namespace T12 {
struct A { int a; int getA() const noexcept; };
struct P : private A { int p; };
struct L : A { int l; };
struct R : A { int r; };
struct Dm : L, R { int dm; };
struct B2 { int b; };
struct M : A, B2 { int m; };
struct VB { int vb; };
struct VD : virtual VB { int vd; VD(int x) noexcept; };
P* makeP() noexcept;
Dm* makeDm() noexcept;
M* makeM() noexcept;
VD* makeVD() noexcept;
}
''', encoding='utf-8')
(negative / 'api.cpp').write_text('''#include "api.h"
inline void* operator new(decltype(sizeof(0)), void* place) noexcept { return place; }
namespace T12 {
int A::getA() const noexcept { return a; }
VD::VD(int x) noexcept { vb = x; vd = x; }
static P p; static Dm dm; static M m;
alignas(VD) static unsigned char vd[sizeof(VD)];
P* makeP() noexcept { p.p = 1; return &p; }
Dm* makeDm() noexcept { dm.dm = 1; dm.l = 1; dm.r = 1; return &dm; }
M* makeM() noexcept { m.a = 1; m.b = 1; m.m = 1; return &m; }
VD* makeVD() noexcept { return new (vd) VD(1); }
}
''', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-fno-rtti', '-c', 'api.cpp',
                      '-o', 'api.obj'], negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
NEGATIVES = (
    ('base-privada', 'T12.P* p = T12.makeP(); return p->a;', "no compatible member named 'a'"),
    ('base-privada-metodo', 'T12.P* p = T12.makeP(); return p->getA();', "no compatible member named 'getA'"),
    ('campo-base-virtual', 'T12.VD* d = T12.makeVD(); return d->vb;', "no compatible member named 'vb'"),
    ('campo-ambiguo', 'T12.Dm* q = T12.makeDm(); return q->a;', "no compatible member named 'a'"),
    ('metodo-ambiguo', 'T12.Dm* q = T12.makeDm(); return q->getA();', "no compatible member named 'getA'"),
    ('conversion-ambigua', 'T12.Dm* q = T12.makeDm(); T12.A* a = q; return a->a;', "cannot initialize local 'a'"),
    ('conversion-base-privada', 'T12.P* p = T12.makeP(); T12.A* a = p; return a->a;', "cannot initialize local 'a'"),
    ('conversion-a-derivado', 'T12.B2* b = T12.makeM(); T12.M* m = b; return m->m;', "cannot initialize local 'm'"),
    ('constructor-base-virtual', 'T12.VD v = new T12.VD(1); return v.vd;',
     'struct has only an implicit parameterless constructor'),
)
for name, body, message in NEGATIVES:
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T12Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado', flush=True)
for name, body in (('contraparte-conversion', 'T12.M* m = T12.makeM(); T12.B2* b = m; T12.A* a = m; '
                                              'return b->b + a->a + m->m + m->getA() - 4;'),
                   ('contraparte-campo-virtual', 'T12.VD* d = T12.makeVD(); return d->vd - 1;')):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T12Pos', plain, 'Main.ns'), encoding='utf-8')
    build_and_run(name, folder, 0, expected=0)
    print(f'{name}: pasa', flush=True)
print('OK: herencia, despacho virtual y tipos dinamicos C++ frente a Clang; evidencia:', root)
