"""T13-021 y T13-022: constantes C++ evaluadas por Clang.

El encabezado declara variables constexpr y const de enlace interno (enteras con y sin signo, negativas, de
64 bits, bool, double, float, enum), una inicializada con una funcion constexpr y otra con un miembro static
constexpr de una especializacion, una variable inline constexpr de enlace externo, un miembro static constexpr
de una especializacion escrita en NativeSharp y una funcion constexpr.
NSC lee cada constante con el valor del evaluador de constantes de Clang, sin simbolo de biblioteca, y la
funcion constexpr se llama con la definicion que emite Clang. Compara un control C++ compilado por Clang
23.1.0 con el consumidor NativeSharp, O0-O3. Tomar la direccion de una constante se rechaza; la contraparte
la lee.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t13-constexpr-', dir='A:/temp'))
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
constexpr int K = 7;
constexpr long long L = 1LL << 40;
constexpr unsigned U = 0xFFFFFFF0u;
constexpr int Neg = -12;
constexpr bool Flag = true;
constexpr double Pi = 3.25;
constexpr float Half = 0.5f;
const int Plain = 40 + 2;
enum class Color : int { Red = 1, Green = 2 };
constexpr Color Fav = Color::Green;
constexpr int sq(int x) noexcept { return x * x; }
constexpr int Sq5 = sq(5);
template <class T> struct Traits { static constexpr int size = sizeof(T); };
constexpr int SizeOfLong = Traits<long long>::size;
inline constexpr int Inl = 99;
API int libSq(int x) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T13 {
int libSq(int x) noexcept { return sq(x) + K - 7; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    int r = T13::K + (int)(T13::L >> 38) + (int)(T13::U >> 24) + T13::Neg + (T13::Flag ? 1 : 0);
    T13::Color fav = T13::Fav; r += (int)(T13::Pi * 4.0) + (int)(T13::Half * 2.0f) + T13::Plain + (int)fav;
    r += T13::Sq5 + T13::SizeOfLong + T13::Inl + T13::sq(3) + T13::libSq(2) + T13::Traits<int>::size;
    return r;
}
'''
NS_BODY = ('int r = T13.K + (int)(T13.L >> 38) + (int)(T13.U >> 24) + T13.Neg + (T13.Flag ? 1 : 0); '
           'T13.Color fav = T13.Fav; r += (int)(T13.Pi * 4.0) + (int)(T13.Half * 2.0f) + T13.Plain + (int)fav; '
           'r += T13.Sq5 + T13.SizeOfLong + T13.Inl + T13.sq(3) + T13.libSq(2) + T13.Traits<int>.size; '
           'return r;')
EXPECTED = 7 + 4 + 255 - 12 + 1 + 13 + 1 + 42 + 2 + 25 + 8 + 99 + 9 + 4 + 4
# Direct calls: sq and libSq.
DIRECT = 2


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
    if '?K@T13@@' in ir or '?Sq5@T13@@' in ir:
        raise SystemExit(f'directo-O{opt}: NSC nombra el simbolo de una constante interna; evidencia: {root}')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden ({direct} declaraciones iguales a las de Clang)',
          flush=True)
negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('''#pragma once
namespace T13 {
constexpr int K = 7;
}
''', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
folder = negative / 'direccion'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { int* p = &T13.K; return *p; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T13Neg', plain, 'Main.ns'), encoding='utf-8')
output = run('negativo-direccion', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                    '-o', 'app.exe'], folder, expected=1).stdout
if 'cannot take the address of readonly storage' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo direccion: falta el diagnostico esperado; evidencia: {root}')
print('negativo direccion: rechazado', flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { int k = T13.K; int* p = &k; return *p - 7; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T13Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: pasa', flush=True)
print('OK: constantes C++ evaluadas por Clang, frente a Clang; evidencia:', root)
