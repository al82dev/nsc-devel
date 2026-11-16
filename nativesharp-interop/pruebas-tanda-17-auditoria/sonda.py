"""Sonda de auditoria T17 (no es una prueba de soporte): diagnosticos, depuracion y cache.

Cada caso compila una biblioteca C++ con Clang 23.1.0 y un consumidor NativeSharp; imprime el codigo de
salida o el diagnostico completo de NSC (recortado), para ver que ubicaciones y notas incluye.
"""
import os, subprocess, sys, tempfile, time
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t17-sonda-', dir='A:/temp'))
env = os.environ.copy(); env['TEMP'] = env['TMP'] = str(root)
env['NSC_LINKER'] = 'nslink.exe'
env['NSC_WINDOWS_RUNTIME_LIBRARY'] = 'A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
TS = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
print('Evidencia:', root, flush=True)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']


def ex(cwd, label, cmd):
    start = time.perf_counter()
    r = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    took = time.perf_counter() - start
    (cwd / (label + '.log')).write_text(repr(cmd) + '\nEXIT ' + str(r.returncode) + '\nSEG ' + f'{took:.2f}' + '\n' + r.stdout,
                                        encoding='utf-8')
    return r.returncode, r.stdout, took


def diag(t):
    lines = [l.rstrip()[:240] for l in t.splitlines() if l.strip() and not l.startswith(('NativeSharp Compiler', 'Copyright', 'Build hash', '['))]
    return '\n    '.join(lines[:12])


def case(name, header, source, body, refs_extra='', lib=True, extra=None, header_name='api.h', library_name='api.lib',
         cflags=(), nscflags=(), run=True):
    if only and name not in only:
        return None
    d = root / name
    d.mkdir()
    (d / 'api.h').write_text('#pragma once\n' + header + '\n', encoding='utf-8')
    (d / 'api.cpp').write_text('#include "api.h"\n' + source + '\n', encoding='utf-8')
    if lib:
        c, t, _ = ex(d, 'clang', [*CXX, *cflags, '-c', 'api.cpp', '-o', 'api.obj'])
        if c:
            print(f'{name}: Clang rechaza:\n    {diag(t)}', flush=True); return None
        ex(d, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    (d / 'Main.ns').write_text('struct P { public int x; }\nint Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (d / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T17App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="' + header_name + '" Language="C++" Standard="c++20" Library="' + library_name + '">' + refs_extra +
        '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
    c, t, took = ex(d, 'nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root,
                               *nscflags, '-O0', '-o', 'a.exe'])
    if c:
        print(f'{name}: NSC rechaza ({took:.1f} s):\n    {diag(t)}', flush=True); return d
    if run:
        code = ex(d, 'run', [str(d / 'a.exe')])[0]
        print(f'{name}: ejecuta {code} ({took:.1f} s)', flush=True)
    return d


NOCACHE = ('--no-artifact-cache',)
case('importacion-fallida', 'namespace A { struct S { int f; }; int take(int S::*m) noexcept; }',
     'namespace A { int take(int S::*m) noexcept { return 0; } }', 'return A.take(0);', nscflags=NOCACHE)
case('uso-invalido', 'namespace A { int add(int a, int b) noexcept; }', 'namespace A { int add(int a, int b) noexcept { return a + b; } }',
     'return A.add(1);', nscflags=NOCACHE)
case('tipo-invalido', 'namespace A { int add(int a, int b) noexcept; }', 'namespace A { int add(int a, int b) noexcept { return a + b; } }',
     'return A.add(1, "x");', nscflags=NOCACHE)
case('sobrecarga', 'namespace A { int f(int x) noexcept; int f(double x) noexcept; int f(int* p) noexcept; }',
     'namespace A { int f(int x) noexcept { return 1; } int f(double x) noexcept { return 2; } int f(int* p) noexcept { return 3; } }',
     'return A.f("x");', nscflags=NOCACHE)
case('sobrecarga-ambigua', 'namespace A { int g(long x) noexcept; int g(short x) noexcept; }',
     'namespace A { int g(long x) noexcept { return 1; } int g(short x) noexcept { return 2; } }',
     'return A.g(1);', nscflags=NOCACHE)
case('plantilla', 'namespace A { struct S { int v; }; template <class T> T twice(T x) noexcept { return x * 2; } }', '',
     'A.S s = new A.S(); A.S r = A.twice<A.S>(s); return 0;', nscflags=NOCACHE)
case('encabezado-ausente', 'namespace A { int one() noexcept; }', 'namespace A { int one() noexcept { return 1; } }',
     'return A.one();', header_name='falta.h', nscflags=NOCACHE)
case('biblioteca-ausente', 'namespace A { int one() noexcept; }', 'namespace A { int one() noexcept { return 1; } }',
     'return A.one();', library_name='falta.lib', nscflags=NOCACHE)
case('simbolo-ausente', 'namespace A { int one() noexcept; int two() noexcept; }', 'namespace A { int one() noexcept { return 1; } }',
     'return A.one() + A.two();', nscflags=NOCACHE)
case('abi-firma', 'namespace A { int one(int x) noexcept; }', 'namespace A { int one(long long x) noexcept { return 1; } }',
     'return A.one(1);', nscflags=NOCACHE)
# Debug information. / Informacion de depuracion.
d = case('depuracion', 'namespace A { int one(int x) noexcept; inline int two(int x) noexcept { return one(x) + 1; } }',
         'namespace A { int one(int x) noexcept { return x * 3; } }', 'int a = A.one(2);\n    return a + A.two(1);',
         cflags=('-g', '-gcodeview'), nscflags=(*NOCACHE, '-g'))
if d is not None and (d / 'a.pdb').exists():
    c, t, _ = ex(d, 'pdb', ['llvm-pdbutil.exe', 'dump', '-files', 'a.pdb'])
    files = sorted({l.strip().split('\\')[-1].split('/')[-1] for l in t.splitlines() if l.strip().lower().endswith(('.ns', '.cpp', '.h'))})
    print('depuracion: archivos en el PDB:', files, flush=True)
    c, t, _ = ex(d, 'pdb-sym', ['llvm-pdbutil.exe', 'dump', '-symbols', 'a.pdb'])
    print('depuracion: simbolos con one/two/Main:', sorted({l.strip()[:90] for l in t.splitlines() if 'S_GPROC32' in l and ('one' in l or 'two' in l or 'Main' in l)})[:8], flush=True)
elif d is not None:
    print('depuracion: sin a.pdb:', sorted(p.name for p in d.iterdir())[:20], flush=True)
# Cache with different C++ options. / Cache con opciones C++ distintas.
if not only or 'cache-opciones' in only:
    d = root / 'cache-opciones'
    d.mkdir()
    (d / 'api.h').write_text('#pragma once\nnamespace A { inline int k() noexcept { return K; } }\n', encoding='utf-8')
    (d / 'api.cpp').write_text('int unused_t17 = 0;\n', encoding='utf-8')
    ex(d, 'clang', [*CXX, '-c', 'api.cpp', '-o', 'api.obj']); ex(d, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    (d / 'Main.ns').write_text('int Main() { return A.k(); }\n', encoding='utf-8')
    results = []
    for value in ('1', '2', '1'):
        (d / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T17Cache</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"><Define Value="K=' + value + '" />'
            '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
        c, t, took = ex(d, 'nsc-K' + value + '-' + str(len(results)), [nsc, '--emit-project-oop-exe', 'App.nsproj',
                                                                        '--cpp-clang-root=' + clang_root, '-O0', '-o', 'a.exe'])
        code = ex(d, 'run-' + str(len(results)), [str(d / 'a.exe')])[0] if c == 0 else 'NSC ' + str(c)
        results.append((value, code, round(took, 2), 'cache' if 'restored' in t.lower() or 'cache hit' in t.lower() else ''))
    print('cache-opciones (K, resultado, s):', results, flush=True)
    # Header change only. / Solo cambia el encabezado.
    (d / 'api.h').write_text('#pragma once\nnamespace A { inline int k() noexcept { return K + 10; } }\n', encoding='utf-8')
    c, t, took = ex(d, 'nsc-header', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root, '-O0', '-o', 'a.exe'])
    print('cache-encabezado:', ex(d, 'run-header', [str(d / 'a.exe')])[0] if c == 0 else 'NSC ' + str(c), f'({took:.2f} s)', flush=True)
# Cost of repeated header analysis. / Costo del analisis repetido de encabezados.
if not only or 'medicion' in only:
    d = root / 'medicion'
    d.mkdir()
    (d / 'api.h').write_text('#pragma once\n#include <vector>\n#include <string>\n#include <map>\n#include <algorithm>\n'
                             'namespace A { int count(int n) noexcept; }\n', encoding='utf-8')
    (d / 'api.cpp').write_text('#include "api.h"\nnamespace A { int count(int n) noexcept { std::vector<int> v(n); return (int)v.size(); } }\n',
                               encoding='utf-8')
    ex(d, 'clang', [*CXX, '-isystem', str(TS / 'include'), '-isystem', str(SDK / 'include/ucrt'), '-c', 'api.cpp', '-o', 'api.obj'])
    ex(d, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    inc = ''.join('<IncludeDirectory Path="' + str(p).replace('\\', '/') + '" Kind="System" />' for p in (TS / 'include', SDK / 'include/ucrt'))
    (d / 'Main.ns').write_text('int Main() { return A.count(4); }\n', encoding='utf-8')
    (d / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T17Cost</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib">' + inc + '</CppLibraryReference>'
        '</ItemGroup></Project>', encoding='utf-8')
    crt = ['--crt-startup']
    for lib in (TS / 'lib/x64/libcmt.lib', TS / 'lib/x64/libvcruntime.lib', TS / 'lib/x64/libcpmt.lib',
                SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib'):
        crt += ['--link-lib', str(lib)]
    env['NSC_LINKER'] = 'lld-link.exe'
    times = []
    for i, flags in enumerate((NOCACHE, NOCACHE, (), ())):
        c, t, took = ex(d, f'nsc-{i}', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root, *flags,
                                        *crt, '--verbose', '-O0', '-o', 'a.exe'])
        phases = {l.split()[1].split('=')[1]: int(l.split()[2].split('=')[1]) // 1000 for l in t.splitlines()
                  if l.startswith('timing phase=') and l.split()[1].split('=')[1] in ('precompiled-metadata', 'names', 'members')}
        run_code = ex(d, f'run-{i}', [str(d / 'a.exe')])[0] if c == 0 else None
        times.append((('sin cache' if flags else 'cache'), c, run_code, round(took, 2), phases))
    env['NSC_LINKER'] = 'nslink.exe'
    print('medicion STL (modo, nsc, exe, s, fases ms):', times, flush=True)
    small = root / 'medicion-base'
    small.mkdir()
    (small / 'api.h').write_text('#pragma once\nnamespace A { int count(int n) noexcept; }\n', encoding='utf-8')
    (small / 'api.cpp').write_text('#include "api.h"\nnamespace A { int count(int n) noexcept { return n; } }\n', encoding='utf-8')
    ex(small, 'clang', [*CXX, '-c', 'api.cpp', '-o', 'api.obj']); ex(small, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    (small / 'Main.ns').write_text('int Main() { return A.count(4); }\n', encoding='utf-8')
    (small / 'App.nsproj').write_text((d / 'App.nsproj').read_text(encoding='utf-8').replace(inc, ''), encoding='utf-8')
    c, t, took = ex(small, 'nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root, *NOCACHE,
                                   '--verbose', '-O0', '-o', 'a.exe'])
    print(f'medicion base (sin STL): nsc {c}, {took:.2f} s', flush=True)
    c, t, took = ex(d, 'clang-sola', [*CXX, '-isystem', str(TS / 'include'), '-isystem', str(SDK / 'include/ucrt'), '-fsyntax-only', 'api.h'])
    print(f'medicion: clang -fsyntax-only del encabezado {took:.2f} s', flush=True)
print('FIN', flush=True)
