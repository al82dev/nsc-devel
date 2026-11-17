"""T10-010: argumentos predeterminados de funciones libres C++ evaluados por Clang.

Clang reduce cada argumento predeterminado a una constante del tipo del parametro (entero con y sin
signo, negativo, bool, double, float, char o puntero nulo) y NativeSharp la materializa en la
llamada como cualquier valor predeterminado. Compara un control C++ compilado por Clang 23.1.0, que
llama omitiendo esos argumentos, con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con
--dll en DLL, directo y desde una plantilla precompilada con y sin encabezado; exige ademas que las
declaraciones del IR de NSC tengan los tipos fisicos de Clang. Un predeterminado que Clang no reduce a
una constante (una llamada o un enumerador) lo evalua Clang en cada llamada (T19-006): cada uso se
compara con un control de Clang, y una llamada con mas argumentos que parametros se rechaza, como en
Clang.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-predeterminados-', dir='A:/temp'))
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
constexpr int K = 4;
API int d(int a, int b = 5);
API int e(int a, int b = K * 2);
API int n(int a = -3);
API double h(double x = 2.5);
API int f(float x = 0.25f);
API int b(bool flag = true);
API int p(int* q = nullptr);
API int u(unsigned long long v = 18446744073709551615ULL);
API int c(char ch = 'A');
API long long w(long long v = -9223372036854775807LL - 1);
API int s(short x = 7, unsigned char y = 200);
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T10 {
int d(int a, int b) { return a + b; }
int e(int a, int b) { return a + b; }
int n(int a) { return a; }
double h(double x) { return x * 2.0; }
int f(float x) { return x == 0.25f ? 1 : 0; }
int b(bool flag) { return flag ? 1 : 0; }
int p(int* q) { return q ? *q : -1; }
int u(unsigned long long v) { return v == 18446744073709551615ULL ? 1 : 0; }
int c(char ch) { return ch; }
long long w(long long v) { return v; }
int s(short x, unsigned char y) { return x + y; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    int v = 9;
    return T10::d(1) + T10::e(1) + T10::n() + (int)T10::h() + T10::f() + T10::b() + T10::p() + T10::p(&v) +
           T10::u() + T10::c() - 60 + (T10::w() < 0 ? 1 : 0) + T10::s() - 200 + T10::s(1, 2);
}
'''
NS_BODY = ('int v = 9; '
           'return T10.d(1) + T10.e(1) + T10.n() + (int)T10.h() + T10.f() + T10.b() + T10.p() + T10.p(&v) + '
           'T10.u() + T10.c() - 60 + (T10.w() < 0 ? 1 : 0) + T10.s() - 200 + T10.s(1, 2);')
EXPECTED = 6 + 9 - 3 + 5 + 1 + 1 - 1 + 9 + 1 + 65 - 60 + 1 + 207 - 200 + 3
DECLARATIONS = 11


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
(negative / 'api.h').write_text('#pragma once\nnamespace T10 { enum Mode { Low = 1, High = 2 }; int compute(); '
                                'int g(int a = compute()); int m(Mode x = High); int mix(int a = 1, int b = compute()); }\n',
                                encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T10 { int compute() { return 3; } int g(int a) { return a; } '
                                  'int m(Mode x) { return (int)x; } int mix(int a, int b) { return a + b; } }\n',
                                  encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
# EN: T19-006. These defaults were rejected before; Clang now evaluates them at each call, so each use is compared
#     with a Clang control. The genuine rejection (more arguments than parameters) is rejected by Clang too.
# ES: T19-006. Estos predeterminados se rechazaban antes; ahora Clang los evalua en cada llamada, asi que cada uso
#     se compara con un control de Clang. El rechazo genuino (mas argumentos que parametros) tambien lo da Clang.
for name, body, cpp, message in (
        ('llamada', 'return T10.g();', 'return T10::g();', None),
        ('llamada-explicita', 'return T10.g(4);', 'return T10::g(4);', None),
        ('enumerador', 'return T10.m();', 'return T10::m();', None),
        ('anterior', 'return T10.mix(1);', 'return T10::mix(1);', None),
        ('anterior-explicita', 'return T10.mix(1, 2);', 'return T10::mix(1, 2);', None),
        ('demasiados', 'return T10.g(1, 2);', 'return T10::g(1, 2);',
         "no overload of namespace function 'T10.g' accepts 2 argument(s)")):
    folder = negative / name
    folder.mkdir()
    (folder / 'Control.cpp').write_text('#include "../api.h"\nint main() { ' + cpp + ' }\n', encoding='utf-8')
    compiled = run('control-' + name, ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                       'Control.cpp', '../api.lib', '-fuse-ld=lld', '-nostdlib', '-Wl,/entry:main',
                                       '-o', 'control.exe'], folder, expected=0 if message is None else 1)
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T10Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=0 if message is None else 1).stdout
    if message is None:
        control = run('control-' + name + '-run', [str(folder / 'control.exe')], folder, expected=None).returncode
        result = run('negativo-' + name + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
        if result != control:
            raise SystemExit(f'{name}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
        print(f'{name}: NativeSharp y Clang devuelven {control}', flush=True)
        continue
    if message not in output:
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado, tambien por Clang', flush=True)
print('OK: argumentos predeterminados C++ evaluados por Clang, frente a Clang; evidencia:', root)
