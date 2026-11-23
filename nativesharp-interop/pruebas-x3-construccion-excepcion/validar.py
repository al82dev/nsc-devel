"""X3-01/X2-06: una construccion que falla destruye la base C++ y los campos C++ ya construidos, como C++.

La biblioteca define `Base` (polimorfica: constructor `1`, destructor `2`) y `Obj` (constructor `3`, que lanza `7`
con un argumento negativo; destructor `4`). Una clase NativeSharp `D : L.Base` con campos `a = new L.Obj(1)` y
`b = new L.Obj(x)` y `D() : base(5)` se compara con `struct D : L::Base { L::Obj a{1}; L::Obj b{x}; }` compilado
por Clang 23.1.0:
- con `x = -1` la excepcion llega al `catch` de la biblioteca; `a` y la base se destruyen en orden inverso: `1342`
  (antes NativeSharp no destruia la base: `134`);
- con `x = 2` el objeto se destruye una sola vez al salir del ambito: `133442`.
O0-O3; en estatica el ejecutable arranca por el runtime de C (`--crt-startup`); con --dll la biblioteca es una DLL.
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-x3-construccion-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
HEADER = '''#pragma once
#if defined(X3_EXPORT)
#define API __declspec(dllexport)
#elif defined(X3_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace L {
using Cb = int(*)();
class API Base { public: explicit Base(int v); virtual ~Base(); virtual int get() const; int v; };
class API Obj { public: explicit Obj(int v); ~Obj(); int v; };
API int run(Cb f);
API int trace() noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace { int marks = 0; }
namespace L {
Base::Base(int x) : v(x) { marks = marks * 10 + 1; }
Base::~Base() { marks = marks * 10 + 2; }
int Base::get() const { return v; }
Obj::Obj(int x) : v(x) { if (x < 0) throw 7; marks = marks * 10 + 3; }
Obj::~Obj() { marks = marks * 10 + 4; }
int run(Cb f) { try { return f(); } catch (int e) { return 100 + e; } }
int trace() noexcept { return marks; }
}
'''
CONTROL = '''#include "api.h"
struct D : L::Base { L::Obj a{1}; L::Obj b{ARG}; D() : L::Base(5) {} };
static int build() { D d; return 0; }
int main() { int r = L::run(build); return (r * 1000000 + L::trace()) % 251; }
'''
NS = '''class D : L.Base { public L.Obj a = new L.Obj(1); public L.Obj b = new L.Obj(ARG); public D() : base(5) { } }
int Build() { { var d = new D(); } return 0; }
int Main() { L.Cb build = () => Build(); int r = L.run(build); return (r * 1000000 + L.trace()) % 251; }
'''


def run(label, command, cwd, expected=0, extra_env=None):
    result = subprocess.run(command, cwd=cwd, env={**env, **(extra_env or {})}, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
EXPECTED = {'-1': (107 * 1000000 + 1342) % 251, '2': 133442 % 251}
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    run(f'O{opt}-api', [*CXX, '-O' + str(opt), *(['-DX3_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'],
        case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                            *[str(p) for p in CRT]], case)
        (case / 'api.h').write_text('#define X3_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>X3Ctor</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')
    for argument, expected in EXPECTED.items():
        tag = f'O{opt}-x{argument}'
        (case / 'control.cpp').write_text(CONTROL.replace('ARG', argument), encoding='utf-8')
        run(tag + '-control-obj', [*CXX, '-O' + str(opt), '-c', 'control.cpp', '-o', 'control.obj'], case)
        run(tag + '-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj',
                                    'api.lib', *[str(p) for p in CRT]], case)
        control = run(tag + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
        if control != expected:
            raise SystemExit(f'{tag}: el control C++ devolvio {control}, se esperaba {expected}; evidencia: {root}')
        (case / 'Main.ns').write_text(NS.replace('ARG', argument), encoding='utf-8')
        command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                   '--no-artifact-cache', '-O' + str(opt)]
        extra = {'NSC_LINKER': 'nslink.exe'}
        if not args.dll:
            command.append('--crt-startup')
            for path in CRT:
                command += ['--link-lib', str(path)]
            extra = {'NSC_LINKER': 'lld-link.exe'}
        run(tag + '-nsc', command + ['-o', 'app.exe'], case, extra_env=extra)
        result = run(tag + '-run', [str(case / 'app.exe')], case, expected=None).returncode
        if result != control:
            raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
        print(f'{tag}: NativeSharp y Clang devuelven {result}', flush=True)
print('OK: una construccion que falla destruye la base C++ y los campos construidos, frente a Clang; evidencia:', root)
