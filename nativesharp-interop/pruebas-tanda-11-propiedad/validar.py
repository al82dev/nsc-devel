"""T11-018, T11-019 y T11-022: objetos C++ dinamicos, punteros no propietarios y propiedad en una DLL.

La biblioteca crea un objeto con `new` y lo destruye con `delete` en sus propias funciones; NativeSharp
solo guarda el puntero, que no posee el objeto, igual que un puntero a un objeto estatico de la
biblioteca. Un local NativeSharp construido en paralelo se destruye al salir de su bloque. Un contador
de objetos vivos de la biblioteca comprueba que cada objeto se destruye una sola vez y por su
propietario. Con --dll la biblioteca es una DLL enlazada con el CRT, que reserva y libera en su propio
heap; sin --dll, la biblioteca estatica, sin CRT, reserva en un almacen propio. Compara un control C++
compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, y exige los tipos fisicos de Clang
en el IR de NSC.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t11-propiedad-', dir='A:/temp'))
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
struct R { int v; API R(int x) noexcept; API ~R(); API int value() const noexcept; };
API int live() noexcept;
API R* create(int x) noexcept;
API void destroy(R* r) noexcept;
API R* shared() noexcept;
API void bump(R& r) noexcept;
}
'''
# With --dll the DLL reserves with its CRT heap; a static library without a CRT reserves in its own pool. The
# shared object is built on first use and never destroyed, without the CRT's guarded statics.
ALLOCATION = ('R* create(int x) noexcept { return new R(x); }\nvoid destroy(R* r) noexcept { delete r; }\n' if args.dll else
              'alignas(R) static unsigned char pool[4][sizeof(R)];\nstatic bool used[4];\n'
              'R* create(int x) noexcept { for (int i = 0; i < 4; ++i) if (!used[i]) { used[i] = true; return new (pool[i]) R(x); } '
              'return nullptr; }\n'
              'void destroy(R* r) noexcept { for (int i = 0; i < 4; ++i) if ((void*)pool[i] == (void*)r) { r->~R(); used[i] = false; } }\n')
SOURCE = '''#include "api.h"
inline void* operator new(decltype(sizeof(0)), void* place) noexcept { return place; }
''' + ('' if args.dll else 'extern "C" __declspec(selectany) int _fltused = 0;\n') + '''namespace T11 {
static int count = 0;
R::R(int x) noexcept : v(x) { ++count; }
R::~R() { --count; }
int R::value() const noexcept { return v; }
int live() noexcept { return count; }
''' + ALLOCATION + '''alignas(R) static unsigned char sharedStorage[sizeof(R)];
static R* sharedObject = nullptr;
R* shared() noexcept { if (!sharedObject) sharedObject = new (sharedStorage) R(7); return sharedObject; }
void bump(R& r) noexcept { r.v += 10; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    using namespace T11;
    int r = 0;
    R* s = shared(); s->v = s->v + 1;
    R* a = create(3);
    { R l(5); bump(l); r += l.v; }
    r += a->value() + live();
    destroy(a);
    r += live() + shared()->v;
    return r;
}
'''
NS_BODY = ('int r = 0; T11.R* s = T11.shared(); s->v = s->v + 1; T11.R* a = T11.create(3); '
           'if (true) { T11.R l = new T11.R(5); T11.bump(ref l); r += l.v; } '
           'r += a->value() + T11.live(); T11.destroy(a); r += T11.live() + T11.shared()->v; return r;')
EXPECTED = 15 + 3 + 2 + 1 + 8
DECLARATIONS = 8
CRT = Path('A:/vc2026-runtime/lib/x64/release')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib')
CRT_LIBRARIES = [CRT / 'libcmt.lib', CRT / 'libvcruntime.lib', SDK / 'ucrt/x64/libucrt.lib', SDK / 'um/x64/kernel32.lib']


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
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                                 *[str(path) for path in CRT_LIBRARIES]], case)
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
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe']
    result = subprocess.run(command, cwd=folder, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (tag + '-build.log')).write_text(result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(f'{tag}: la construccion fallo; evidencia: {root}')
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


print('OK: objetos C++ dinamicos, punteros no propietarios y propiedad en la biblioteca, frente a Clang; evidencia:', root)
