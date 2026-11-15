"""T15-002 a T15-007 y T15-013: excepciones C++ que atraviesan codigo NativeSharp.

La biblioteca lanza enteros y un tipo con destructor; funciones C++ con `try`/`catch` llaman a callbacks
NativeSharp que llaman a la funcion que lanza, asi que la excepcion atraviesa los frames de NSC. NativeSharp
ejecuta su `finally` y destruye un local C++ durante el desenrollado. Una funcion `noexcept` que deja escapar
una excepcion termina el proceso con `std::terminate`, igual que una excepcion que nadie atrapa; un `catch`
NativeSharp no atrapa excepciones C++. Un `panic` dentro de un callback termina el proceso con 142.

El ejecutable arranca por el runtime estatico de MSVC (`--crt-startup` y `--link-lib`), que inicializa el
manejo de excepciones de C++, y se enlaza con lld-link. Cada caso se compara con un control C++ compilado por
Clang 23.1.0 y enlazado con el mismo runtime, O0-O3; con --dll la biblioteca es una DLL con su propio runtime.
"""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t15-excepciones-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
env['NSC_LINKER'] = 'lld-link.exe'
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-isystem', str(TOOLSET / 'include'),
       '-isystem', str(SDK / 'include/ucrt')]

HEADER = '''#pragma once
#if defined(T15_EXPORT)
#define API __declspec(dllexport)
#elif defined(T15_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T15 {
using Cb = int(*)(int);
API int thrower(int x);
API int guarded(Cb f);
API void bump() noexcept;
API int bumps() noexcept;
struct API Tracker { int v; Tracker() noexcept; ~Tracker(); };
struct API Failure { int code; explicit Failure(int c) noexcept; Failure(const Failure& o) noexcept; ~Failure(); };
API int throwFailure(int x);
API int guardedFailure(Cb f);
API int failures() noexcept;
API int bad(int x) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T15 {
static int bumpCount = 0;
static int failureCount = 0;
int thrower(int x) { throw 40 + x; }
int guarded(Cb f) { try { return f(1); } catch (int e) { return e; } }
void bump() noexcept { ++bumpCount; }
int bumps() noexcept { return bumpCount; }
Tracker::Tracker() noexcept : v(1) {}
Tracker::~Tracker() { ++bumpCount; }
Failure::Failure(int c) noexcept : code(c) {}
Failure::Failure(const Failure& o) noexcept : code(o.code) {}
Failure::~Failure() { ++failureCount; }
int throwFailure(int x) { throw Failure(x); }
int guardedFailure(Cb f) { try { return f(7); } catch (const Failure& e) { return e.code; } }
int failures() noexcept { return failureCount; }
static int raise(int x) { throw x; }
int bad(int x) noexcept { return raise(x); }
}
'''
# (name, NativeSharp prelude, NativeSharp body, C++ control prelude, C++ control body)
CASES = [
    ('cruza-callback', '', 'T15.Cb f = (int v) => T15.thrower(v); return T15.guarded(f);',
     '', 'return T15::guarded([](int v) { return T15::thrower(v); });'),
    ('finally-y-destructor',
     'int Inner(int v) { try { T15.Tracker t = new T15.Tracker(); return T15.thrower(v) + t.v; } finally { T15.bump(); } }\n',
     'T15.Cb f = (int v) => Inner(v); int r = T15.guarded(f); return r + T15.bumps() * 100;',
     'static int inner(int v) { struct F { ~F() { T15::bump(); } } fin; T15::Tracker t; return T15::thrower(v) + t.v; }\n',
     'int r = T15::guarded(inner); return r + T15::bumps() * 100;'),
    ('tipo-con-destructor', '', 'T15.Cb f = (int v) => T15.throwFailure(v); int r = T15.guardedFailure(f); '
     'return r + (T15.failures() > 0 ? 100 : 0);',
     '', 'int r = T15::guardedFailure([](int v) { return T15::throwFailure(v); }); return r + (T15::failures() > 0 ? 100 : 0);'),
    ('noexcept-termina', '', 'return T15.bad(5);', '', 'return T15::bad(5);'),
    ('sin-catch-termina', '', 'return T15.thrower(1);', '', 'return T15::thrower(1);'),
    ('catch-nativesharp-no-atrapa', '', 'try { return T15.thrower(1); } catch { return 99; }', '', 'return T15::thrower(1);'),
    ('panic-en-callback', 'int Boom(int v) { panic("boom"); }\n', 'T15.Cb f = (int v) => Boom(v); return T15.guarded(f);',
     None, '142'),
]


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


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    lib = root / f'O{opt}'
    lib.mkdir()
    (lib / 'api.h').write_text(HEADER, encoding='utf-8')
    (lib / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    run(f'O{opt}-api', [*CXX, '-O' + str(opt), *(['-DT15_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], lib)
    if args.dll:
        # The DLL links its own static C runtime and starts it from its DLL entry.
        # La DLL enlaza su propio runtime estatico de C y lo arranca desde su entrada de DLL.
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                            *[str(p) for p in CRT]], lib)
        (lib / 'api.h').write_text('#define T15_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], lib)
    for name, prelude, body, control_prelude, control_body in CASES:
        case = lib / name
        case.mkdir()
        if args.dll:
            shutil.copyfile(lib / 'api.dll', case / 'api.dll')
        if control_prelude is None:
            expected = int(control_body)
        else:
            (case / 'Control.cpp').write_text('#include "../api.h"\n' + control_prelude + 'int main() { ' + control_body + ' }\n',
                                              encoding='utf-8')
            run(f'O{opt}-{name}-control-obj', [*CXX, '-O' + str(opt), '-c', 'Control.cpp', '-o', 'control.obj'], case)
            run(f'O{opt}-{name}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj',
                                                '../api.lib', *[str(p) for p in CRT]], case)
            expected = run(f'O{opt}-{name}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
        (case / 'Main.ns').write_text(prelude + 'int Main() { ' + body + ' }\n', encoding='utf-8')
        runtime = ' Runtime="../api.dll"' if args.dll else ''
        (case / 'App.nsproj').write_text(
            '<Project><PropertyGroup><Name>T15App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib"' + runtime + ' />'
            '</ItemGroup></Project>', encoding='utf-8')
        command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                   '--no-artifact-cache', '-O' + str(opt), '--crt-startup']
        for library in CRT:
            command += ['--link-lib', str(library)]
        run(f'O{opt}-{name}-nsc', command + ['-o', 'app.exe'], case)
        result = run(f'O{opt}-{name}-run', [str(case / 'app.exe')], case, expected=None).returncode
        if result != expected:
            raise SystemExit(f'O{opt}-{name}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')
        print(f'O{opt}-{name}: NativeSharp y Clang devuelven {result}', flush=True)
# NativeSharp without the C runtime startup does not initialize C++ exception handling: diagnose NSC_LINKER.
# NativeSharp sin el arranque del runtime de C no inicializa las excepciones C++: diagnostico de NSC_LINKER.
case = root / 'O0' / 'cruza-callback'
env['NSC_LINKER'] = 'nslink.exe'
output = run('negativo-nslink', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                 '--no-artifact-cache', '--crt-startup', '-o', 'nslink.exe'], case, expected=1).stdout
if '--crt-startup links with lld-link' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo nslink: falta el diagnostico esperado; evidencia: {root}')
print('negativo nslink: --crt-startup con NSLink se rechaza con su motivo', flush=True)
print('OK: excepciones C++ que atraviesan NativeSharp, frente a Clang; evidencia:', root)
