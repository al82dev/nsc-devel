"""T15-014 a T15-018: punteros inteligentes, allocators, tipos movibles no copiables y corutinas.

La biblioteca, compilada con la STL de MSVC, define un puntero inteligente propietario no copiable
(`Owner<T>`, con un contador de liberaciones), un allocator con estado (`Arena`), usa `std::unique_ptr` y
`std::allocator` en sus funciones y define un generador con `co_yield`. NativeSharp guarda un `Owner<int>` en
un local que lo destruye al salir del bloque, transfiere la propiedad con una funcion C++ que mueve el
puntero, pasa un `Arena` con estado a la biblioteca y suspende, reanuda y libera una corutina cuyo frame
reserva el runtime de C++. Copiar un `Owner<int>` o un `std::unique_ptr<int>` se rechaza (NS3020).

El ejecutable arranca por el runtime estatico de MSVC (`--crt-startup`), que reserva los frames y los
objetos. Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3; con --dll
la biblioteca es una DLL con su propio runtime.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t15-recursos-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
env['NSC_LINKER'] = 'lld-link.exe'
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib', SDK / 'lib/um/x64/synchronization.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-isystem', str(TOOLSET / 'include'),
       '-isystem', str(SDK / 'include/ucrt')]
INCLUDES = ''.join('<IncludeDirectory Path="' + str(p).replace('\\', '/') + '" Kind="System" />'
                   for p in (TOOLSET / 'include', SDK / 'include/ucrt'))

HEADER = '''#pragma once
#include <memory>
#include <coroutine>
#if defined(T15_EXPORT)
#define API __declspec(dllexport)
#elif defined(T15_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T15 {
API int released() noexcept;
API void noteRelease() noexcept;
template <class T> struct Owner {
    T* p;
    Owner() noexcept : p(nullptr) {}
    Owner(const Owner&) = delete;
    Owner& operator=(const Owner&) = delete;
    ~Owner() { if (p) { delete p; noteRelease(); } }
};
API void fill(Owner<int>& o, int v);
API void hand(Owner<int>& dst, Owner<int>& src) noexcept;
API int value(const Owner<int>& o) noexcept;
struct Arena { int used; int capacity; Arena() noexcept : used(0), capacity(64) {} };
API int sumWith(Arena& a, int n);
API int viaUnique(int v);
API int viaAllocator(int v);
struct Gen {
    struct promise_type {
        int v = 0;
        ~promise_type();
        Gen get_return_object() noexcept { return Gen{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(int x) noexcept { v = x; return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept {}
    };
    std::coroutine_handle<promise_type> h;
};
API void start(Gen& g, int n);
API int next(Gen& g) noexcept;
API bool done(const Gen& g) noexcept;
API void destroy(Gen& g) noexcept;
API int frames() noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T15 {
static int releaseCount = 0;
static int frameCount = 0;
int released() noexcept { return releaseCount; }
void noteRelease() noexcept { ++releaseCount; }
void fill(Owner<int>& o, int v) { delete o.p; o.p = new int(v); }
void hand(Owner<int>& dst, Owner<int>& src) noexcept { delete dst.p; dst.p = src.p; src.p = nullptr; }
int value(const Owner<int>& o) noexcept { return o.p ? *o.p : -1; }
int sumWith(Arena& a, int n) { int s = 0; for (int i = 0; i < n && a.used < a.capacity; ++i) { s += i; a.used += 8; } return s; }
int viaUnique(int v) { auto p = std::make_unique<int>(v); auto q = std::move(p); return *q + (p ? 1000 : 0); }
int viaAllocator(int v) { std::allocator<int> a; int* p = a.allocate(2); p[0] = v; p[1] = 1; int r = p[0] + p[1]; a.deallocate(p, 2); return r; }
Gen::promise_type::~promise_type() { --frameCount; }
static Gen counter(int n) { for (int i = 0; i < n; ++i) co_yield i * 10; }
void start(Gen& g, int n) { ++frameCount; g = counter(n); }
int next(Gen& g) noexcept { g.h.resume(); return g.h.done() ? -1 : g.h.promise().v; }
bool done(const Gen& g) noexcept { return g.h.done(); }
void destroy(Gen& g) noexcept { g.h.destroy(); }
int frames() noexcept { return frameCount; }
}
'''
CONTROL = '''#include "api.h"
static int use() {
    T15::Owner<int> a; T15::Owner<int> b;
    T15::fill(a, 42); T15::hand(b, a);
    return T15::value(b) + (T15::value(a) == -1 ? 100 : 0);
}
int main() {
    int r = use() + T15::released() * 1000;
    T15::Arena arena; r += T15::sumWith(arena, 4) + arena.used;
    r += T15::viaUnique(5) + T15::viaAllocator(6);
    T15::Gen g; T15::start(g, 3);
    int first = T15::next(g); int second = T15::next(g); int live = T15::frames();
    T15::next(g); int last = T15::next(g); bool finished = T15::done(g);
    T15::destroy(g);
    return r + first + second + live * 7 + (last == -1 && finished ? 300 : 0) + T15::frames() * 50;
}
'''
PRELUDE = ('int Use() { T15.Owner<int> a = new T15.Owner<int>(); T15.Owner<int> b = new T15.Owner<int>(); '
           'T15.fill(ref a, 42); T15.hand(ref b, ref a); return T15.value(in b) + (T15.value(in a) == -1 ? 100 : 0); }\n')
NS_BODY = ('int r = Use() + T15.released() * 1000; '
           'T15.Arena arena = new T15.Arena(); r += T15.sumWith(ref arena, 4) + arena.used; '
           'r += T15.viaUnique(5) + T15.viaAllocator(6); '
           'T15.Gen g = new T15.Gen(); T15.start(ref g, 3); '
           'int first = T15.next(ref g); int second = T15.next(ref g); int live = T15.frames(); '
           'T15.next(ref g); int last = T15.next(ref g); bool finished = T15.done(in g); '
           'T15.destroy(ref g); '
           'return r + first + second + live * 7 + (last == -1 && finished ? 300 : 0) + T15.frames() * 50;')


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


def build(folder, opt, prelude, body, output):
    (folder / 'Main.ns').write_text(prelude + 'int Main() { ' + body + ' }\n', encoding='utf-8')
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T15App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + '>' +
        INCLUDES + '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt), '--crt-startup']
    for library in CRT:
        command += ['--link-lib', str(library)]
    return command + ['-o', output]


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    run(f'O{opt}-api', [*CXX, '-O' + str(opt), *(['-DT15_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                            *[str(p) for p in CRT]], case)
        (case / 'api.h').write_text('#define T15_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', [*CXX, '-O' + str(opt), '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj', 'api.lib',
                                 *[str(p) for p in CRT]], case)
    expected = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    run(f'O{opt}-nsc', build(case, opt, PRELUDE, NS_BODY, 'app.exe'), case)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != expected:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}', flush=True)
case = root / 'O0'
for name, body in (('copia-owner', 'T15.Owner<int> a = new T15.Owner<int>(); T15.Owner<int> b = a; return 0;'),
                   ('copia-unique', 'std.unique_ptr<int> a = new std.unique_ptr<int>(); std.unique_ptr<int> b = a; return 0;')):
    output = run('negativo-' + name, build(case, 0, '', body, name + '.exe'), case, expected=1).stdout
    if 'NS3020' not in output or 'cannot be copied' not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: la copia de un tipo C++ no copiable se rechaza (NS3020)', flush=True)
print('OK: punteros inteligentes, allocators, tipos no copiables y corutinas, frente a Clang; evidencia:', root)
