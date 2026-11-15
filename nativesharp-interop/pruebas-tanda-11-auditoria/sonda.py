"""Sonda de auditoria T11 (no es una prueba de soporte): clases C++ y vida de objetos consumidos desde NativeSharp.

Cada caso compila una biblioteca C++ con Clang 23.1.0 y un consumidor NativeSharp que devuelve 0 si
los valores coinciden. Imprime: compila y ejecuta (codigo), o el primer diagnostico de NSC.
"""
import os, subprocess, sys, tempfile
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t11-sonda-', dir='A:/temp'))
env = os.environ.copy(); env['TEMP'] = env['TMP'] = str(root)
env['NSC_LINKER'] = 'nslink.exe'
env['NSC_WINDOWS_RUNTIME_LIBRARY'] = 'A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
print('Evidencia:', root, flush=True)


def ex(cwd, label, cmd):
    r = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (cwd / (label + '.log')).write_text(repr(cmd) + '\nEXIT ' + str(r.returncode) + '\n' + r.stdout, encoding='utf-8')
    return r.returncode, r.stdout


def err(t):
    lines = [l.strip()[:220] for l in t.splitlines() if 'error' in l]
    return ' | '.join(lines)[:400] if lines else t.strip()[-300:]


# (name, header, implementation, NativeSharp body)
CASES = [
    ('campos-struct', 'namespace A { struct P { int x; int y; }; int sum(P* p); }',
     'namespace A { int sum(P* p) { return p->x + p->y; } }',
     'A.P p; p.x = 2; p.y = 3; return A.sum(&p) == 5 ? 0 : 1;'),
    ('campos-class', 'namespace A { class P { public: int x; int y; }; int sum(P* p); }',
     'namespace A { int sum(P* p) { return p->x + p->y; } }',
     'A.P p; p.x = 2; p.y = 3; return A.sum(&p) == 5 ? 0 : 1;'),
    ('campo-privado', 'namespace A { class P { int hidden = 1; public: int x; int get() const; }; }',
     'namespace A { int P::get() const { return hidden + x; } }',
     'A.P p; p.x = 2; return p.hidden;'),
    ('sizeof-alineacion', 'namespace A { struct alignas(16) Q { int a; char b; }; int size(); }',
     'namespace A { int size() { return sizeof(Q) * 100 + alignof(Q); } }',
     'return (int)sizeof(A.Q) * 100 + 16 == A.size() ? 0 : 1;'),
    ('metodo-instancia', 'namespace A { struct C { int v; int get() const; void put(int x); }; }',
     'namespace A { int C::get() const { return v; } void C::put(int x) { v = x; } }',
     'A.C c; c.v = 0; c.put(4); return c.get() == 4 ? 0 : 1;'),
    ('metodo-sobrecarga', 'namespace A { struct C { int v; int f(int x) const; int f(double x) const; }; }',
     'namespace A { int C::f(int x) const { return v + x; } int C::f(double x) const { return v + (int)(x * 2); } }',
     'A.C c; c.v = 1; return c.f(2) == 3 && c.f(1.5) == 4 ? 0 : 1;'),
    ('metodo-inline', 'namespace A { struct C { int v; int twice() const { return v * 2; } }; }',
     '',
     'A.C c; c.v = 4; return c.twice() == 8 ? 0 : 1;'),
    ('metodo-privado', 'namespace A { struct C { int v; private: int secret() const; }; }',
     'namespace A { int C::secret() const { return v; } }',
     'A.C c; c.v = 4; return c.secret();'),
    ('metodo-record-valor', 'namespace A { struct P { int x; int y; }; struct C { int v; P scaled(P p) const; }; }',
     'namespace A { P C::scaled(P p) const { return {p.x * v, p.y * v}; } }',
     'A.C c; c.v = 3; A.P p; p.x = 1; p.y = 2; A.P q = c.scaled(p); return q.x + q.y == 9 ? 0 : 1;'),
    ('metodo-ref-retorno', 'namespace A { struct C { int v; int& slot(); }; }',
     'namespace A { int& C::slot() { return v; } }',
     'A.C c; c.v = 1; c.slot() = 7; return c.v == 7 ? 0 : 1;'),
    ('constructor', 'namespace A { struct C { int v; C(int x); }; }',
     'namespace A { C::C(int x) : v(x * 2) {} }',
     'A.C c = new A.C(5); return c.v == 10 ? 0 : 1;'),
    ('constructor-defecto', 'namespace A { struct C { int v; C(); }; }',
     'namespace A { C::C() : v(9) {} }',
     'A.C c = new A.C(); return c.v == 9 ? 0 : 1;'),
    ('constructor-sobrecarga', 'namespace A { struct C { int v; C(int x); C(double x); }; }',
     'namespace A { C::C(int x) : v(x) {} C::C(double x) : v((int)(x * 10)) {} }',
     'A.C a = new A.C(3); A.C b = new A.C(0.5); return a.v + b.v == 8 ? 0 : 1;'),
    ('destructor', 'namespace A { extern int dtors; struct D { int v; D(int x); ~D(); }; }',
     'namespace A { int dtors = 0; D::D(int x) : v(x) {} D::~D() { dtors += v; } }',
     'if (true) { A.D d = new A.D(4); } return A.dtors == 4 ? 0 : 1;'),
    ('destructor-orden', 'namespace A { extern int log; struct D { int v; D(int x); ~D(); }; }',
     'namespace A { int log = 0; D::D(int x) : v(x) {} D::~D() { log = log * 10 + v; } }',
     'if (true) { A.D a = new A.D(1); A.D b = new A.D(2); } return A.log == 21 ? 0 : 1;'),
    ('copia', 'namespace A { extern int copies; struct K { int v; K(int x); K(const K& o); }; }',
     'namespace A { int copies = 0; K::K(int x) : v(x) {} K::K(const K& o) : v(o.v + 1) { ++copies; } }',
     'A.K a = new A.K(1); A.K b = a; return b.v == 2 && A.copies == 1 ? 0 : 1;'),
    ('copia-eliminada', 'namespace A { struct U { int v; U(int x); U(const U&) = delete; }; }',
     'namespace A { U::U(int x) : v(x) {} }',
     'A.U a = new A.U(1); A.U b = a; return b.v;'),
    ('movimiento', 'namespace A { extern int moves; struct M { int v; M(int x); M(M&& o); M(const M&) = delete; }; M make(int x); }',
     'namespace A { int moves = 0; M::M(int x) : v(x) {} M::M(M&& o) : v(o.v) { o.v = 0; ++moves; } M make(int x) { return M(x); } }',
     'A.M a = A.make(3); return a.v == 3 ? 0 : 1;'),
    ('fabrica-puntero', 'namespace A { struct C { int v; }; C* create(int x); void release(C* c); }',
     'namespace A { C* create(int x) { return new C{x}; } void release(C* c) { delete c; } }',
     'A.C* c = A.create(6); int r = c->v; A.release(c); return r == 6 ? 0 : 1;'),
    ('puntero-no-propietario', 'namespace A { struct C { int v; }; C* shared(); }',
     'namespace A { C* shared() { static C c{7}; return &c; } }',
     'A.C* c = A.shared(); c->v = c->v + 1; return A.shared()->v == 8 ? 0 : 1;'),
    ('union-cpp', 'namespace A { union U { int i; float f; }; int bits(U u); }',
     'namespace A { int bits(U u) { return u.i; } }',
     'A.U u; u.i = 5; return A.bits(u) == 5 ? 0 : 1;'),
    ('operador-miembro', 'namespace A { struct V { int x; V operator+(V o) const; }; }',
     'namespace A { V V::operator+(V o) const { V r; r.x = x + o.x; return r; } }',
     'A.V a; a.x = 2; A.V b; b.x = 3; A.V c = a + b; return c.x == 5 ? 0 : 1;'),
    ('miembro-no-trivial', 'namespace A { struct In { int v; In(); ~In(); }; struct Out { In in; int w; }; extern int live; }',
     'namespace A { int live = 0; In::In() : v(1) { ++live; } In::~In() { --live; } }',
     'if (true) { A.Out o = new A.Out(); } return A.live == 0 ? 0 : 1;'),
    ('constructor-noexcept', 'namespace A { struct C { int v; C(int x) noexcept; ~C() noexcept; }; }',
     'namespace A { C::C(int x) noexcept : v(x * 2) {} C::~C() noexcept {} }',
     'A.C c = new A.C(5); return c.v == 10 ? 0 : 1;'),
    ('destructor-noexcept', 'namespace A { extern int log; struct D { int v; D(int x) noexcept; ~D(); }; }',
     'namespace A { int log = 0; D::D(int x) noexcept : v(x) {} D::~D() { log = log * 10 + v; } }',
     'if (true) { A.D a = new A.D(1); A.D b = new A.D(2); } return A.log == 21 ? 0 : 1;'),
    ('copia-noexcept', 'namespace A { extern int copies; struct K { int v; K(int x) noexcept; K(const K& o) noexcept; ~K(); }; }',
     'namespace A { int copies = 0; K::K(int x) noexcept : v(x) {} K::K(const K& o) noexcept : v(o.v + 1) { ++copies; } K::~K() {} }',
     'A.K a = new A.K(1); A.K b = copy(a); return b.v == 2 && A.copies == 1 ? 0 : 1;'),
    ('movimiento-noexcept', 'namespace A { extern int moves; struct M { int v; M(int x) noexcept; M(M&& o) noexcept; ~M(); }; }',
     'namespace A { int moves = 0; M::M(int x) noexcept : v(x) {} M::M(M&& o) noexcept : v(o.v) { o.v = 0; ++moves; } M::~M() {} }',
     'A.M a = new A.M(3); A.M b = move(a); return b.v == 3 ? 0 : 1;'),
    ('metodo-en-objeto-vivo', 'namespace A { struct C { int v; C(int x) noexcept; ~C(); int get() const; }; }',
     'namespace A { C::C(int x) noexcept : v(x) {} C::~C() {} int C::get() const { return v; } }',
     'A.C c = new A.C(4); return c.get() == 4 ? 0 : 1;'),
    ('alineacion-local', 'namespace A { struct alignas(32) Q { int a; Q() noexcept; ~Q() noexcept; int aligned() const; }; }',
     'namespace A { Q::Q() noexcept : a(1) {} Q::~Q() noexcept {} int Q::aligned() const { return ((unsigned long long)this % 32) == 0; } }',
     'A.Q q = new A.Q(); return q.aligned() == 1 ? 0 : 1;'),
    ('campo-objeto-vivo', 'namespace A { struct C { int v; C(int x) noexcept; ~C() noexcept; }; int peek(C* c); }',
     'namespace A { C::C(int x) noexcept : v(x) {} C::~C() noexcept {} int peek(C* c) { return c->v; } }',
     'A.C c = new A.C(4); c.v = c.v + 1; return A.peek(&c) == 5 ? 0 : 1;'),
    ('metodo-objeto-vivo', 'namespace A { struct C { int v; C(int x) noexcept; ~C() noexcept; int value() const; void add(int x); }; }',
     'namespace A { C::C(int x) noexcept : v(x) {} C::~C() noexcept {} int C::value() const { return v; } void C::add(int x) { v += x; } }',
     'A.C c = new A.C(4); c.add(3); return c.value() == 7 ? 0 : 1;'),
    ('metodo-enum', 'namespace A { enum class E : short { X = 1, Y = 2 }; struct C { int v; int scale(E e) const; E kind() const; }; }',
     'namespace A { int C::scale(E e) const { return v * (int)e; } E C::kind() const { return E::Y; } }',
     'A.C c; c.v = 3; return c.scale(A.E.Y) == 6 && c.kind() == A.E.Y ? 0 : 1;'),
    ('metodo-puntero-record', 'namespace A { struct P { int x; }; struct C { int v; int read(const P* p) const; }; }',
     'namespace A { int C::read(const P* p) const { return v + p->x; } }',
     'A.C c; c.v = 1; A.P p; p.x = 2; return c.read(&p) == 3 ? 0 : 1;'),
    ('metodo-in', 'namespace A { struct P { int x; }; struct C { int v; int read(const P& p) const; }; }',
     'namespace A { int C::read(const P& p) const { return v + p.x; } }',
     'A.C c; c.v = 1; A.P p; p.x = 2; return c.read(p) == 3 ? 0 : 1;'),
    ('copia-trivial-con-constructor', 'namespace A { struct T { int v; T(int x) noexcept; }; }',
     'namespace A { T::T(int x) noexcept : v(x) {} }',
     'A.T a = new A.T(3); A.T b = a; return b.v == 3 ? 0 : 1;'),
    ('constructor-lanzador', 'namespace A { struct C { int v; C(int x); ~C() noexcept; }; }',
     'namespace A { C::C(int x) : v(x) { if (x < 0) throw 1; } C::~C() noexcept {} }',
     'A.C c = new A.C(3); return c.v == 3 ? 0 : 1;'),
    ('destructor-privado', 'namespace A { struct C { int v; C() noexcept; private: ~C() noexcept; }; }',
     'namespace A { C::C() noexcept : v(1) {} C::~C() noexcept {} }',
     'A.C c = new A.C(); return c.v;'),
    ('constructor-privado', 'namespace A { struct C { int v; private: C(int x) noexcept; public: ~C() noexcept; }; }',
     'namespace A { C::C(int x) noexcept : v(x) {} C::~C() noexcept {} }',
     'A.C c = new A.C(1); return c.v;'),
]

for name, header, impl, body in CASES:
    if only and name not in only:
        continue
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text('#pragma once\n' + header + '\n', encoding='utf-8')
    (case / 'api.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n' + impl + '\n', encoding='utf-8')
    c, t = ex(case, 'clang', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'])
    if c:
        print(f'{name}: Clang rechaza: {err(t)}', flush=True)
        continue
    ex(case, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    (case / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T11App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib" /></ItemGroup></Project>',
        encoding='utf-8')
    c, t = ex(case, 'nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root,
                            '--no-artifact-cache', '-O0', '-o', 'a.exe'])
    if c:
        print(f'{name}: NSC rechaza: {err(t)}', flush=True)
        continue
    code = ex(case, 'run', [str(case / 'a.exe')])[0]
    print(f'{name}: ejecuta {code}', flush=True)
print('FIN', flush=True)
