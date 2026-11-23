"""X2-05: funciones C++ que devuelven por valor un record no trivial, consumidas desde locales NativeSharp.

La biblioteca define `Obj` (constructor, copia, movimiento y destructor que anotan contadores) y `Only` (solo se
mueve). `make(v)` devuelve un `Obj` por valor y lanza `7` si v es negativo; `make_only(v)` devuelve un `Only`.
NativeSharp construye el resultado en el local (adaptador en sitio, elision garantizada):
- `var a = make(1); var b = make(-1);` dentro de un callback que la biblioteca llama con `try`: la excepcion
  atraviesa el frame NativeSharp, `a` se destruye y `b`, que nunca se construyo, no (anotacion `12`);
- un retorno temprano destruye el local una sola vez;
- ni `make` ni `make_only` copian ni mueven: el objeto se construye una vez en el destino;
- el resultado se compara con el mismo programa escrito en C++ (Clang 23.1.0), O0-O3.
En estatica el ejecutable arranca por el runtime de C (`--crt-startup`); con --dll la biblioteca es una DLL con su
propio runtime. Una plantilla precompilada que devuelve el record queda fuera (catalogo, R2).
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
root = Path(tempfile.mkdtemp(prefix='nsc-x2-retorno-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
HEADER = '''#pragma once
#if defined(X2_EXPORT)
#define API __declspec(dllexport)
#elif defined(X2_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace X2 {
using Cb = int(*)();
class API Obj {
public:
    explicit Obj(int v);
    Obj(const Obj& o);
    Obj(Obj&& o) noexcept;
    ~Obj();
    int value() const noexcept { return v_; }
private:
    int v_;
};
class API Only {
public:
    explicit Only(int v);
    Only(const Only&) = delete;
    Only(Only&& o) noexcept;
    ~Only();
    int value() const noexcept { return v_; }
private:
    int v_;
};
API Obj make(int v);
API Only make_only(int v);
API int run(Cb f);
API int trace() noexcept;
API int copies() noexcept;
API int moves() noexcept;
API int alive() noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace { int marks = 0; int copied = 0; int moved = 0; int live = 0; }
namespace X2 {
Obj::Obj(int v) : v_(v) { marks = marks * 10 + 1; ++live; }
Obj::Obj(const Obj& o) : v_(o.v_) { ++copied; ++live; }
Obj::Obj(Obj&& o) noexcept : v_(o.v_) { ++moved; ++live; }
Obj::~Obj() { marks = marks * 10 + 2; --live; }
Only::Only(int v) : v_(v) { ++live; }
Only::Only(Only&& o) noexcept : v_(o.v_) { ++moved; ++live; }
Only::~Only() { --live; }
Obj make(int v) { if (v < 0) throw 7; return Obj(v); }
Only make_only(int v) { return Only(v); }
int run(Cb f) { try { return f(); } catch (int e) { return 100 + e; } }
int trace() noexcept { return marks; }
int copies() noexcept { return copied; }
int moves() noexcept { return moved; }
int alive() noexcept { return live; }
}
'''
CONTROL = '''#include "api.h"
static int throwing() { auto a = X2::make(1); auto b = X2::make(-1); return a.value() + b.value(); }
static int early() { auto a = X2::make(5); if (a.value() == 5) return 40; return 0; }
static int only() { auto m = X2::make_only(2); return m.value(); }
int main()
{
    int r = X2::run(throwing);
    int t = X2::trace();
    int e = early();
    int o = only();
    return r * 1000000 + t * 1000 + (e + o) * 10 + X2::copies() + X2::moves() + X2::alive();
}
'''
NS = '''int Throwing() { var a = X2.make(1); var b = X2.make(-1); return a.value() + b.value(); }
int Early() { var a = X2.make(5); if (a.value() == 5) return 40; return 0; }
int OnlyValue() { var m = X2.make_only(2); return m.value(); }
int Main()
{
    X2.Cb throwing = () => Throwing();
    int r = X2.run(throwing);
    int t = X2.trace();
    int e = Early();
    int o = OnlyValue();
    return r * 1000000 + t * 1000 + (e + o) * 10 + X2.copies() + X2.moves() + X2.alive();
}
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
# a(1) is built (1), make(-1) throws before b exists, a is destroyed (2): marks 12, read before early(); early()
# and only() add 40 + 2; no copy, no move, nothing alive at the end: 107 * 1e6 + 12 * 1e3 + 42 * 10.
EXPECTED = 107 * 1000000 + 12 * 1000 + 42 * 10
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'control.cpp').write_text(CONTROL, encoding='utf-8')
    run(f'O{opt}-api', [*CXX, '-O' + str(opt), *(['-DX2_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'],
        case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                            *[str(p) for p in CRT]], case)
        (case / 'api.h').write_text('#define X2_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', [*CXX, '-O' + str(opt), '-c', 'control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj',
                                 'api.lib', *[str(p) for p in CRT]], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'O{opt}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    (case / 'Main.ns').write_text(NS, encoding='utf-8')
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>X2Ret</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt)]
    extra = {'NSC_LINKER': 'nslink.exe'}
    if not args.dll:
        command.append('--crt-startup')
        for path in CRT:
            command += ['--link-lib', str(path)]
        extra = {'NSC_LINKER': 'lld-link.exe'}
    run(f'O{opt}-nsc', command + ['-o', 'app.exe'], case, extra_env=extra)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}: el resultado se construye en el local sin copias ni '
          f'movimientos, se destruye una vez, y si la funcion lanza no se destruye', flush=True)
print('OK: retorno por valor de records C++ no triviales en locales NativeSharp, frente a Clang; evidencia:', root)
