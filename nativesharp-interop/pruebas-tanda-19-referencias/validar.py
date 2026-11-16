"""T19-007: retornos `const T&` de funciones y metodos C++ como referencias de solo lectura.

`A::pick` y `A::limit` devuelven `const int&`, y `A::V::get` es un metodo const que devuelve `const int&`; junto a
`int& V::slot` (escritura por referencia ya admitida). NativeSharp lee los resultados como un control C++ de Clang
23.1.0, O0-O3, en estatica y DLL. Escribir a traves de un resultado `const T&` se rechaza, igual que Clang.
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-referencias-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
API = '''#pragma once
#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
HEADER = '''namespace A {
API const int& pick(const int& a, const int& b) noexcept;
API const int& limit() noexcept;
struct API V {
    int x[4];
    const int& get(int i) const noexcept;
    int& slot(int i) noexcept;
};
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace A {
static const int four = 4;
const int& pick(const int& a, const int& b) noexcept { return a < b ? b : a; }
const int& limit() noexcept { return four; }
const int& V::get(int i) const noexcept { return x[i]; }
int& V::slot(int i) noexcept { return x[i]; }
}
'''
CPP_BODY = ('int a = 3; int b = 9; int r = A::pick(a, b); A::V v; v.x[1] = 5; r = r + v.get(1) * 10; '
            'v.slot(2) = 7; r = r + v.x[2] * 100; r = r + A::limit() * 1000; return r;')
NS_BODY = ('int a = 3; int b = 9; int r = A.pick(in a, in b); A.V v; v.x[1] = 5; r = r + v.get(1) * 10; '
           'v.slot(2) = 7; r = r + v.x[2] * 100; r = r + A.limit() * 1000; return r;')


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


def project(case, body):
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'Main.ns').write_text('int Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19Referencias</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')


def nsc(case, opt, expected=0):
    return run(f'{case.name}-nsc-O{opt}', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                           '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                           '-O' + str(opt), '-o', 'app.exe'], case, expected=expected).stdout


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
library = root / 'lib'
library.mkdir()
(library / 'api.h').write_text(API + HEADER, encoding='utf-8')
(library / 'api.cpp').write_text(SOURCE, encoding='utf-8')
run('api', [*CXX, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], library)
if args.dll:
    run('api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll', '/implib:api.lib',
                    'api.obj'], library)
    header = '#define T19_IMPORT 1\n' + API + HEADER
else:
    run('api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
    header = API + HEADER


def prepare(name):
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text(header, encoding='utf-8')
    for item in ('api.lib', *(('api.dll',) if args.dll else ())):
        (case / item).write_bytes((library / item).read_bytes())
    return case


for opt in range(4):
    case = prepare(f'O{opt}')
    (case / 'control.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n'
                                      'int main() { ' + CPP_BODY + ' }\n', encoding='utf-8')
    run(f'O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    project(case, NS_BODY)
    nsc(case, opt)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}', flush=True)
# Writing through a `const T&` result is rejected, by Clang too.
# Escribir a traves de un resultado `const T&` se rechaza, tambien en Clang.
for label, cpp, ns in (
        ('funcion', 'int a = 3; int b = 9; A::pick(a, b) = 1; return a;',
         'int a = 3; int b = 9; A.pick(in a, in b) = 1; return a;'),
        ('metodo', 'A::V v{}; v.get(1) = 2; return 0;', 'A.V v; v.get(1) = 2; return 0;')):
    case = prepare('rechazo-' + label)
    (case / 'control.cpp').write_text('#include "api.h"\nint main() { ' + cpp + ' }\n', encoding='utf-8')
    clang = run(case.name + '-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], case, expected=1).stdout
    if 'cannot assign to return value' not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(f'{case.name}: Clang no rechaza la escritura; evidencia: {root}')
    project(case, ns)
    output = nsc(case, 0, expected=1)
    if 'error:' not in output or 'assign' not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{case.name}: falta el rechazo de la escritura; evidencia: {root}')
    print(f'{case.name}: escribir a traves de const T& se rechaza, igual que Clang', flush=True)
print('OK: retornos const T& de funciones y metodos C++, frente a Clang; evidencia:', root)
