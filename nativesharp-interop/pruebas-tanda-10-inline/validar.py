"""T10-012 y T10-006: funciones inline y variables inline C++ sin simbolo en la biblioteca.

Una funcion inline (tambien constexpr y un metodo estatico definido en la clase) y una variable
inline (tambien un miembro estatico constexpr) se definen en cada unidad de traduccion que las usa,
asi que la biblioteca no las exporta. NSC pide a Clang sus definiciones y las emite en un objeto
generado con enlace weak_odr. Compara un control C++ compilado por Clang 23.1.0 con el consumidor
NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL, directo y desde plantilla precompilada
con y sin encabezado (la plantilla solo llama a las funciones inline), y exige los tipos fisicos de Clang en el IR de NSC. La variable inline
`counter` la usan tambien la biblioteca y el programa: en biblioteca estatica ambas copias COMDAT se
fusionan en una sola, como en el control C++. El control negativo comprueba que una funcion inline
que solo declara el encabezado, sin definicion, sigue sin simbolo y falla al enlazar.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-inline-', dir='A:/temp'))
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
API int base();
inline int sq(int x) { return x * x; }
constexpr int cube(int x) { return x * x * x; }
inline int mixed(int x) { return sq(x) + base(); }
inline int counter = 3;
API int bump();
struct S {
    static int add(int a, int b) { return a + b + base(); }
    static constexpr int k = 12;
};
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T10 {
int base() { return 100; }
int bump() { counter = counter + sq(2); return counter; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    T10::counter = 10;
    int seen = T10::bump();
    return T10::sq(3) + T10::cube(2) + T10::mixed(1) - 100 + T10::S::add(1, 2) - 100 + T10::S::k +
           (seen == 14 ? 1 : 0) + T10::counter / 2;
}
'''
NS_BODY = ('T10.counter = 10; int seen = T10.bump(); '
           'return T10.sq(3) + T10.cube(2) + T10.mixed(1) - 100 + T10.S.add(1, 2) - 100 + T10.S.k + '
           '(seen == 14 ? 1 : 0) + T10.counter / 2;')
# C++ globals and static data members are not yet reconstructed in a precompiled template body, so the
# template only calls the inline functions.
NS_TEMPLATE_BODY = 'return T10.sq(3) + T10.cube(2) + T10.mixed(1) - 100 + T10.S.add(1, 2) - 100;'
TEMPLATE_EXPECTED = 9 + 8 + 1 + 3
EXPECTED = None
DECLARATIONS = None


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
    global EXPECTED
    control = run(case.name + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    # Static: one merged counter, bumped to 14. DLL: the DLL bumps its own copy (7) and the program keeps 10.
    expected = 9 + 8 + 1 + 3 + 12 + ((1 + 7) if not args.dll else (0 + 5))
    if control != expected:
        raise SystemExit(f'{case.name}: el control C++ devolvio {control}, se esperaba {expected}; evidencia: {root}')
    EXPECTED = expected
    return declarations(control_ir)


def compare(tag, clang, ir):
    nsc = declarations(ir)
    for symbol, shape in clang.items():
        if symbol not in nsc and '?base@' in symbol:
            continue  # called only from inline bodies, which Clang emits; NativeSharp never names it
        if symbol not in nsc:
            raise SystemExit(f'{tag}: falta la declaracion {symbol} en el IR de NSC; evidencia: {root}')
        if nsc[symbol] != shape:
            raise SystemExit(f'{tag}: {symbol} tiene {nsc[symbol]} en NSC y {shape} en Clang; evidencia: {root}')


def build_and_run(tag, folder, opt, expected=None):
    expected = EXPECTED if expected is None else expected
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
                                          + NS_TEMPLATE_BODY + ' } } }\n', encoding='utf-8')
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
        compare(f'precompilado-O{opt}-{visibility}', {k: v for k, v in clang.items() if '?bump@' not in k}, ir)
        if args.dll:
            shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
        build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, TEMPLATE_EXPECTED)
    print(f'precompilado-O{opt}: plantilla precompilada con y sin encabezado coincide con Clang ({TEMPLATE_EXPECTED})', flush=True)

negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T10 { inline int ghost(int x); int keep(); }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T10 { int keep() { return 0; } }\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
folder = negative / 'sin-definicion'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { return T10.keep() + T10.ghost(1); }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T10Neg', plain, 'Main.ns'), encoding='utf-8')
output = run('negativo-sin-definicion', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                         '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                         '-o', 'app.exe'], folder, expected=1).stdout
if 'unresolved external symbol' not in output or '?ghost@T10@@' not in output:
    raise SystemExit(f'negativo sin-definicion: falta el diagnostico esperado; evidencia: {root}')
print('negativo sin-definicion: la funcion inline sin definicion sigue sin simbolo', flush=True)
print('OK: funciones y variables inline C++ emitidas por Clang, frente a Clang; evidencia:', root)
