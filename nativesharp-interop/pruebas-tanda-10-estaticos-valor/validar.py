"""T10-005 y T10-013: metodos estaticos C++ con records triviales por valor.

Un metodo estatico C++ toma y devuelve records C++ triviales por valor, de 4, 8, 12, 16 y 24 bytes,
con la convencion Win64 de Clang. Compara un control C++ compilado por Clang 23.1.0 con el consumidor
NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL, directo y desde plantilla precompilada
con y sin encabezado, y exige los tipos fisicos de Clang en el IR de NSC. El control negativo
comprueba que un record con constructor de copia propio no se pasa por valor.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-estaticos-valor-', dir='A:/temp'))
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
struct R4 { int a; };
struct R8 { int a; int b; };
struct R12 { int a; int b; int c; };
struct R16 { double a; double b; };
struct R24 { int a[6]; };
struct S {
    API static R4 make4(int v);
    API static int sum8(R8 r);
    API static R12 grow(R12 r, int k);
    API static double dot(R16 a, R16 b);
    API static R24 fill(int v);
    API static int total(R24 r);
};
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T10 {
R4 S::make4(int v) { return {v * 2}; }
int S::sum8(R8 r) { return r.a + r.b; }
R12 S::grow(R12 r, int k) { return {r.a * k, r.b * k, r.c * k}; }
double S::dot(R16 a, R16 b) { return a.a * b.a + a.b * b.b; }
R24 S::fill(int v) { R24 r; for (int i = 0; i < 6; ++i) r.a[i] = v + i; return r; }
int S::total(R24 r) { int t = 0; for (int i = 0; i < 6; ++i) t += r.a[i]; return t; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    using namespace T10;
    R4 a = S::make4(3);
    R8 b{1, 2};
    int r = a.a + S::sum8(b);
    R12 c{1, 2, 3};
    R12 d = S::grow(c, 2);
    r += d.a + d.b + d.c;
    R16 e{1.5, 2.0};
    r += (int)S::dot(e, e);
    R24 f = S::fill(2);
    r += S::total(f);
    return r;
}
'''
NS_BODY = ('T10.R4 a = T10.S.make4(3); T10.R8 b; b.a = 1; b.b = 2; int r = a.a + T10.S.sum8(b); '
           'T10.R12 c; c.a = 1; c.b = 2; c.c = 3; T10.R12 d = T10.S.grow(c, 2); r += d.a + d.b + d.c; '
           'T10.R16 e; e.a = 1.5; e.b = 2.0; r += (int)T10.S.dot(e, e); '
           'T10.R24 f = T10.S.fill(2); r += T10.S.total(f); return r;')
EXPECTED = 6 + 3 + 12 + 6 + 27
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
(negative / 'api.h').write_text('#pragma once\nnamespace T10 { struct C { int a; C(); C(const C& other); }; '
                                'struct T { int a; }; struct S { static int take(C c); static int keep(T t); }; }\n',
                                encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T10 { C::C() : a(1) {} C::C(const C& other) : a(other.a) {} '
                                  'int S::take(C c) { return c.a; } int S::keep(T t) { return t.a; } }\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
folder = negative / 'copia-no-trivial'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T10.C c; return T10.S.take(c); }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T10Neg', plain, 'Main.ns'), encoding='utf-8')
output = run('negativo-copia-no-trivial', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                          '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                          '-o', 'app.exe'], folder, expected=1).stdout
if 'error' not in output:
    raise SystemExit(f'negativo copia-no-trivial: falta el diagnostico; evidencia: {root}')
print('negativo copia-no-trivial: rechazado:', next((l.strip() for l in output.splitlines() if 'error' in l), ''), flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T10.T t; t.a = 4; return T10.S.keep(t) - 4; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T10Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: el record trivial del mismo encabezado pasa por valor', flush=True)
print('OK: metodos estaticos C++ con records triviales por valor, frente a Clang; evidencia:', root)
