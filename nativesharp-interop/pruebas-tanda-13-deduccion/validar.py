"""T13-005 y T13-014: llamadas a plantillas de funcion C++ con argumentos deducidos.

El programa NativeSharp llama a plantillas de funcion C++ sin argumentos explicitos: con enteros, double,
un puntero, un record pasado a `const T&` (`in p`), un paquete, una plantilla sobrecargada con una funcion que no es
plantilla y una plantilla con restriccion. El primer analisis registra los tipos de los argumentos, Sema
deduce los argumentos de plantilla y los cuerpos se analizan otra vez con las especializaciones deducidas.
Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, y exige los tipos
fisicos de Clang en cada declaracion C++ que NSC llama. Los rechazos (restriccion no satisfecha, argumento
sin tipo C++) llevan el diagnostico de Clang y tienen su contraparte valida.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t13-deduccion-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T13_EXPORT)
#define API __declspec(dllexport)
#elif defined(T13_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T13 {
struct Point { int x; int y; };
template <class T> T twice(T x) noexcept { return x + x; }
template <class T> T maxOf(T a, T b) noexcept { return a > b ? a : b; }
template <class T> int sizeOfPtr(const T* p) noexcept { return (int)sizeof(*p); }
template <class T> int sumPoint(const T& p) noexcept { return p.x + p.y; }
template <class... Ts> int countAll(Ts... xs) noexcept { return (int)sizeof...(Ts) + (0 + ... + (int)xs); }
inline int over(int x) noexcept { return x + 1000; }
template <class T> int over(T x) noexcept { return (int)x + 2000; }
template <class T> requires (sizeof(T) <= 4) int small(T x) noexcept { return (int)x; }
API int libValue(int x) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T13 {
int libValue(int x) noexcept { return twice(x) + 1; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    int r = 0;
    r += T13::twice(21);
    r += (int)T13::twice(1.5);
    r += T13::maxOf(4, 9);
    r += (int)T13::maxOf(3.5, 2.0);
    int x = 7; r += T13::sizeOfPtr(&x);
    T13::Point p; p.x = 4; p.y = 5; r += T13::sumPoint(p);
    r += T13::countAll(1, 2, 3);
    r += T13::over(5);
    r += T13::over(5.5);
    r += T13::small(3);
    r += T13::libValue(2);
    return r;
}
'''
NS_BODY = ('int r = 0; '
           'r += T13.twice(21); '
           'r += (int)T13.twice(1.5); '
           'r += T13.maxOf(4, 9); '
           'r += (int)T13.maxOf(3.5, 2.0); '
           'int x = 7; r += T13.sizeOfPtr(&x); '
           'T13.Point p; p.x = 4; p.y = 5; r += T13.sumPoint(in p); '
           'r += T13.countAll(1, 2, 3); '
           'r += T13.over(5); '
           'r += T13.over(5.5); '
           'r += T13.small(3); '
           'r += T13.libValue(2); '
           'return r;')
EXPECTED = 42 + 3 + 9 + 3 + 4 + 9 + 9 + 1005 + 2005 + 3 + 5
# Direct calls: the deduced specializations, the non-template over and libValue.
DIRECT = 11


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
    """Physical parameter and return types of every T13 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T13@@[^"(]*)"?\((.*)\)', line)
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
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT13_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T13_IMPORT 1\n' + HEADER, encoding='utf-8')
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
    called = set(re.findall(r'(?:call|invoke) [^@]*@"(\?[^"]*T13@@[^"]*)"\(', ir))
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


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'directo-O{opt}'
    clang = build_library(case, opt)
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T13App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    direct = compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden ({direct} declaraciones iguales a las de Clang)',
          flush=True)
negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('''#pragma once
namespace T13 {
template <class T> requires (sizeof(T) <= 4) int small(T x) noexcept { return (int)x; }
template <class T> T twice(T x) noexcept { return x + x; }
}
''', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
CASES = [
    ('restriccion', 'return T13.small(2.5);', 'constraints not satisfied', 'return T13.small(2) - 2;'),
    ('sin-tipo-cpp', 'string s = "x"; T13.twice(s); return 0;', 'no C++ type', 'return T13.twice(2) - 4;'),
]
for name, body, clang_text, counterpart in CASES:
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T13Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if 'Clang cannot deduce the template arguments of the C++ function template' not in output or \
            clang_text not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado con el diagnostico de Clang', flush=True)
    folder = negative / (name + '-contraparte')
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + counterpart + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T13Pos', plain, 'Main.ns'), encoding='utf-8')
    build_and_run(name + '-contraparte', folder, 0, expected=0)
    print(f'contraparte {name}: pasa', flush=True)
print('OK: llamadas a plantillas de funcion C++ con argumentos deducidos, frente a Clang; evidencia:', root)
