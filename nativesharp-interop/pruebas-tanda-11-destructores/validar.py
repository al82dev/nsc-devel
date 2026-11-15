"""T11-007 y T11-013: destructores C++ noexcept implicitos.

En C++ un destructor declarado sin especificacion de excepciones (`~D();`) o por defecto es noexcept
de forma implicita, igual que un destructor implicito. NSC resuelve esa especificacion con Sema y un
local NativeSharp de ese tipo se construye y se destruye al salir de su ambito, en orden inverso.
Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, en
biblioteca estatica y con --dll en DLL, y exige los tipos fisicos de Clang en el IR de NSC. Los objetos
C++ con vida no trivial aun no se transportan en una plantilla precompilada. El control negativo
comprueba que un destructor `noexcept(false)` sigue rechazado.
"""
import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t11-destructores-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T11_EXPORT)
#define API __declspec(dllexport)
#elif defined(T11_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T11 {
API int trace();
struct D { int v; API D(int x) noexcept; API ~D(); };
struct E { int v; API E(int x) noexcept; API ~E() = default; };
struct H { D d; int w; API H(int x) noexcept; };
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T11 {
static int log = 0;
int trace() { return log; }
D::D(int x) noexcept : v(x) {}
D::~D() { log = log * 10 + v; }
E::E(int x) noexcept : v(x) {}
H::H(int x) noexcept : d(x), w(x) {}
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    using namespace T11;
    int r = 0;
    { D a(1); D b(2); r += a.v + b.v; }
    { E e(5); r += e.v; }
    { H h(4); r += h.w; }
    return r + trace();
}
'''
NS_BODY = ('int r = 0; if (true) { T11.D a = new T11.D(1); T11.D b = new T11.D(2); r += a.v + b.v; } '
           'if (true) { T11.E e = new T11.E(5); r += e.v; } '
           'if (true) { T11.H h = new T11.H(4); r += h.w; } return r + T11.trace();')
EXPECTED = 3 + 5 + 4 + 214
DECLARATIONS = 6


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


def reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


def project(name, references, source, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def declarations(ir):
    """Physical parameter and return types of every T11 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T11@@[^"(]*)"?\((.*)\)', line)
        if not match:
            continue
        head, symbol, parameters = match.groups()
        result = [w for w in head.split() if w not in ('dso_local', 'dllimport', 'noundef', 'nonnull', 'zeroext',
                                                       'signext') and not w.startswith(('dereferenceable', 'align'))]
        shapes = []
        for parameter in parameters.split(', ') if parameters.strip() else []:
            words = parameter.split()
            shapes.append(words[0])
        found[symbol] = (result[-1] if result else '', tuple(shapes))
    return found


def build_library(case, opt):
    case.mkdir(parents=True)
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT11_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T11_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    control_ir = run(case.name + '-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                 '-O0', '-S', '-emit-llvm', 'Control.cpp', '-o', '-'], case).stdout
    run(case.name + '-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(case.name + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                      '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    if args.dll:
        pass
    control = run(case.name + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'{case.name}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    clang = declarations(control_ir)
    if len(clang) != DECLARATIONS:
        raise SystemExit(f'{case.name}: se esperaban {DECLARATIONS} declaraciones de Clang, hay {len(clang)}; evidencia: {root}')
    return clang


def compare(tag, clang, ir):
    nsc = declarations(ir)
    for symbol, shape in clang.items():
        if symbol not in nsc:
            raise SystemExit(f'{tag}: falta la declaracion {symbol} en el IR de NSC; evidencia: {root}')
        if nsc[symbol] != shape:
            raise SystemExit(f'{tag}: {symbol} tiene {nsc[symbol]} en NSC y {shape} en Clang; evidencia: {root}')


def build_and_run(tag, folder, opt, expected=EXPECTED):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'directo-O{opt}'
    clang = build_library(case, opt)
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T11App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++, IR de Clang y NativeSharp coinciden ({len(clang)} declaraciones)', flush=True)


negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T11 { struct L { int v; L(int x) noexcept; ~L() noexcept(false); }; '
                                'struct K { int v; K(int x) noexcept; ~K(); }; }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T11 { L::L(int x) noexcept : v(x) {} L::~L() noexcept(false) {} '
                                  'K::K(int x) noexcept : v(x) {} K::~K() {} }\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
folder = negative / 'destructor-lanzador'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T11.L l = new T11.L(1); return l.v; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T11Neg', plain, 'Main.ns'), encoding='utf-8')
output = run('negativo-destructor-lanzador', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                             '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                             '-o', 'app.exe'], folder, expected=1).stdout
if 'non-throwing' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo destructor-lanzador: falta el diagnostico; evidencia: {root}')
print('negativo destructor-lanzador: rechazado:', next((l.strip() for l in output.splitlines() if 'error' in l), ''), flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T11.K k = new T11.K(1); return k.v - 1; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T11Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: el destructor implicitamente noexcept del mismo encabezado se acepta', flush=True)
print('OK: destructores C++ noexcept implicitos, frente a Clang; evidencia:', root)
