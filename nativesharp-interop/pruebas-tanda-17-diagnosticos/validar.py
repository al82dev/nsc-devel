"""T17-001 a T17-006: diagnosticos de importacion, uso, sobrecarga, plantilla, entradas ausentes y simbolos.

Cada caso compila una biblioteca C++ con Clang 23.1.0 y un consumidor NativeSharp que la usa mal, y comprueba
el diagnostico de NSC: la declaracion C++ con su ubicacion cuando una importacion falla, el uso NativeSharp con
su linea y columna, una nota por candidato de una sobrecarga (aridad, tipos y ambiguedad), las notas de
instanciacion de Clang con su ubicacion, un encabezado y una biblioteca ausentes con mensajes distintos, y,
tras un enlace fallido, una funcion que ninguna biblioteca define frente a una que la biblioteca define con
otra firma. El control de Clang 23.1.0 confirma lo que dice NSC: el mismo uso no compila o no enlaza en C++, y
la contraparte correcta devuelve lo mismo en C++ y en NativeSharp. Con NSLink y lld-link; con --dll la
biblioteca es una DLL y los simbolos se buscan en su biblioteca de importacion.
"""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t17-diagnosticos-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
env.pop('NSC_LINKER', None)
print('Evidencia:', root, flush=True)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
API = '''#pragma once
#if defined(T17_EXPORT)
#define API __declspec(dllexport)
#elif defined(T17_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
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


def library(case, header, source):
    """Builds api.lib (static, or the import library of api.dll) from a header and its source."""
    case.mkdir()
    (case / 'api.h').write_text(API + header, encoding='utf-8')
    (case / 'api.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n' + source,
                                  encoding='utf-8')
    run(case.name + '-api', [*CXX, *(['-DT17_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T17_IMPORT 1\n' + API + header, encoding='utf-8')
    else:
        run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)


def project(case, body, header='api.h', lib='api.lib'):
    runtime = ' Runtime="api.dll"' if args.dll and lib == 'api.lib' else ''
    (case / 'Main.ns').write_text('struct P { public int x; }\nint Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T17App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="' + header + '" Language="C++" Standard="c++20" Library="' + lib + '"' + runtime +
        ' /></ItemGroup></Project>', encoding='utf-8')


def nsc(case, label, linker='nslink.exe', expected=1):
    return run(case.name + '-' + label, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                         '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                         '-o', 'a.exe'], case, expected=expected,
               extra_env={'NSC_LINKER': linker}).stdout


def expect(name, output, *parts):
    missing = [part for part in parts if part not in output]
    if missing:
        print(output[-3000:], flush=True)
        raise SystemExit(f'{name}: faltan {missing!r} en el diagnostico; evidencia: {root}')


def clang_control(case, label, source, expected_failure):
    """A C++ translation unit with the same use: Clang must reject it (compile or link)."""
    (case / (label + '.cpp')).write_text('#include "api.h"\n' + source, encoding='utf-8')
    result = run(case.name + '-control-' + label, [*CXX, label + '.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                                                   '-Wl,/entry:main', '-o', label + '.exe'], case, expected=None)
    if result.returncode == 0 or expected_failure not in result.stdout:
        print(result.stdout[-2000:], flush=True)
        raise SystemExit(f'{case.name}: el control de Clang no falla con {expected_failure!r}; evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
results = []
# The import header has one more line. / El encabezado de importacion tiene una linea mas.
LINE = (lambda n: str(n + 1)) if args.dll else str

# T17-001 / T17-002: a declaration with no NativeSharp mapping, rejected at the use with its declaration.
case = root / 'importacion'
library(case, 'namespace A { struct S { int f; };\nAPI int take(int S::*m) noexcept; API int one() noexcept; }\n',
        'namespace A { int take(int S::*m) noexcept { return 0; } int one() noexcept { return 1; } }\n')
project(case, 'return A.take(0);')
output = nsc(case, 'nsc')
expect(case.name, output, 'Main.ns:4:12: error:', "C/C++ function 'A::take' cannot be imported",
       "pointer to member", "note: 'A::take' is declared at", 'importacion/api.h:' + LINE(10) + ':')
results.append('importacion: rechazo en Main.ns:4:12 con la declaracion de A::take en api.h')

# T17-002 / T17-003: arity, argument types and ambiguity, with one note per candidate.
case = root / 'sobrecarga'
library(case, 'namespace A {\nAPI int add(int a, int b) noexcept;\nAPI int f(int x) noexcept;\nAPI int f(double x) noexcept;\n'
              'API int f(int* p) noexcept;\nAPI int g(int x) noexcept;\nAPI int g(long x) noexcept;\n}\n',
        'namespace A { int add(int a, int b) noexcept { return a + b; } int f(int x) noexcept { return 1; }\n'
        'int f(double x) noexcept { return 2; } int f(int* p) noexcept { return 3; }\n'
        'int g(int x) noexcept { return 4; } int g(long x) noexcept { return 5; } }\n')
project(case, 'return A.add(1);')
expect(case.name + '-aridad', nsc(case, 'aridad'), 'Main.ns:4:12: error:', "accepts 1 argument(s)",
       'note: candidate: int A::add(int, int) takes 2 argument(s), declared at', 'sobrecarga/api.h:' + LINE(10) + ':')
clang_control(case, 'aridad', 'int main() { return A::add(1); }\n', 'too few arguments')
project(case, 'return A.f("x");')
expect(case.name + '-tipos', nsc(case, 'tipos'), "no viable overload of namespace function 'A.f'",
       "note: candidate: int A::f(int): argument 1 of type 'string literal' does not convert to 'int'",
       "note: candidate: int A::f(double): argument 1 of type 'string literal' does not convert to 'double'",
       "note: candidate: int A::f(int *): argument 1 of type 'string literal' does not convert to 'int*'",
       *('sobrecarga/api.h:' + LINE(n) + ':' for n in (11, 12, 13)))
clang_control(case, 'tipos', 'int main() { return A::f("x"); }\n', 'no matching function')
AMBIGUOUS_BODY = 'return A.g(1);'
project(case, AMBIGUOUS_BODY)
# `int` and `long` are both NativeSharp `int`: the call is ambiguous here, while C++ takes g(int) exactly.
# `int` y `long` son ambos `int` en NativeSharp: aqui la llamada es ambigua; C++ toma g(int) exactamente.
expect(case.name + '-ambigua', nsc(case, 'ambigua'), "ambiguous namespace function call to 'A.g'",
       'note: candidate: int A::g(int), declared at', 'note: candidate: int A::g(long), declared at')
# Counterpart: the calls that resolve. / Contraparte: las llamadas que resuelven.
(case / 'bien.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\nint main() { int v = 7; return A::add(1, 2) + A::f(1) + A::f(2.0) + A::f(&v); }\n',
                               encoding='utf-8')
run(case.name + '-control-bien', [*CXX, 'bien.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib', '-Wl,/entry:main', '-o', 'bien.exe'], case)
control = run(case.name + '-control-bien-run', [str(case / 'bien.exe')], case, expected=None).returncode
project(case, 'int v = 7; return A.add(1, 2) + A.f(1) + A.f(2.0) + A.f(&v);')
nsc(case, 'bien', expected=0)
value = run(case.name + '-bien-run', [str(case / 'a.exe')], case, expected=None).returncode
if value != control or control != 3 + 1 + 2 + 3:
    raise SystemExit(f'sobrecarga: NativeSharp devolvio {value}, Clang {control}; evidencia: {root}')
results.append(f'sobrecarga: aridad, tipos y ambiguedad con una nota por candidato; contraparte {value} igual que Clang')

# T17-004: template instantiation notes with their location.
case = root / 'plantilla'
library(case, 'namespace A { struct S { int v; };\ntemplate <class T> T twice(T x) noexcept { return x * 2; } API int one() noexcept; }\n',
        'namespace A { int one() noexcept { return 1; } }\n')
project(case, 'A.S s = new A.S(); A.S r = A.twice<A.S>(s); return 0;')
expect(case.name, nsc(case, 'nsc'), "Clang cannot instantiate the C++ template 'A::twice'",
       'plantilla/api.h:' + LINE(10) + ':', 'invalid operands to binary expression', 'in instantiation of function template specialization')
clang_control(case, 'uso', 'int main() { A::S s{}; A::S r = A::twice(s); return 0; }\n', 'invalid operands')
results.append('plantilla: notas de Clang con su ubicacion en api.h')

# T17-005: missing header and missing library.
case = root / 'ausentes'
library(case, 'namespace A { API int one() noexcept; }\n', 'namespace A { int one() noexcept { return 1; } }\n')
project(case, 'return A.one();', header='falta.h')
expect(case.name + '-encabezado', nsc(case, 'encabezado'), "CppLibraryReference header does not exist: 'falta.h'")
project(case, 'return A.one();', lib='falta.lib')
output = nsc(case, 'biblioteca')
expect(case.name + '-biblioteca', output, 'referenced library does not exist', 'falta.lib')
if 'header does not exist' in output:
    raise SystemExit(f'ausentes: la biblioteca ausente se informa como encabezado; evidencia: {root}')
results.append('ausentes: encabezado y biblioteca con mensajes distintos')

# T17-005 / T17-006: a missing definition and a definition with another signature, with both linkers.
case = root / 'simbolos'
library(case, 'namespace A { API int one(int x) noexcept; API int two() noexcept; }\n',
        'namespace A { API int one(long long x) noexcept { return 1; } }\n')
for linker in ('nslink.exe', 'lld-link.exe'):
    tag = linker.split('.')[0]
    project(case, 'return A.two();')
    expect(f'{case.name}-ausente-{tag}', nsc(case, 'ausente-' + tag, linker),
           "nsc: error: C/C++ function 'int __cdecl A::two(void)' (symbol '?two@A@@YAHXZ'), declared at",
           'simbolos/api.h:' + LINE(9) + ':', 'is not defined by any linked library')
    project(case, 'return A.one(1);')
    expect(f'{case.name}-firma-{tag}', nsc(case, 'firma-' + tag, linker),
           "nsc: error: C/C++ function 'int __cdecl A::one(int)' (symbol '?one@A@@YAHH@Z')",
           'the header and the library disagree on it',
           "defines 'int __cdecl A::one(__int64)' (symbol '?one@A@@YAH_J@Z')")
clang_control(case, 'ausente', 'int main() { return A::two(); }\n', 'undefined symbol')
clang_control(case, 'firma', 'int main() { return A::one(1); }\n', 'undefined symbol')
results.append('simbolos: ausente frente a firma distinta, con NSLink y lld-link')

for line in results:
    print(line, flush=True)
print('OK: diagnosticos de importacion, uso, sobrecarga, plantilla, ausencias y simbolos; evidencia:', root)
