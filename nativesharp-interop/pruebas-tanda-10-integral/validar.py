"""T10-017 (con T10-001, T10-008, T10-009 y T10-011): sobrecargas, namespaces y miembros estaticos C++
contra una biblioteca compilada por Clang oficial.

Una sola biblioteca C++20 reune homonimos en namespaces distintos, una funcion extern "C" en un namespace,
un alias de namespace, sobrecargas por tipo, aridad y puntero, conversiones que NativeSharp resuelve con
sus propias reglas, un record con campo y metodos estaticos sobrecargados, un argumento predeterminado y
una referencia. Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3,
en biblioteca estatica y con --dll en DLL, directo y desde plantilla precompilada con y sin encabezado,
y exige que cada declaracion del IR de NSC lleve el simbolo y los tipos fisicos de Clang. Donde C++ es
ambiguo y NativeSharp no (T10-009), el control fija con una conversion explicita la sobrecarga que
NativeSharp elige. Los controles negativos comprueban un empate que tambien es ambiguo en NativeSharp y
una sobrecarga inexistente.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-integral-', dir='A:/temp'))
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
namespace L {
namespace Geo { API int area(int w, int h); API double area(double r); }
namespace Text { API int area(const char* s); }
extern "C" API int l_plain(int x);
API int pick(int x);
API int pick(int x, int y);
API int pick(int* p);
API int pick(const char* s);
API int pick(char* s);
API int widen(long long x);
API int widen(double x);
API int narrow(long x);
API int narrow(short x);
struct Counter {
    API static int total;
    API static int add(int x);
    API static int add(int x, int y);
    API static double add(double x);
};
API int scale(int x, int factor = 3);
API void twice(int& x);
namespace Fast = Geo;
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace L {
namespace Geo { int area(int w, int h) { return w * h; } double area(double r) { return r * 10.0; } }
namespace Text { int area(const char* s) { return s[0]; } }
extern "C" int l_plain(int x) { return x + 1; }
int pick(int x) { return x; }
int pick(int x, int y) { return x + y; }
int pick(int* p) { return *p + 100; }
int pick(const char* s) { return 200; }
int pick(char* s) { return 300; }
int widen(long long x) { return (int)x * 2; }
int widen(double x) { return 1000; }
int narrow(long x) { return (int)x + 10; }
int narrow(short x) { return 2000; }
int Counter::total = 10;
int Counter::add(int x) { return x + total; }
int Counter::add(int x, int y) { return x + y; }
double Counter::add(double x) { return x * 2.0; }
int scale(int x, int factor) { return x * factor; }
void twice(int& x) { x = x * 2; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    int v = 5; char b = 65;
    int r = L::Geo::area(3, 4) + (int)L::Geo::area(2.0) + L::Text::area(&b) + L::l_plain(5);
    r += L::pick(1) + L::pick(1, 2) + L::pick(&v) + L::pick(&b);
    // C++ finds widen(4) and narrow(3) ambiguous; NativeSharp picks long long and long (T10-009).
    r += L::widen((long long)4) + L::narrow((long)3);
    r += L::Counter::add(1) + L::Counter::add(1, 2) + (int)L::Counter::add(1.5);
    r += L::scale(4) + L::scale(4, 2);
    L::twice(v);
    return r + v + L::Fast::area(1, 1);
}
'''
NS_BODY = ('int v = 5; sbyte b = 65; '
           'int r = L.Geo.area(3, 4) + (int)L.Geo.area(2.0) + L.Text.area(&b) + L.l_plain(5); '
           'r += L.pick(1) + L.pick(1, 2) + L.pick(&v) + L.pick(&b); '
           'r += L.widen(4) + L.narrow(3); '
           'r += L.Counter.add(1) + L.Counter.add(1, 2) + (int)L.Counter.add(1.5); '
           'r += L.scale(4) + L.scale(4, 2); '
           'L.twice(ref v); return r + v + L.Fast.area(1, 1);')
EXPECTED = 12 + 20 + 65 + 6 + 1 + 3 + 105 + 300 + 8 + 13 + 11 + 3 + 3 + 12 + 8 + 10 + 1
DECLARATIONS = 14


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
    """Physical parameter and return types of every L declaration, keyed by Clang's mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*L@@[^"(]*)"?\((.*)\)', line)
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
    (producer / 'Producer.ns').write_text('namespace L10N { public class Bridge { public static int Run<T>(T unused) { '
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
        (consumer / 'Main.ns').write_text('int Main() { return L10N.Bridge.Run<int>(0); }\n', encoding='utf-8')
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
(negative / 'api.h').write_text('#pragma once\nnamespace L { int tie(int a, double b); int tie(double a, int b); '
                                'namespace Text { int area(const char* s); } }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace L { int tie(int a, double b) { return 1; } '
                                  'int tie(double a, int b) { return 2; } namespace Text { int area(const char* s) { return 0; } } }\n',
                                  encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
for name, body, message in (
        ('empate', 'return L.tie(1, 1);', 'ambiguous'),
        ('sin-sobrecarga', 'return L.Text.area(3);', "no viable overload of namespace function 'L.Text.area'")):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('L10Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado', flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { return L.tie(1, 2.0) + L.tie(2.0, 1) - 3; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('L10Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: cada sobrecarga exacta de tie se elige', flush=True)
print('OK: sobrecargas, namespaces y miembros estaticos C++ con una biblioteca de Clang oficial; evidencia:', root)
