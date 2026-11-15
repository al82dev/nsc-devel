"""T14-001, T14-002 y T14-003: macros de C y C++ consumidas desde NativeSharp.

El encabezado define macros de objeto con enteros, sin signo, de 64 bits, negativos, expresiones que usan
otras macros, flotantes, bool, un enumerador, un caracter, una llamada a una funcion constexpr y una macro
de un encabezado incluido. Clang evalua cada expansion y NSC importa las que son constantes escalares con el
tipo que deduce Clang. Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp,
O0-O3, en estatica y DLL; un encabezado C con macros se comprueba igual. Una macro de funcion se rechaza con
su motivo; la contraparte llama a una funcion inline.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t14-macros-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T14_EXPORT)
#define API __declspec(dllexport)
#elif defined(T14_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
#include "inner.h"
#define T14_N 4
#define T14_SHIFT 3
#define T14_MASK (1u << T14_SHIFT)
#define T14_BIG (1LL << 40)
#define T14_NEG (-5)
#define T14_PI 3.25
#define T14_HALF 0.5f
#define T14_YES true
#define T14_CH 'A'
#define T14_SUM (T14_N + T14_INNER)
namespace T14 {
enum class Color : int { Red = 1, Green = 2 };
constexpr int sq(int x) noexcept { return x * x; }
API int libTwice(int x) noexcept;
}
#define T14_FAV T14::Color::Green
#define T14_SQ5 (T14::sq(5))
#define T14_SQUARE(x) ((x) * (x))
'''
INNER = '#pragma once\n#define T14_INNER 6\n'
SOURCE = '''#include "api.h"
namespace T14 { int libTwice(int x) noexcept { return x * 2; } }
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    int r = T14_N + (int)(T14_MASK >> 1) + (int)(T14_BIG >> 38) + T14_NEG + (int)(T14_PI * 4.0) + (int)(T14_HALF * 2.0f);
    r += (T14_YES ? 1 : 0) + (int)T14_CH + T14_SUM;
    T14::Color fav = T14_FAV; r += (int)fav;
    r += T14_SQ5 + T14::libTwice(T14_N);
    return r;
}
'''
NS_BODY = ('int r = T14_N + (int)(T14_MASK >> 1) + (int)(T14_BIG >> 38) + T14_NEG + (int)(T14_PI * 4.0) + '
           '(int)(T14_HALF * 2.0f); '
           'r += (T14_YES ? 1 : 0) + (int)T14_CH + T14_SUM; '
           'T14.Color fav = T14_FAV; r += (int)fav; '
           'r += T14_SQ5 + T14.libTwice(T14_N); '
           'return r;')
EXPECTED = 4 + 4 + 4 - 5 + 13 + 1 + 1 + 65 + 10 + 2 + 25 + 8
DIRECT = 1


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
    """Physical parameter and return types of every T14 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T14@@[^"(]*)"?\((.*)\)', line)
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


def build_library_in(case, opt):
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT14_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T14_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    control_ir = run(case.name + '-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                 '-O0', '-S', '-emit-llvm', 'Control.cpp', '-o', '-'], case).stdout
    run(case.name + '-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(case.name + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                      '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    control = run(case.name + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'{case.name}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    return declarations(control_ir)


def compare(tag, clang, ir):
    """Every C++ symbol NSC calls is one Clang's control declares, with the same physical types (an sret
    result is a leading ptr parameter and a void result)."""
    nsc = declarations(ir)
    called = set(re.findall(r'(?:call|invoke) [^@]*@"(\?[^"]*T14@@[^"]*)"\(', ir))
    for symbol in called:
        if symbol not in clang:
            raise SystemExit(f'{tag}: NSC llama a {symbol}, que el control de Clang no declara; evidencia: {root}')
        if clang[symbol] != nsc.get(symbol):
            raise SystemExit(f'{tag}: {symbol} tiene {nsc.get(symbol)} en NSC y {clang[symbol]} en Clang; '
                             f'evidencia: {root}')
    if len(called) != DIRECT:
        raise SystemExit(f'{tag}: se esperaban {DIRECT} llamadas C++ en NSC, hay {len(called)}; evidencia: {root}')
    return len(called)


def build_and_run(tag, folder, opt, expected=EXPECTED):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


def write_common(case):
    (case / 'inner.h').write_text(INNER, encoding='utf-8')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'directo-O{opt}'
    case.mkdir(parents=True)
    write_common(case)
    clang = build_library_in(case, opt)
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T14App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    direct = compare(f'directo-O{opt}', clang, ir)
    if '__nsc_macro_' in ir:
        raise SystemExit(f'directo-O{opt}: NSC nombra un simbolo de macro; evidencia: {root}')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden ({direct} declaraciones iguales a las de Clang)',
          flush=True)
# A C header. / Un encabezado C.
cfolder = root / 'c'
cfolder.mkdir()
(cfolder / 'capi.h').write_text('#pragma once\n#define T14_C 9\n#define T14_CNEG (-3)\n#define T14_CF 1.5\n'
                                'int c_add(int a, int b);\n', encoding='utf-8')
(cfolder / 'capi.c').write_text('#include "capi.h"\nint c_add(int a, int b) { return a + b; }\n', encoding='utf-8')
run('c-obj', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c11', '-c', 'capi.c', '-o', 'capi.obj'], cfolder)
run('c-lib', ['llvm-lib.exe', '/nologo', '/out:capi.lib', 'capi.obj'], cfolder)
(cfolder / 'Main.ns').write_text('int Main() { return c_add(T14_C, T14_CNEG) + (int)(T14_CF * 2.0); }\n', encoding='utf-8')
(cfolder / 'App.nsproj').write_text(project('T14C', '<CppLibraryReference Header="capi.h" Language="C" Standard="c11" '
                                    'Library="capi.lib" />', 'Main.ns'), encoding='utf-8')
build_and_run('c', cfolder, 0, expected=9)
print('c: macros de un encabezado C importadas', flush=True)
negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\n#define T14_SQUARE(x) ((x) * (x))\n'
                                'inline int square(int x) noexcept { return x * x; }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
folder = negative / 'macro-funcion'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { return T14_SQUARE(3) - 9; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T14Neg', plain, 'Main.ns'), encoding='utf-8')
output = run('negativo-macro-funcion', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                        '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                        '-o', 'app.exe'], folder, expected=1).stdout
if "C/C++ macro 'T14_SQUARE' is a function-like macro" not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo macro-funcion: falta el diagnostico esperado; evidencia: {root}')
print('negativo macro-funcion: rechazado con su motivo', flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { return square(3) - 9; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T14Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: pasa', flush=True)
print('OK: macros C y C++ consumidas desde NativeSharp, frente a Clang; evidencia:', root)
