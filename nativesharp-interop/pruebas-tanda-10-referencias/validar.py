"""T10-003: funciones libres C++ con parametros y retornos por referencia lvalue.

Una referencia `T&` se proyecta como parametro `ref T`, `const T&` como parametro `in T` y un retorno
`T&` como retorno `ref T`; en Windows x64 Clang las pasa como la direccion del referente. Para
escalares, `double` y un record trivial, compara un control C++ compilado por Clang 23.1.0 con el
consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL. Exige que cada declaracion
del IR de NSC tenga los mismos tipos fisicos de parametros y retorno que la de Clang. Ademas consume
una plantilla precompilada de una biblioteca NativeSharp, con y sin encabezado. La sobrecarga entre
`o(int)` y `o(int&)`, ambigua en C++ con un argumento lvalue, se resuelve en NativeSharp por el
modificador `ref`. Los controles negativos comprueban que un argumento sin `ref` no se enlaza a `T&`
y que una referencia rvalue y un retorno `const T&` no se proyectan.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-referencias-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T10_EXPORT)
#define API __declspec(dllexport)
#elif defined(T10_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T10 {
struct P { int x; int y; };
API void inc(int& x);
API int read(const int& x);
API int& slot();
API int& pick(int& a);
API void grow(P& p);
API int sum(const P& p);
API void twice(double& d);
API int o(int x);
API int o(int& x);
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T10 {
void inc(int& x) { x = x + 1; }
int read(const int& x) { return x; }
int& slot() { static int v = 3; return v; }
int& pick(int& a) { return a; }
void grow(P& p) { p.x += 1; p.y += 1; }
int sum(const P& p) { return p.x + p.y; }
void twice(double& d) { d = d * 2.0; }
int o(int x) { return 1; }
int o(int& x) { return 2; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    int v = 4; T10::inc(v); int r1 = T10::read(v);
    T10::slot() = 9; int r2 = T10::slot();
    int w = 2; T10::pick(w) = 7;
    T10::P p{1, 2}; T10::grow(p); int r3 = T10::sum(p);
    double d = 1.5; T10::twice(d);
    int q = 1; int (*by_value)(int) = T10::o; int (*by_ref)(int&) = T10::o;
    return r1 + r2 + w + r3 + (int)d + by_value(q) * 10 + by_ref(q) * 100;
}
'''
NS_BODY = ('int v = 4; T10.inc(ref v); int r1 = T10.read(in v); '
           'T10.slot() = 9; int r2 = T10.slot(); '
           'int w = 2; T10.pick(ref w) = 7; '
           'T10.P p; p.x = 1; p.y = 2; T10.grow(ref p); int r3 = T10.sum(in p); '
           'double d = 1.5; T10.twice(ref d); '
           'int q = 1; return r1 + r2 + w + r3 + (int)d + T10.o(q) * 10 + T10.o(ref q) * 100;')
EXPECTED = 5 + 9 + 7 + 5 + 3 + 10 + 200
SYMBOLS = ['inc', 'read', 'slot', 'pick', 'grow', 'sum', 'twice', 'o']


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
    """Physical parameter and return types of every T10 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T10@@[^"(]*)"?\((.*)\)', line)
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
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT10_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T10_IMPORT 1\n' + HEADER, encoding='utf-8')
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
    if len(clang) != 9:
        raise SystemExit(f'{case.name}: se esperaban 9 declaraciones de Clang, hay {len(clang)}; evidencia: {root}')
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
    (app / 'App.nsproj').write_text(project('T10App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++, IR de Clang y NativeSharp coinciden ({len(clang)} declaraciones)', flush=True)

    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text('namespace T10N { public class Bridge { public static int Run<T>(T unused) { '
                                          + NS_BODY + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T10Producer', reference('../'), 'Producer.ns', True),
                                              encoding='utf-8')
    run(f'precompilado-O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
        '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
    for visibility in ('con-encabezado', 'sin-encabezado'):
        consumer = case / visibility
        consumer.mkdir()
        references = '<LibraryReference Include="../producer/Producer.lib" />'
        if visibility == 'con-encabezado':
            references += reference('../')
        elif (case / 'api.h').exists():
            (case / 'api.h').rename(case / 'api.h.oculto')
        (consumer / 'Main.ns').write_text('int Main() { return T10N.Bridge.Run<int>(0); }\n', encoding='utf-8')
        (consumer / 'App.nsproj').write_text(project('T10Consumer', references, 'Main.ns'), encoding='utf-8')
        ir = run(f'precompilado-O{opt}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                          '--cpp-clang-root=' + args.clang_root], consumer).stdout
        compare(f'precompilado-O{opt}-{visibility}', clang, ir)
        if args.dll:
            shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
        build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt)
    print(f'precompilado-O{opt}: plantilla precompilada con y sin encabezado coincide con Clang', flush=True)

negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T10 { void inc(int& x); int take(int&& x); '
                                'const int& view(); int keep(); }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T10 { void inc(int& x) { x = x + 1; } '
                                  'int take(int&& x) { return x; } const int& view() { static int v = 3; return v; } '
                                  'int keep() { return 0; } }\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
for name, body, message in (
        ('sin-ref', 'int v = 4; T10.inc(v); return v;', "no viable overload of namespace function 'T10.inc'"),
        # The name is not projected, so the member lookup after the visible T10.keep() fails.
        ('rvalue', 'return T10.take(3);', ":41: error: unknown identifier 'T10'")):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { int k = T10.keep(); ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T10Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if message not in output:
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado', flush=True)
# T19-007: a `const int&` result is read, as Clang reads it; writing through it is rejected by both.
# T19-007: un resultado `const int&` se lee, como en Clang; escribir a traves de el lo rechazan los dos.
folder = negative / 'retorno-const'
folder.mkdir()
(folder / 'control.cpp').write_text('#include "../api.h"\nint main() { return T10::view() + T10::keep(); }\n',
                                    encoding='utf-8')
run('retorno-const-control', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', 'control.cpp',
                              '../api.lib', '-fuse-ld=lld', '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'],
    folder)
control = run('retorno-const-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode
(folder / 'Main.ns').write_text('int Main() { return T10.view() + T10.keep(); }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T10Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('retorno-const', folder, 0, expected=control)
print(f'retorno-const: T10.view() devuelve {control}, igual que Clang', flush=True)
(folder / 'escribe.cpp').write_text('#include "../api.h"\nint main() { T10::view() = 1; return 0; }\n',
                                    encoding='utf-8')
clang = run('retorno-const-escribe-control', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c',
                                              'escribe.cpp', '-o', 'escribe.obj'], folder, expected=1).stdout
if 'cannot assign to return value' not in clang:
    raise SystemExit(f'retorno-const: Clang no rechaza la escritura; evidencia: {root}')
(folder / 'Main.ns').write_text('int Main() { T10.view() = 1; return 0; }\n', encoding='utf-8')
output = run('negativo-retorno-const-escribe', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                                '-o', 'app.exe'], folder, expected=1).stdout
if 'error:' not in output or 'assign' not in output:
    raise SystemExit(f'negativo retorno-const-escribe: falta el diagnostico; evidencia: {root}')
print('negativo retorno-const-escribe: rechazado, igual que Clang', flush=True)
print('OK: referencias lvalue C++ como ref, in y retorno ref, frente a Clang; evidencia:', root)
