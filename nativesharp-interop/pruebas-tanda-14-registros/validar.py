"""T14-006 y T14-008: bit-fields y atributos de layout en records C++.

El encabezado declara records C++ con bit-fields con y sin signo que comparten unidad, un bit-field de
ancho cero, un record empaquetado con `[[gnu::packed]]` y otro con `#pragma pack(1)`, uno con `alignas(16)`,
uno con `__declspec(align(32))` y uno con `[[msvc::no_unique_address]]`. NativeSharp escribe y lee sus
campos con el layout de Clang y la biblioteca los lee; los tamanos coinciden con `sizeof` de C++. Compara
un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, en estatica y DLL.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t14-registros-', dir='A:/temp'))
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
namespace T14 {
struct Flags { unsigned a : 3; unsigned b : 5; int s : 4; unsigned : 0; unsigned c : 7; int tail; };
struct [[gnu::packed]] Pk { char a; int b; short c; };
#pragma pack(push, 1)
struct Pk1 { char a; double d; };
#pragma pack(pop)
struct alignas(16) Al { int a; };
struct __declspec(align(32)) Al32 { int a; };
struct Empty {};
struct Nu { [[msvc::no_unique_address]] Empty e; int x; };
API int readFlags(const Flags* f) noexcept;
API int readPk(const Pk* p) noexcept;
API int readPk1(const Pk1* p) noexcept;
API int sizes() noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T14 {
int readFlags(const Flags* f) noexcept { return (int)f->a + (int)f->b * 10 + f->s * 100 + (int)f->c * 1000 + f->tail; }
int readPk(const Pk* p) noexcept { return p->a + p->b + p->c; }
int readPk1(const Pk1* p) noexcept { return p->a + (int)p->d; }
int sizes() noexcept {
    return (int)sizeof(Flags) + (int)sizeof(Pk) * 10 + (int)sizeof(Pk1) * 100 + (int)sizeof(Al) * 1000 +
        (int)sizeof(Al32) * 10000 + (int)sizeof(Nu) * 100000;
}
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    T14::Flags f; f.a = 5; f.b = 9; f.s = -3; f.c = 77; f.tail = 1;
    int r = T14::readFlags(&f) + f.a + f.b + f.s + f.c;
    T14::Pk p; p.a = 1; p.b = 2; p.c = 3; r += T14::readPk(&p) + p.b;
    T14::Pk1 q; q.a = 4; q.d = 5.0; r += T14::readPk1(&q);
    T14::Al al; al.a = 6; T14::Al32 al32; al32.a = 7; T14::Nu nu; nu.x = 8; r += al.a + al32.a + nu.x;
    int sizes = (int)sizeof(T14::Flags) + (int)sizeof(T14::Pk) * 10 + (int)sizeof(T14::Pk1) * 100 +
        (int)sizeof(T14::Al) * 1000 + (int)sizeof(T14::Al32) * 10000 + (int)sizeof(T14::Nu) * 100000;
    return r + (sizes == T14::sizes() ? 0 : 1000000);
}
'''
NS_BODY = ('T14.Flags f; f.a = (uint)5; f.b = (uint)9; f.s = -3; f.c = (uint)77; f.tail = 1; '
           'int r = T14.readFlags(&f) + (int)f.a + (int)f.b + f.s + (int)f.c; '
           'T14.Pk p; p.a = (sbyte)1; p.b = 2; p.c = (short)3; r += T14.readPk(&p) + p.b; '
           'T14.Pk1 q; q.a = (sbyte)4; q.d = 5.0; r += T14.readPk1(&q); '
           'T14.Al al; al.a = 6; T14.Al32 al32; al32.a = 7; T14.Nu nu; nu.x = 8; r += al.a + al32.a + nu.x; '
           'int sizes = (int)sizeof(T14.Flags) + (int)sizeof(T14.Pk) * 10 + (int)sizeof(T14.Pk1) * 100 + '
           '(int)sizeof(T14.Al) * 1000 + (int)sizeof(T14.Al32) * 10000 + (int)sizeof(T14.Nu) * 100000; '
           'return r + (sizes == T14.sizes() ? 0 : 1000000);')
EXPECTED = None
DIRECT = 4


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
    # A DLL linked without the C runtime defines the floating-point marker itself.
    # Una DLL enlazada sin el runtime de C define ella misma el marcador de coma flotante.
    (case / 'api.cpp').write_text(SOURCE + ('extern "C" __declspec(selectany) int _fltused = 0;\n' if args.dll else ''),
                                  encoding='utf-8')
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
    global EXPECTED
    if EXPECTED is None:
        EXPECTED = control
    if control != EXPECTED or control >= 1000000:
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


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'directo-O{opt}'
    case.mkdir(parents=True)
    clang = build_library_in(case, opt)
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T14App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    direct = compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected=EXPECTED)
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden en {EXPECTED} ({direct} declaraciones iguales a las '
          'de Clang)', flush=True)
print('OK: bit-fields y atributos de layout en records C++, frente a Clang; evidencia:', root)
