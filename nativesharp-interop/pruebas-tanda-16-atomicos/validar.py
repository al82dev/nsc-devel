"""T16-017: operaciones atomicas compartidas entre NativeSharp y C++ desde varios hilos.

La biblioteca crea cuatro hilos con `std::thread`; cada uno llama mil veces a un callback NativeSharp que
incrementa un `std::atomic<long long>` de la biblioteca (`fetch_add`, orden secuencial) y publica un dato con
liberacion y adquisicion (`store(release)` / `load(acquire)`). El total y los datos publicados coinciden con el
control. En estatica el ejecutable arranca por el runtime de C (`--crt-startup`); con --dll la biblioteca es
una DLL con su propio runtime. Compara con un control C++ de Clang 23.1.0, O0-O3.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t16-atomicos-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-isystem', str(TOOLSET / 'include'),
       '-isystem', str(SDK / 'include/ucrt')]
INCLUDES = ''.join('<IncludeDirectory Path="' + str(p).replace('\\', '/') + '" Kind="System" />'
                   for p in (TOOLSET / 'include', SDK / 'include/ucrt'))

HEADER = '''#pragma once
#if defined(T16_EXPORT)
#define API __declspec(dllexport)
#elif defined(T16_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T16 {
using Cb = int(*)(int);
API int bump(int k) noexcept;
API long long hammer(Cb f, int threads, int each);
API int published() noexcept;
}
'''
SOURCE = '''#include "api.h"
#include <atomic>
#include <thread>
#include <vector>
namespace {
std::atomic<long long> counter{0};
std::atomic<int> ready{0};
int data[8];
}
namespace T16 {
int bump(int k) noexcept {
    counter.fetch_add(1);
    if (k == 999) { data[ready.load(std::memory_order_relaxed) & 7] = 1; ready.fetch_add(1, std::memory_order_release); }
    return k;
}
long long hammer(Cb f, int threads, int each) {
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) workers.emplace_back([=] { for (int i = 0; i < each; ++i) f(i); });
    for (auto& w : workers) w.join();
    return counter.load();
}
int published() noexcept { int r = 0; int n = ready.load(std::memory_order_acquire); for (int i = 0; i < 8; ++i) r += data[i]; return n * 10 + (r > 0 ? 1 : 0); }
}
'''
CONTROL = '#include "api.h"\nint main() { long long n = T16::hammer([](int k) { return T16::bump(k); }, 4, 1000); return (int)(n / 100) + T16::published(); }\n'
NS_BODY = 'T16.Cb f = (int k) => T16.bump(k); long n = T16.hammer(f, 4, 1000); return (int)(n / 100) + T16.published();'
EXPECTED = 40 + 41


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


def project(folder):
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (folder / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T16App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + '>' + INCLUDES + '</CppLibraryReference>'
        '</ItemGroup></Project>', encoding='utf-8')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    run(f'O{opt}-api', [*CXX, '-O' + str(opt), *(['-DT16_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                            *[str(p) for p in CRT]], case)
        (case / 'api.h').write_text('#define T16_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', [*CXX, '-O' + str(opt), '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj', 'api.lib',
                                 *[str(p) for p in CRT]], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'O{opt}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    project(case)
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt)]
    extra = {}
    if not args.dll:
        command.append('--crt-startup')
        for library in CRT:
            command += ['--link-lib', str(library)]
        extra = {'NSC_LINKER': 'lld-link.exe'}
    run(f'O{opt}-nsc', command + ['-o', 'app.exe'], case, extra_env=extra)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != EXPECTED:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {EXPECTED}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}; cuatro hilos incrementan el atomico compartido', flush=True)
print('OK: operaciones atomicas compartidas desde varios hilos, frente a Clang; evidencia:', root)
