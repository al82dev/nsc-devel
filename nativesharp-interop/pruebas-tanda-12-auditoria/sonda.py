"""Sonda de auditoria T12 (no es una prueba de soporte): herencia, despacho y tipos dinamicos C++ consumidos desde NativeSharp.

Cada caso compila una biblioteca C++ con Clang 23.1.0 y un consumidor NativeSharp que devuelve 0 si
los valores coinciden. Imprime: compila y ejecuta (codigo), o el primer diagnostico de NSC.
"""
import os, subprocess, sys, tempfile
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t12-sonda-', dir='A:/temp'))
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
    ('base-campos', 'namespace A { struct B { int x; }; struct D : B { int y; }; int sum(D* d); }',
     'namespace A { int sum(D* d) { return d->x + d->y; } }',
     'A.D d; d.x = 2; d.y = 3; return A.sum(&d) == 5 ? 0 : 1;'),
    ('base-conversion-puntero', 'namespace A { struct B { int x; }; struct D : B { int y; }; int readBase(B* b); }',
     'namespace A { int readBase(B* b) { return b->x; } }',
     'A.D d; d.x = 7; d.y = 1; return A.readBase(&d) == 7 ? 0 : 1;'),
    ('metodo-heredado', 'namespace A { struct B { int x; int getX() const; }; struct D : B { int y; }; }',
     'namespace A { int B::getX() const { return x; } }',
     'A.D d; d.x = 4; d.y = 1; return d.getX() == 4 ? 0 : 1;'),
    ('base-multiple-ajuste', 'namespace A { struct B1 { int a; }; struct B2 { int b; int getB() const; }; struct D : B1, B2 { int c; }; }',
     'namespace A { int B2::getB() const { return b; } }',
     'A.D d; d.a = 1; d.b = 2; d.c = 3; return d.getB() == 2 ? 0 : 1;'),
    ('virtual-llamada', 'namespace A { struct B { virtual int f() const; virtual ~B(); int x; }; struct D : B { int f() const override; }; B* makeD(); void drop(B* b); }',
     'namespace A { int B::f() const { return 1; } B::~B() {} int D::f() const { return 2; } B* makeD() { static D d; return &d; } void drop(B*) {} }',
     'A.B* b = A.makeD(); return b->f() == 2 ? 0 : 1;'),
    ('virtual-propio', 'namespace A { struct B { virtual int f() const noexcept; B() noexcept; virtual ~B() noexcept; int x; }; }',
     'namespace A { int B::f() const noexcept { return 3; } B::B() noexcept : x(1) {} B::~B() noexcept {} }',
     'A.B b = new A.B(); return b.f() == 3 ? 0 : 1;'),
    ('virtual-campo', 'namespace A { struct B { virtual ~B(); int x; }; B* make(); }',
     'namespace A { B::~B() {} B* make() { static B b; b.x = 5; return &b; } }',
     'A.B* b = A.make(); return b->x == 5 ? 0 : 1;'),
    ('destructor-virtual', 'namespace A { extern int log; struct B { virtual ~B(); }; struct D : B { ~D() override; }; B* create(); void destroy2(B* b); }',
     'namespace A { int log = 0; B::~B() { log += 1; } D::~D() { log += 10; } B* create() { return new D(); } void destroy2(B* b) { delete b; } }',
     'A.B* b = A.create(); A.destroy2(b); return A.log == 11 ? 0 : 1;'),
    ('herencia-virtual', 'namespace A { struct V { int v; }; struct L : virtual V { int l; }; struct R : virtual V { int r; }; struct D : L, R { int d; }; int readV(D* d); D* make(); }',
     'namespace A { int readV(D* d) { return d->v; } D* make() { static D d; d.v = 9; return &d; } }',
     'A.D* d = A.make(); return A.readV(d) == 9 ? 0 : 1;'),
    ('rtti-dynamic-cast', 'namespace A { struct B { virtual ~B(); }; struct D : B { int y = 4; }; B* make(); D* asD(B* b); }',
     'namespace A { B::~B() {} B* make() { static D d; return &d; } D* asD(B* b) { return dynamic_cast<D*>(b); } }',
     'A.B* b = A.make(); A.D* d = A.asD(b); return d->y == 4 ? 0 : 1;'),
    ('base-privada', 'namespace A { struct B { int x; }; struct D : private B { int y; }; }',
     '',
     'A.D d; d.y = 1; return d.x;'),
    ('override-no-virtual-derivado', 'namespace A { struct B { virtual int f() const; virtual ~B(); }; struct D : B { int f() const override; int g() const; }; D* make(); }',
     'namespace A { int B::f() const { return 1; } B::~B() {} int D::f() const { return 2; } int D::g() const { return 5; } D* make() { static D d; return &d; } }',
     'A.D* d = A.make(); return d->g() == 5 && d->f() == 2 ? 0 : 1;'),
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
    (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T12App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
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
