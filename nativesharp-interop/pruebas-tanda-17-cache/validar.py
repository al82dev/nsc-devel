"""T17-009 a T17-011: reutilizacion de la compilacion de un proyecto con una biblioteca C++.

Con la cache de artefactos, la compilacion de un proyecto con `CppLibraryReference` se reutiliza solo si
coinciden las opciones de la sesion de Clang, la huella del preprocesado y el contenido de cada archivo que
cargo Clang, ademas de las fuentes NativeSharp, las bibliotecas y las opciones de NSC. Cada paso cambia una
entrada (Define, encabezado transitivo, Standard, IncludeDirectory, la biblioteca, la fuente NativeSharp) y
debe recompilar y devolver lo que devuelve el control de Clang 23.1.0 con las mismas entradas; repetir una
configuracion ya vista debe reutilizarla (`compilation-bundle-cache: hit`) y devolver lo mismo. En estatica,
mide ademas un encabezado con la STL: sin cache, la primera compilacion con cache y la segunda. Con --dll la
biblioteca es una DLL.
"""
import argparse
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t17-cache-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc']
HEADER = '''#pragma once
#if defined(T17_EXPORT)
#define API __declspec(dllexport)
#elif defined(T17_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
#include "inner.h"
#if __cplusplus >= 202002L
#define T17_STD 2
#else
#define T17_STD 1
#endif
namespace A {
API int lib(int x) noexcept;
inline int k() noexcept { return INNER + K * 10 + T17_STD * 100; }
}
'''


def run(label, command, cwd, expected=0, extra_env=None):
    result = subprocess.run(command, cwd=cwd, env={**env, **(extra_env or {})}, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


case = root / 'proyecto'
cache = root / 'cache'
for folder in (case, case / 'inc1', case / 'inc2', cache):
    folder.mkdir()
(case / 'api.h').write_text(HEADER, encoding='utf-8')
(case / 'inc1' / 'inner.h').write_text('#pragma once\n#define INNER 1\n', encoding='utf-8')
(case / 'inc2' / 'inner.h').write_text('#pragma once\n#define INNER 2\n', encoding='utf-8')


def build_library(value):
    (case / 'api.cpp').write_text('#include "api.h"\nnamespace A { int lib(int x) noexcept { return x + ' + str(value) +
                                  '; } }\n', encoding='utf-8')
    flags = [*CXX, '-std=c++20', '-DK=0', '-Iinc1']
    run(f'lib-{value}', [*flags, *(['-DT17_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'dll-{value}', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                             '/implib:api.lib', 'api.obj'], case)
    else:
        run(f'ar-{value}', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)


def configure(k, standard, include, extra):
    import_define = '<Define Value="T17_IMPORT" />' if args.dll else ''
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'Main.ns').write_text('int Main() { return A.k() + A.lib(0) * 1000' + extra + '; }\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T17Cache</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="' + standard + '" Library="api.lib"' + runtime +
        '><IncludeDirectory Path="' + include + '" /><Define Value="K=' + str(k) + '" />' + import_define +
        '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
    (case / 'control.cpp').write_text('#include "api.h"\nint main() { return A::k() + A::lib(0) * 1000' + extra + '; }\n',
                                      encoding='utf-8')
    defines = ['-DK=' + str(k)] + (['-DT17_IMPORT'] if args.dll else [])
    run('control-obj', [*CXX, '-std=' + standard, *defines, '-I' + include, '-c', 'control.cpp', '-o', 'control.obj'], case)
    run('control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                         '/out:control.exe', 'control.obj', 'api.lib'], case)
    return run('control-run', [str(case / 'control.exe')], case, expected=None).returncode


def build(step):
    start = time.perf_counter()
    output = run(step + '-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                 '--artifact-cache=' + str(cache), '--verbose', '-O0', '-o', 'app.exe'], case,
                 extra_env={'NSC_LINKER': 'nslink.exe'}).stdout
    took = time.perf_counter() - start
    reasons = [line.split(':', 1)[1].strip().split()[0] for line in output.splitlines()
               if line.startswith('compilation-bundle-cache:')]
    if len(reasons) != 1:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{step}: no se informa la cache de compilacion; evidencia: {root}')
    return run(step + '-run', [str(case / 'app.exe')], case, expected=None).returncode, reasons[0], took


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
build_library(3)
# (step, K, Standard, IncludeDirectory, NativeSharp extra, rebuild the library with, expected hit)
STEPS = [('base', 1, 'c++20', 'inc1', '', None, False),
         ('repetida', 1, 'c++20', 'inc1', '', None, True),
         ('define', 2, 'c++20', 'inc1', '', None, False),
         ('define-vuelve', 1, 'c++20', 'inc1', '', None, True),
         ('encabezado-transitivo', 1, 'c++20', 'inc1', '', None, False),
         ('standard', 1, 'c++17', 'inc1', '', None, False),
         ('include-directory', 1, 'c++20', 'inc2', '', None, False),
         ('biblioteca', 1, 'c++20', 'inc2', '', 4, False),
         ('fuente', 1, 'c++20', 'inc2', ' + 7', None, False),
         ('fuente-repetida', 1, 'c++20', 'inc2', ' + 7', None, True)]
for step, k, standard, include, extra, library, hit in STEPS:
    if step == 'encabezado-transitivo':
        (case / 'inc1' / 'inner.h').write_text('#pragma once\n#define INNER 5\n', encoding='utf-8')
    if library is not None:
        build_library(library)
    expected = configure(k, standard, include, extra)
    result, reason, took = build(step)
    if result != expected or (reason == 'hit') != hit:
        raise SystemExit(f'{step}: NativeSharp devolvio {result} ({reason}), Clang {expected}, se esperaba '
                         f'{"hit" if hit else "sin hit"}; evidencia: {root}')
    print(f'{step}: NativeSharp y Clang devuelven {result}; cache {reason} ({took:.2f} s)', flush=True)

if not args.dll:
    # Cost of a header with the STL. / Costo de un encabezado con la STL.
    measured = root / 'medicion'
    measured.mkdir()
    (measured / 'api.h').write_text('#pragma once\n#include <vector>\n#include <string>\n#include <map>\n#include <algorithm>\n'
                                    'namespace A { int count(int n) noexcept; }\n', encoding='utf-8')
    (measured / 'api.cpp').write_text('#include "api.h"\nnamespace A { int count(int n) noexcept { std::vector<int> v(n); '
                                      'return (int)v.size(); } }\n', encoding='utf-8')
    includes = ['-isystem', str(TOOLSET / 'include'), '-isystem', str(SDK / 'include/ucrt')]
    run('stl-api', [*CXX, '-std=c++20', *includes, '-c', 'api.cpp', '-o', 'api.obj'], measured)
    run('stl-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], measured)
    (measured / 'Main.ns').write_text('int Main() { return A.count(4); }\n', encoding='utf-8')
    system = ''.join('<IncludeDirectory Path="' + str(path).replace('\\', '/') + '" Kind="System" />'
                     for path in (TOOLSET / 'include', SDK / 'include/ucrt'))
    (measured / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T17Stl</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib">' + system +
        '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
    crt = ['--crt-startup'] + [item for path in CRT for item in ('--link-lib', str(path))]
    timings = []
    for label, options in (('sin-cache', ['--no-artifact-cache']), ('cache-fria', ['--artifact-cache=' + str(root / 'stl-cache')]),
                           ('cache-caliente', ['--artifact-cache=' + str(root / 'stl-cache')])):
        start = time.perf_counter()
        output = run('stl-' + label, [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                      *options, *crt, '--verbose', '-O0', '-o', 'app.exe'], measured,
                     extra_env={'NSC_LINKER': 'lld-link.exe'}).stdout
        took = time.perf_counter() - start
        if run('stl-' + label + '-run', [str(measured / 'app.exe')], measured, expected=None).returncode != 4:
            raise SystemExit(f'medicion {label}: NativeSharp no devuelve 4; evidencia: {root}')
        reason = next((line.split(':', 1)[1].strip().split()[0] for line in output.splitlines()
                       if line.startswith('compilation-bundle-cache:')), '?')
        timings.append((label, reason, took))
    if timings[2][1] != 'hit' or timings[2][2] >= timings[0][2] / 2:
        raise SystemExit(f'medicion: la segunda compilacion con cache no se reutiliza: {timings}; evidencia: {root}')
    print('medicion STL: ' + ', '.join(f'{label} {reason} {took:.2f} s' for label, reason, took in timings), flush=True)
print('OK: la compilacion con una biblioteca C++ se reutiliza solo con las mismas entradas, frente a Clang; evidencia:', root)
