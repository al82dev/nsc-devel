"""Sonda de auditoria T10 (no es una prueba de soporte): nombres, sobrecargas, estaticos y operadores C++ consumidos desde NativeSharp.

Cada caso compila una biblioteca C++ con Clang 23.1.0 y un consumidor NativeSharp que devuelve 0 si
los valores coinciden. Imprime: compila y ejecuta (codigo), o el primer diagnostico de NSC.
"""
import os, subprocess, sys, tempfile
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t10-sonda-', dir='A:/temp'))
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
    ('ns-anidado', 'namespace A { namespace B { int f(int x); } }',
     'namespace A { namespace B { int f(int x) { return x + 1; } } }',
     'return A.B.f(3) == 4 ? 0 : 1;'),
    ('ns-anidado-c17', 'namespace A::B::C { int f(int x); }',
     'namespace A::B::C { int f(int x) { return x + 2; } }',
     'return A.B.C.f(3) == 5 ? 0 : 1;'),
    ('ns-homonimos', 'namespace A { int f(int x); } namespace B { int f(int x); }',
     'namespace A { int f(int x) { return x + 1; } } namespace B { int f(int x) { return x + 10; } }',
     'return A.f(1) == 2 && B.f(1) == 11 ? 0 : 1;'),
    ('ns-inline', 'namespace A { inline namespace v1 { int f(int x); } }',
     'namespace A { inline namespace v1 { int f(int x) { return x * 2; } } }',
     'return A.f(3) == 6 ? 0 : 1;'),
    ('ns-inline-explicito', 'namespace A { inline namespace v1 { int f(int x); } }',
     'namespace A { inline namespace v1 { int f(int x) { return x * 2; } } }',
     'return A.v1.f(3) == 6 ? 0 : 1;'),
    ('ns-alias', 'namespace A { namespace B { int f(int x); } } namespace AB = A::B;',
     'namespace A { namespace B { int f(int x) { return x + 1; } } }',
     'return AB.f(3) == 4 ? 0 : 1;'),
    ('ns-using-decl', 'namespace A { int f(int x); } namespace B { using A::f; }',
     'namespace A { int f(int x) { return x + 1; } }',
     'return B.f(3) == 4 ? 0 : 1;'),
    ('ns-extern-c', 'namespace A { extern "C" int t10_cf(int x); }',
     'namespace A { extern "C" int t10_cf(int x) { return x + 7; } }',
     'return A.t10_cf(1) == 8 ? 0 : 1;'),
    ('ns-global-var', 'namespace A { extern int counter; }',
     'namespace A { int counter = 5; }',
     'A.counter = A.counter + 2; return A.counter == 7 ? 0 : 1;'),
    ('ns-const-var', 'namespace A { extern const int limit; }',
     'namespace A { extern const int limit = 9; }',
     'return A.limit == 9 ? 0 : 1;'),
    ('sobrecarga-tipo', 'namespace A { int g(int x); int g(double x); }',
     'namespace A { int g(int x) { return 1; } int g(double x) { return 2; } }',
     'return A.g(3) == 1 && A.g(2.5) == 2 ? 0 : 1;'),
    ('sobrecarga-aridad', 'namespace A { int h(int x); int h(int x, int y); }',
     'namespace A { int h(int x) { return x; } int h(int x, int y) { return x + y; } }',
     'return A.h(3) == 3 && A.h(3, 4) == 7 ? 0 : 1;'),
    ('sobrecarga-puntero', 'namespace A { int p(int x); int p(int* x); }',
     'namespace A { int p(int x) { return 1; } int p(int* x) { return *x; } }',
     'int v = 5; return A.p(3) == 1 && A.p(&v) == 5 ? 0 : 1;'),
    ('sobrecarga-conversion', 'namespace A { long long w(long long x); }',
     'namespace A { long long w(long long x) { return x * 3; } }',
     'return A.w(4) == 12 ? 0 : 1;'),
    ('sobrecarga-promocion', 'namespace A { int q(int x); int q(long long x); }',
     'namespace A { int q(int x) { return 1; } int q(long long x) { return 2; } }',
     'short s = 3; return A.q(s) == 1 ? 0 : 1;'),
    ('sobrecarga-ambigua', 'namespace A { int m(long x); int m(short x); }',
     'namespace A { int m(long x) { return 1; } int m(short x) { return 2; } }',
     'return A.m(3) == 1 ? 0 : 1;'),
    ('sobrecarga-const-ptr', 'namespace A { int c(const char* s); int c(char* s); }',
     'namespace A { int c(const char* s) { return 1; } int c(char* s) { return 2; } }',
     'sbyte b = 65; return A.c(&b) == 2 ? 0 : 1;'),
    ('defecto', 'namespace A { int d(int a, int b = 5); }',
     'namespace A { int d(int a, int b) { return a + b; } }',
     'return A.d(1) == 6 && A.d(1, 2) == 3 ? 0 : 1;'),
    ('defecto-expr', 'namespace A { constexpr int K = 4; int e(int a, int b = K * 2); }',
     'namespace A { int e(int a, int b) { return a + b; } }',
     'return A.e(1) == 9 ? 0 : 1;'),
    ('estatico-metodo', 'namespace A { struct S { static int add(int a, int b); }; }',
     'namespace A { int S::add(int a, int b) { return a + b; } }',
     'return A.S.add(2, 3) == 5 ? 0 : 1;'),
    ('estatico-sobrecarga', 'namespace A { struct S { static int add(int a, int b); static double add(double a, double b); }; }',
     'namespace A { int S::add(int a, int b) { return a + b; } double S::add(double a, double b) { return a + b + 1.0; } }',
     'return A.S.add(2, 3) == 5 && A.S.add(1.0, 1.0) == 3.0 ? 0 : 1;'),
    ('estatico-campo', 'namespace A { struct S { static int value; }; }',
     'namespace A { int S::value = 3; }',
     'A.S.value = A.S.value * 2; return A.S.value == 6 ? 0 : 1;'),
    ('estatico-campo-const', 'namespace A { struct S { static const int k; }; }',
     'namespace A { const int S::k = 11; }',
     'return A.S.k == 11 ? 0 : 1;'),
    ('estatico-constexpr', 'namespace A { struct S { static constexpr int k = 12; }; int use(); }',
     'namespace A { int use() { return S::k; } }',
     'return A.S.k == 12 ? 0 : 1;'),
    ('estatico-privado', 'namespace A { class S { static int hidden(int a); public: static int open(int a); }; }',
     'namespace A { int S::hidden(int a) { return a; } int S::open(int a) { return hidden(a) + 1; } }',
     'return A.S.hidden(2) == 2 ? 0 : 1;'),
    ('estatico-publico-clase', 'namespace A { class S { static int hidden(int a); public: static int open(int a); }; }',
     'namespace A { int S::hidden(int a) { return a; } int S::open(int a) { return hidden(a) + 1; } }',
     'return A.S.open(2) == 3 ? 0 : 1;'),
    ('estatico-protegido', 'namespace A { class S { protected: static int prot(int a); public: static int open(int a); }; }',
     'namespace A { int S::prot(int a) { return a; } int S::open(int a) { return prot(a) + 1; } }',
     'return A.S.prot(2) == 2 ? 0 : 1;'),
    ('estatico-anidado', 'namespace A { struct S { struct T { static int f(int a); }; }; }',
     'namespace A { int S::T::f(int a) { return a + 4; } }',
     'return A.S.T.f(1) == 5 ? 0 : 1;'),
    ('enum-en-clase', 'namespace A { struct S { enum E { X = 1, Y = 2 }; static int take(E e); }; }',
     'namespace A { int S::take(E e) { return (int)e * 10; } }',
     'return A.S.take(A.S.E.Y) == 20 ? 0 : 1;'),
    ('inline-sin-simbolo', 'namespace A { inline int sq(int x) { return x * x; } int keep(); }',
     'namespace A { int keep() { return 0; } }',
     'return A.sq(3) == 9 ? 0 : 1;'),
    ('referencia', 'namespace A { void inc(int& x); int read(const int& x); }',
     'namespace A { void inc(int& x) { x = x + 1; } int read(const int& x) { return x; } }',
     'int v = 4; A.inc(ref v); return A.read(v) == 5 ? 0 : 1;'),
    ('referencia-sin-ref', 'namespace A { void inc(int& x); }',
     'namespace A { void inc(int& x) { x = x + 1; } }',
     'int v = 4; A.inc(v); return v == 5 ? 0 : 1;'),
    ('referencia-const', 'namespace A { int read(const int& x); }',
     'namespace A { int read(const int& x) { return x; } }',
     'return A.read(6) == 6 ? 0 : 1;'),
    ('referencia-retorno', 'namespace A { int& slot(); }',
     'namespace A { int& slot() { static int v = 3; return v; } }',
     'return A.slot() == 3 ? 0 : 1;'),
    ('ns-anonimo', 'namespace { int hidden(int x) { return x; } } namespace A { int keep(); }',
     'namespace A { int keep() { return hidden(1); } }',
     'return hidden(2) == 2 ? 0 : 1;'),
    ('ambigua-ns', 'namespace A { int z(long long x); int z(double x); }',
     'namespace A { int z(long long x) { return 1; } int z(double x) { return 2; } }',
     'return A.z(3) == 1 ? 0 : 1;'),
    ('ambigua-exacta', 'namespace A { int y(int x, double d); int y(double d, int x); }',
     'namespace A { int y(int x, double d) { return 1; } int y(double d, int x) { return 2; } }',
     'return A.y(1, 2) == 1 ? 0 : 1;'),
    ('sobrecarga-ns-distintos', 'namespace A { int f(int x); } namespace A { int f(double x); }',
     'namespace A { int f(int x) { return 1; } int f(double x) { return 2; } }',
     'return A.f(1) == 1 && A.f(1.5) == 2 ? 0 : 1;'),
    ('enum-clase-scoped', 'namespace A { struct S { enum class E : short { X = 1, Y = 2 }; static int take(E e); }; }',
     'namespace A { int S::take(E e) { return (int)e * 10; } }',
     'return A.S.take(A.S.E.Y) == 20 ? 0 : 1;'),
    ('campo-estatico-arreglo', 'namespace A { struct S { static int table[3]; }; }',
     'namespace A { int S::table[3] = {1, 2, 3}; }',
     'return A.S.table[2] == 3 ? 0 : 1;'),
    ('campo-estatico-record', 'namespace A { struct P { int x; int y; }; struct S { static P origin; }; }',
     'namespace A { P S::origin = {4, 5}; }',
     'return A.S.origin.y == 5 ? 0 : 1;'),
    ('global-record', 'namespace A { struct P { int x; int y; }; extern P origin; }',
     'namespace A { P origin = {4, 5}; }',
     'return A.origin.y == 5 ? 0 : 1;'),
    ('global-thread-local', 'namespace A { extern thread_local int tl; }',
     'namespace A { thread_local int tl = 3; }',
     'return A.tl == 3 ? 0 : 1;'),
    ('operador-miembro', 'namespace A { struct V { int x; V operator+(V o) const; }; }',
     'namespace A { V V::operator+(V o) const { V r; r.x = x + o.x; return r; } }',
     'A.V a; a.x = 2; A.V b; b.x = 3; A.V c = a + b; return c.x == 5 ? 0 : 1;'),
    ('operador-libre', 'namespace A { struct V { int x; }; V operator+(V a, V b); }',
     'namespace A { V operator+(V a, V b) { V r; r.x = a.x + b.x; return r; } }',
     'A.V a; a.x = 2; A.V b; b.x = 3; A.V c = a + b; return c.x == 5 ? 0 : 1;'),
    ('operador-comparacion', 'namespace A { struct V { int x; }; bool operator==(V a, V b); }',
     'namespace A { bool operator==(V a, V b) { return a.x == b.x; } }',
     'A.V a; a.x = 2; A.V b; b.x = 2; return a == b ? 0 : 1;'),
    ('bool-char', 'namespace A { bool even(int x); char letter(int x); }',
     'namespace A { bool even(int x) { return x % 2 == 0; } char letter(int x) { return (char)(65 + x); } }',
     'return A.even(4) && A.letter(1) == 66 ? 0 : 1;'),
    ('noexcept', 'namespace A { int safe(int x) noexcept; }',
     'namespace A { int safe(int x) noexcept { return x - 1; } }',
     'return A.safe(3) == 2 ? 0 : 1;'),
    ('global-cpp', 'int t10_global_f(int x); extern int t10_global_v;',
     'int t10_global_f(int x) { return x + 100; } int t10_global_v = 8;',
     'return t10_global_f(1) == 101 && t10_global_v == 8 ? 0 : 1;'),
]

for name, header, impl, body in CASES:
    if only and name not in only:
        continue
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text('#pragma once\n' + header + '\n', encoding='utf-8')
    (case / 'api.cpp').write_text('#include "api.h"\n' + impl + '\n', encoding='utf-8')
    c, t = ex(case, 'clang', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'])
    if c:
        print(f'{name}: Clang rechaza: {err(t)}', flush=True)
        continue
    ex(case, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    (case / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T10App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
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
