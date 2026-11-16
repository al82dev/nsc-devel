"""T19-005: tipos C++ nombrados a traves de un alias de namespace.

Con `namespace A = B;`, NativeSharp nombra `A.P` (struct), `A.E` (enum class, como tipo), `A.Count` (typedef) y `A.N.Q`
(struct de un namespace anidado) en locales, `new`, `var` y el parametro de una funcion NativeSharp, y llama
funciones de B por el alias, como un control C++ de Clang 23.1.0, O0-O3, en estatica y DLL. Un nombre que B no
declara se rechaza, igual que Clang.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-alias-', dir='A:/temp'))
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
HEADER = '''namespace B {
struct P { int x; };
enum class E : int { a = 2, b = 3 };
typedef int Count;
namespace N { struct Q { int y; }; }
API int get(P p) noexcept;
API int depth(N::Q q) noexcept;
}
namespace A = B;
'''
SOURCE = '''#include "api.h"
namespace B {
int get(P p) noexcept { return p.x; }
int depth(N::Q q) noexcept { return q.y; }
}
'''
CPP_BODY = ('A::P p; p.x = 7; A::N::Q q; q.y = 4; A::Count c = 5; A::P p2; p2.x = 1; A::E e = B::E::b; '
            'return A::get(p) + A::depth(q) * 10 + c * 100 + (int)e * 1000 + F(p2) * 10000;')
NS_BODY = ('A.P p; p.x = 7; A.N.Q q; q.y = 4; A.Count c = 5; var p2 = new A.P(); p2.x = 1; A.E e = B.E.b; '
           'return A.get(p) + A.depth(q) * 10 + c * 100 + (int)e * 1000 + F(p2) * 10000;')
CPP_PRELUDE = 'int F(A::P v) { return v.x; }\n'
NS_PRELUDE = 'int F(A.P v) { return v.x; }\n'


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
    (case / 'Main.ns').write_text(NS_PRELUDE + 'int Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19Alias</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
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
    (case / 'control.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n' +
                                      CPP_PRELUDE + 'int main() { ' + CPP_BODY + ' }\n',
                                      encoding='utf-8')
    run(f'O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    project(case, NS_BODY)
    nsc(case, opt)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}', flush=True)
# A name the target namespace does not declare is rejected, by Clang too.
# Un nombre que el namespace destino no declara se rechaza, tambien en Clang.
case = prepare('rechazo')
(case / 'control.cpp').write_text('#include "api.h"\nint main() { A::Z z; return 0; }\n', encoding='utf-8')
clang = run('rechazo-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], case, expected=1).stdout
if "no type named 'Z' in namespace 'B'" not in clang:
    print(clang[-2000:], flush=True)
    raise SystemExit(f'rechazo: Clang no rechaza A::Z; evidencia: {root}')
project(case, 'A.Z z; return 0;')
output = nsc(case, 0, expected=1)
if 'error:' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'rechazo: falta el diagnostico; evidencia: {root}')
print('rechazo: A.Z, que B no declara, se rechaza igual que Clang', flush=True)
print('OK: tipos C++ nombrados por un alias de namespace, frente a Clang; evidencia:', root)
