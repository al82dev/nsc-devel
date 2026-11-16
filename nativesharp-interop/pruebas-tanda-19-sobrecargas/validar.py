"""T19-006: sobrecargas C++ con parametros aritmeticos, ordenadas por la conversion C++ de cada argumento.

`long` mide 32 bits en C++ en Windows y se proyecta como `int` NativeSharp; `long` NativeSharp es `long long`.
NativeSharp elige como Clang 23.1.0 entre `g(int)` y `g(long)` (coincidencia exacta y promocion),
`u(unsigned)` y `u(unsigned long)`, `w(long)` y `w(long long)`, `d(float)` y `d(double)` y `p(int)` y
`p(double)`, O0-O3, en estatica y DLL. Rechazos genuinos, iguales a Clang: `h(long)` frente a `h(long long)`
con un `int`, y `m(int, long)` frente a `m(long, int)` con dos `int`, son ambiguas.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-sobrecargas-', dir='A:/temp'))
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
API int g(int x) noexcept;
API int g(long x) noexcept;
API int u(unsigned x) noexcept;
API int u(unsigned long x) noexcept;
API int w(long x) noexcept;
API int w(long long x) noexcept;
API int d(float x) noexcept;
API int d(double x) noexcept;
API int p(int x) noexcept;
API int p(double x) noexcept;
API int h(long x) noexcept;
API int h(long long x) noexcept;
API int m(int a, long b) noexcept;
API int m(long a, int b) noexcept;
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace A {
int g(int x) noexcept { return 1; }
int g(long x) noexcept { return 2; }
int u(unsigned x) noexcept { return 1; }
int u(unsigned long x) noexcept { return 2; }
int w(long x) noexcept { return 1; }
int w(long long x) noexcept { return 2; }
int d(float x) noexcept { return 1; }
int d(double x) noexcept { return 2; }
int p(int x) noexcept { return 1; }
int p(double x) noexcept { return 2; }
int h(long x) noexcept { return 1; }
int h(long long x) noexcept { return 2; }
int m(int a, long b) noexcept { return 1; }
int m(long a, int b) noexcept { return 2; }
}
'''
# Each call with the C++ and the NativeSharp spelling of its argument.
# Cada llamada con la escritura C++ y NativeSharp de su argumento.
CALLS = [
    ('A::g(5)', 'A.g(5)'),
    ('A::g(s)', 'A.g(s)'),
    ('A::u(ui)', 'A.u(ui)'),
    ('A::w(ll)', 'A.w(ll)'),
    ('A::d(fl)', 'A.d(fl)'),
    ('A::d(db)', 'A.d(db)'),
    ('A::p(s)', 'A.p(s)'),
    ('A::p(fl)', 'A.p(fl)'),
]
CPP_LOCALS = 'short s = 3; unsigned ui = 4; long long ll = 5; float fl = 1.5f; double db = 2.5;'
NS_LOCALS = 'short s = 3; uint ui = 4; long ll = 5; float fl = 1.5f; double db = 2.5;'
CPP_SUM = ' + '.join(f'{call} * {10 ** i}' for i, (call, _) in enumerate(CALLS))
NS_SUM = ' + '.join(f'{call} * {10 ** i}' for i, (_, call) in enumerate(CALLS))


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
        '<Project><PropertyGroup><Name>T19Sobrecargas</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
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
                                      'int main() { ' + CPP_LOCALS + ' return ' + CPP_SUM + '; }\n', encoding='utf-8')
    run(f'O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    project(case, NS_LOCALS + ' return ' + NS_SUM + ';')
    nsc(case, opt)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result} ({len(CALLS)} llamadas sobrecargadas)', flush=True)
# Genuine ambiguities, rejected by Clang too. / Ambiguedades genuinas, que Clang tambien rechaza.
for name, cpp, ns, notes in (
        ('h', 'A::h(5)', 'A.h(5)', ('int A::h(long)', 'int A::h(long long)')),
        ('m', 'A::m(1, 1)', 'A.m(1, 1)', ('int A::m(int, long)', 'int A::m(long, int)'))):
    case = prepare('ambigua-' + name)
    (case / 'control.cpp').write_text('#include "api.h"\nint main() { return ' + cpp + '; }\n', encoding='utf-8')
    clang = run(case.name + '-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], case, expected=1).stdout
    if 'is ambiguous' not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(f'{case.name}: Clang no la rechaza como ambigua; evidencia: {root}')
    project(case, 'return ' + ns + ';')
    output = nsc(case, 0, expected=1)
    missing = [part for part in ("ambiguous namespace function call to 'A." + name + "'",
                                 *('note: candidate: ' + note for note in notes)) if part not in output]
    if missing:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{case.name}: faltan {missing!r}; evidencia: {root}')
    print(f'{case.name}: ambigua, igual que Clang', flush=True)
print('OK: sobrecargas C++ aritmeticas resueltas como Clang; evidencia:', root)
