"""T16-001 a T16-005, T16-008, T16-012 y T16-013: arranque, constructores y destructores globales.

La biblioteca tiene un objeto global con constructor y destructor, y un entero con inicializacion dinamica.
El constructor, el `Main` NativeSharp y el destructor escriben una marca en un archivo con Win32
(`CreateFileA`/`WriteFile`), una observacion independiente del CRT: debe quedar `cmd`. En estatica el
ejecutable arranca por el runtime de C (`--crt-startup`), que ejecuta los constructores antes de `main` y los
destructores al salir; con --dll la biblioteca es una DLL con su propio runtime, que los ejecuta al cargarse y
al descargarse, y el ejecutable usa la entrada de NativeSharp. Sin `--crt-startup`, un ejecutable o una DLL
NativeSharp que toma un miembro de una biblioteca estatica con inicializadores dinamicos se rechaza nombrando el
miembro; si el enlace no toma ese miembro, enlaza y se ejecuta. Compara con un control C++ de Clang 23.1.0, O0-O3.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t16-arranque-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']

HEADER = '''#pragma once
#if defined(T16_EXPORT)
#define API __declspec(dllexport)
#elif defined(T16_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T16 {
API int value() noexcept;
API void noteMain() noexcept;
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(dllimport) void* __stdcall CreateFileA(const char*, unsigned long, unsigned long, void*,
                                                             unsigned long, unsigned long, void*);
extern "C" __declspec(dllimport) int __stdcall WriteFile(void*, const void*, unsigned long, unsigned long*, void*);
extern "C" __declspec(dllimport) int __stdcall CloseHandle(void*);
// Appends one letter to marca.txt with Win32 only. / Agrega una letra a marca.txt solo con Win32.
static void mark(const char* letter) noexcept {
    void* h = CreateFileA("marca.txt", 0x0004, 0, nullptr, 4, 0x80, nullptr);
    unsigned long written = 0;
    if (h != (void*)-1) { WriteFile(h, letter, 1, &written, nullptr); CloseHandle(h); }
}
namespace {
struct Global { int v; Global() noexcept : v(7) { mark("c"); } ~Global() { mark("d"); } };
int compute() noexcept { return 35; }
Global global;
int dynamic = compute();
}
namespace T16 {
int value() noexcept { return global.v + dynamic; }
void noteMain() noexcept { mark("m"); }
}
'''
CONTROL = '#include "api.h"\nint main() { T16::noteMain(); return T16::value(); }\n'
NS_BODY = 'T16.noteMain(); return T16.value();'
EXPECTED = 42


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


def observe(label, executable, cwd):
    marker = cwd / 'marca.txt'
    if marker.exists():
        marker.unlink()
    code = run(label, [str(executable)], cwd, expected=None).returncode
    return code, marker.read_text(encoding='ascii') if marker.exists() else ''


def project(folder):
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (folder / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T16App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
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
    control, control_mark = observe(f'O{opt}-control-run', case / 'control.exe', case)
    if control != EXPECTED or control_mark != 'cmd':
        raise SystemExit(f'O{opt}: el control C++ devolvio {control} con marca {control_mark!r}; evidencia: {root}')
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
    result, mark = observe(f'O{opt}-run', case / 'app.exe', case)
    if result != EXPECTED or mark != 'cmd':
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result} con marca {mark!r}, Clang {EXPECTED} con cmd; '
                         f'evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}; constructor, Main y destructor en orden (cmd)', flush=True)
if not args.dll:
    case = root / 'O0'
    output = run('negativo-sin-crt', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                      '--no-artifact-cache', '-o', 'sin-crt.exe'], case, expected=1).stdout
    if "has dynamic initializers in its linked member 'api.obj'" not in output or '--crt-startup' not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo sin-crt: falta el diagnostico esperado; evidencia: {root}')
    print('negativo sin-crt: una biblioteca estatica con inicializadores dinamicos sin --crt-startup se rechaza', flush=True)
    # A NativeSharp DLL does not start the C runtime either. / Una DLL NativeSharp tampoco arranca el runtime de C.
    (case / 'Lib.ns').write_text('namespace P16 { public class Lib { public static int Get() { return T16.value(); } } }\n',
                                 encoding='utf-8')
    (case / 'Lib.nsproj').write_text(
        '<Project><PropertyGroup><Name>T16Lib</Name><OutputType>SharedLibrary</OutputType></PropertyGroup><ItemGroup>'
        '<Compile Include="Lib.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
        'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
    output = run('negativo-dll-ns', [args.nsc, '--emit-project-oop-dll', 'Lib.nsproj', '--cpp-clang-root=' + args.clang_root,
                                     '--no-artifact-cache', '-o', 'lib.dll'], case, expected=1).stdout
    if 'has dynamic initializers' not in output or 'a NativeSharp DLL does not start the C runtime' not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo dll-ns: falta el diagnostico esperado; evidencia: {root}')
    print('negativo dll-ns: una DLL NativeSharp con esa biblioteca estatica se rechaza con su motivo', flush=True)
    # Only the members the link takes count. / Solo cuentan los miembros que toma el enlace.
    partial = root / 'parcial'
    partial.mkdir()
    (partial / 'api.h').write_text('#pragma once\nnamespace T16 { int plain(int x) noexcept; int started() noexcept; }\n',
                                   encoding='utf-8')
    (partial / 'plain.cpp').write_text('#include "api.h"\nnamespace T16 { int plain(int x) noexcept { return x + 1; } }\n',
                                       encoding='utf-8')
    (partial / 'started.cpp').write_text('#include "api.h"\nnamespace { int compute() noexcept { return 5; } int dynamic = '
                                         'compute(); }\nnamespace T16 { int started() noexcept { return dynamic; } }\n',
                                         encoding='utf-8')
    for unit in ('plain', 'started'):
        run('parcial-' + unit, [*CXX, '-c', unit + '.cpp', '-o', unit + '.obj'], partial)
    run('parcial-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'plain.obj', 'started.obj'], partial)
    (partial / 'Main.ns').write_text('int Main() { return T16.plain(41); }\n', encoding='utf-8')
    (partial / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T16Partial</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib" />'
        '</ItemGroup></Project>', encoding='utf-8')
    run('parcial-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                        '--no-artifact-cache', '-o', 'app.exe'], partial)
    if run('parcial-run', [str(partial / 'app.exe')], partial, expected=None).returncode != 42:
        raise SystemExit(f'parcial: NativeSharp no devuelve 42; evidencia: {root}')
    (partial / 'Main.ns').write_text('int Main() { return T16.plain(36) + T16.started(); }\n', encoding='utf-8')
    output = run('parcial-negativo', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                      '--no-artifact-cache', '-o', 'app2.exe'], partial, expected=1).stdout
    if "in its linked member 'started.obj'" not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'parcial: el miembro con inicializadores no se nombra; evidencia: {root}')
    print('parcial: sin --crt-startup enlaza si no se usa el miembro con inicializadores (42) y lo nombra si se usa',
          flush=True)
print('OK: arranque, constructores y destructores globales, frente a Clang; evidencia:', root)
