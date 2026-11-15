"""Sonda de auditoria T13 (no es una prueba de soporte): plantillas C++, sus argumentos e instanciaciones, constexpr, consumidos desde NativeSharp.

Cada caso compila una biblioteca C++ con Clang 23.1.0 y un consumidor NativeSharp que devuelve 0 si
los valores coinciden. Imprime: compila y ejecuta (codigo), o el primer diagnostico de NSC.
"""
import os, subprocess, sys, tempfile
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t13-sonda-', dir='A:/temp'))
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
    ('alias-especializacion',
     'namespace A { template <class T> struct Box { T v; T get() const noexcept { return v; } }; using IntBox = Box<int>; IntBox make(int x) noexcept; }',
     'namespace A { IntBox make(int x) noexcept { IntBox b; b.v = x; return b; } }',
     'A.IntBox b = A.make(3); return b.v + b.get() == 6 ? 0 : 1;'),
    ('nombre-generico',
     'namespace A { template <class T> struct Box { T v; }; Box<int> make(int x) noexcept; }',
     'namespace A { Box<int> make(int x) noexcept { Box<int> b; b.v = x; return b; } }',
     'A.Box<int> b = A.make(3); return b.v == 3 ? 0 : 1;'),
    ('funcion-plantilla-explicita',
     'namespace A { template <class T> T twice(T x) noexcept { return x * 2; } }',
     '',
     'return A.twice<int>(3) == 6 ? 0 : 1;'),
    ('funcion-plantilla-deducida',
     'namespace A { template <class T> T twice(T x) noexcept { return x * 2; } }',
     '',
     'return A.twice(3) == 6 ? 0 : 1;'),
    ('instanciacion-explicita-biblioteca',
     'namespace A { template <class T> struct Acc { T total; T add(T x) noexcept; }; extern template struct Acc<long long>; using LAcc = Acc<long long>; }',
     'namespace A { template <class T> T Acc<T>::add(T x) noexcept { total += x; return total; } template struct Acc<long long>; }',
     'A.LAcc a; a.total = 1; return a.add(2) == 3 ? 0 : 1;'),
    ('parametro-no-tipo',
     'namespace A { template <int N> struct Arr { int a[N]; int size() const noexcept { return N; } }; using Arr4 = Arr<4>; }',
     '',
     'A.Arr4 x; return x.size() == 4 ? 0 : 1;'),
    ('especializacion-parcial',
     'namespace A { template <class T> struct Tag { static constexpr int k = 1; }; template <class T> struct Tag<T*> { static constexpr int k = 2; }; inline int pick() noexcept { return Tag<int*>::k; } }',
     '',
     'return A.pick() == 2 ? 0 : 1;'),
    ('concepto',
     'namespace A { template <class T> concept Small = sizeof(T) <= 4; template <Small T> T inc(T x) noexcept { return x + 1; } }',
     '',
     'return A.inc<int>(1) == 2 ? 0 : 1;'),
    ('constexpr-funcion',
     'namespace A { constexpr int sq(int x) noexcept { return x * x; } }',
     '',
     'return A.sq(5) == 25 ? 0 : 1;'),
    ('constexpr-variable',
     'namespace A { constexpr int K = 7; inline constexpr long long L = 1LL << 40; }',
     '',
     'return A.K == 7 && A.L == 1099511627776 ? 0 : 1;'),
    ('plantilla-con-struct-ns',
     'namespace A { template <class T> struct Box { T v; }; }',
     '',
     'A.Box<P> b; b.v.x = 1; return b.v.x - 1;'),
    ('paquete-parametros',
     'namespace A { template <class... T> int count(T...) noexcept { return sizeof...(T); } }',
     '',
     'return A.count<int, int>(1, 2) == 2 ? 0 : 1;'),
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
    (case / 'Main.ns').write_text('struct P { public int x; }\nint Main() { ' + body + ' }\n', encoding='utf-8')
    (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T13App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
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
