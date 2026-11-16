"""T19-004: campos de una union C++ cuyos miembros son escalares, arreglos o enums.

NativeSharp escribe y lee los miembros de `union A::U { int i; float f; unsigned char b[4]; }`, la pasa por valor
y por puntero, y usa una union dentro de un struct pasado por referencia, como un control C++ de Clang 23.1.0,
O0-O3, en estatica y DLL.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-uniones-', dir='A:/temp'))
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
union U { int i; float f; unsigned char b[4]; };
struct W { int tag; U value; };
API int bits(U u) noexcept;
API float half(U* u) noexcept;
API int unwrap(const W& w) noexcept;
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace A {
int bits(U u) noexcept { return u.i; }
float half(U* u) noexcept { return u->f / 2.0f; }
int unwrap(const W& w) noexcept { return w.tag + w.value.i; }
}
'''
CPP_BODY = ('A::U u; u.i = 7; int r = (int)u.b[0]; u.f = 3.0f; r = r + (int)(A::half(&u) * 10.0f); A::W w; '
            'w.tag = 100; w.value.i = 5; r = r + A::unwrap(w); A::U v; v.i = 1000; r = r + A::bits(v); return r;')
NS_BODY = ('A.U u; u.i = 7; int r = (int)u.b[0]; u.f = 3.0f; r = r + (int)(A.half(&u) * 10.0f); A.W w; '
           'w.tag = 100; w.value.i = 5; r = r + A.unwrap(in w); A.U v; v.i = 1000; r = r + A.bits(v); return r;')


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
        '<Project><PropertyGroup><Name>T19Uniones</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
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
print('OK: union C++ de miembros escalares, frente a Clang; evidencia:', root)
