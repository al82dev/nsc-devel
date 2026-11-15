"""T16-014, T16-015 y T16-016: enlace separado, simbolos duplicados y LTO.

Dos unidades C++ en una biblioteca y una tercera en otra usan una funcion inline del encabezado (COMDAT), un
modulo NativeSharp precompilado (`--emit-project-oop-lib`) las llama y el ejecutable consume el modulo y las
bibliotecas, con NSLink y con lld-link. Una biblioteca que define otra vez una funcion de la primera se rechaza
al enlazar con el nombre del simbolo duplicado, con ambos enlazadores (en estatica). Una biblioteca compilada con
`-flto=thin` contiene bitcode: NSC elige lld-link cuando no se fija el enlazador y NSLink la rechaza. Compara
con un control C++ de Clang 23.1.0, O0-O3; con --dll la primera biblioteca es una DLL.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t16-enlace-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
env.pop('NSC_LINKER', None)
print('Evidencia:', root, flush=True)
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
inline int shared(int x) noexcept { return x * 3; }
API int first(int x) noexcept;
API int second(int x) noexcept;
int third(int x) noexcept;
}
'''
UNIT_A = '#include "api.h"\nnamespace T16 { int first(int x) noexcept { return shared(x) + 1; } int clash() noexcept { return 1; } }\n'
UNIT_B = '#include "api.h"\nnamespace T16 { int second(int x) noexcept { return shared(x) + 2; } }\n'
UNIT_C = '#include "api.h"\nnamespace T16 { int third(int x) noexcept { return shared(x) + 3; } }\n'
DUP_HEADER = '#pragma once\nnamespace D16 { int extra(int x) noexcept; }\n'
UNIT_DUP = '#include "dup.h"\nnamespace D16 { int extra(int x) noexcept { return x; } }\nnamespace T16 { int clash() noexcept { return 2; } }\n'
LTO_HEADER = '#pragma once\nnamespace L16 { int twice(int x) noexcept; }\n'
LTO_UNIT = '#include "lto.h"\nnamespace L16 { int twice(int x) noexcept { return x * 2; } }\n'
CONTROL = '''#include "api.h"
static int producer(int x) { return T16::first(x) + T16::second(x) + T16::shared(x); }
int main() { return producer(2) + T16::third(3) + T16::shared(1); }
'''
PRODUCER = 'namespace P16 { public class Lib { public static int Use(int x) { return T16.first(x) + T16.second(x) + T16.shared(x); } } }\n'
CONSUMER = 'int Main() { return P16.Lib.Use(2) + T16.third(3) + T16.shared(1); }\n'
EXPECTED = (7 + 8 + 6) + 12 + 3


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


def project(name, references, source, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def references(prefix, extra=''):
    runtime = ' Runtime="' + prefix + 'first.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="' + prefix +
            'first.lib"' + runtime + ' /><CppLibraryReference Header="' + prefix + 'api.h" Language="C++" '
            'Standard="c++20" Library="' + prefix + extra + 'third.lib" />')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    for name, text in (('api.h', HEADER), ('a.cpp', UNIT_A), ('b.cpp', UNIT_B), ('c.cpp', UNIT_C), ('dup.cpp', UNIT_DUP), ('dup.h', DUP_HEADER),
                       ('Control.cpp', CONTROL)):
        (case / name).write_text(text, encoding='utf-8')
    export = ['-DT16_EXPORT'] if args.dll else []
    for unit in ('a', 'b'):
        run(f'O{opt}-{unit}', [*CXX, '-O' + str(opt), *export, '-c', unit + '.cpp', '-o', unit + '.obj'], case)
    run(f'O{opt}-c', [*CXX, '-O' + str(opt), '-c', 'c.cpp', '-o', 'c.obj'], case)
    run(f'O{opt}-dup', [*CXX, '-O' + str(opt), '-c', 'dup.cpp', '-o', 'dup.obj'], case)
    if args.dll:
        run(f'O{opt}-first', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:first.dll',
                              '/implib:first.lib', 'a.obj', 'b.obj'], case)
        (case / 'api.h').write_text('#define T16_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-first', ['llvm-lib.exe', '/nologo', '/out:first.lib', 'a.obj', 'b.obj'], case)
    run(f'O{opt}-third', ['llvm-lib.exe', '/nologo', '/out:third.lib', 'c.obj'], case)
    run(f'O{opt}-dup-lib', ['llvm-lib.exe', '/nologo', '/out:dup.lib', 'dup.obj'], case)
    run(f'O{opt}-control-obj', [*CXX, '-O' + str(opt), '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                 '/out:control.exe', 'control.obj', 'first.lib', 'third.lib'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'O{opt}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('P16', references('../'), 'Producer.ns', True), encoding='utf-8')
    run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', '--cpp-clang-root=' + args.clang_root,
                             '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer,
        extra_env={'NSC_LINKER': 'nslink.exe'})
    for linker in ('nslink.exe', 'lld-link.exe'):
        consumer = case / ('consumer-' + linker.split('.')[0])
        consumer.mkdir()
        (consumer / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
        (consumer / 'App.nsproj').write_text(
            project('T16App', '<LibraryReference Include="../producer/Producer.lib" />' + references('../'), 'Main.ns'),
            encoding='utf-8')
        if args.dll:
            shutil.copyfile(case / 'first.dll', consumer / 'first.dll')
        tag = f'O{opt}-{linker.split(".")[0]}'
        run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                             '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], consumer,
            extra_env={'NSC_LINKER': linker})
        result = run(tag + '-run', [str(consumer / 'app.exe')], consumer, expected=None).returncode
        if result != EXPECTED:
            raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {EXPECTED}; evidencia: {root}')
        if args.dll:
            print(f'{tag}: NativeSharp y Clang devuelven {result}', flush=True)
            continue
        # Both objects are needed and both define T16::clash. / Ambos objetos se necesitan y ambos definen T16::clash.
        (consumer / 'Dup.ns').write_text('int Main() { return P16.Lib.Use(2) + T16.third(3) + D16.extra(1); }\n',
                                         encoding='utf-8')
        (consumer / 'Dup.nsproj').write_text(
            project('T16Dup', '<LibraryReference Include="../producer/Producer.lib" />' + references('../') +
                    '<CppLibraryReference Header="../dup.h" Language="C++" Standard="c++20" Library="../dup.lib" />',
                    'Dup.ns'), encoding='utf-8')
        output = run(tag + '-duplicado', [args.nsc, '--emit-project-oop-exe', 'Dup.nsproj', '--cpp-clang-root=' + args.clang_root,
                                          '--no-artifact-cache', '-O' + str(opt), '-o', 'dup.exe'], consumer, expected=1,
                     extra_env={'NSC_LINKER': linker}).stdout
        if 'duplicate symbol' not in output or ('?clash@T16@@YAHXZ' not in output and 'T16::clash' not in output):
            print(output[-2000:], flush=True)
            raise SystemExit(f'{tag}: el simbolo duplicado no se rechaza con su nombre; evidencia: {root}')
        print(f'{tag}: NativeSharp y Clang devuelven {result}; el simbolo duplicado se rechaza con su nombre', flush=True)
    # ThinLTO bitcode. / Bitcode ThinLTO.
    lto = case / 'lto'
    lto.mkdir()
    (lto / 'lto.h').write_text(LTO_HEADER, encoding='utf-8')
    (lto / 'lto.cpp').write_text(LTO_UNIT, encoding='utf-8')
    run(f'O{opt}-lto-obj', [*CXX, '-O' + str(opt), '-flto=thin', '-c', 'lto.cpp', '-o', 'lto.obj'], lto)
    run(f'O{opt}-lto-lib', ['llvm-lib.exe', '/nologo', '/out:lto.lib', 'lto.obj'], lto)
    (lto / 'Main.ns').write_text('int Main() { return L16.twice(21); }\n', encoding='utf-8')
    (lto / 'App.nsproj').write_text(project('T16Lto', '<CppLibraryReference Header="lto.h" Language="C++" Standard="c++20" '
                                                      'Library="lto.lib" />', 'Main.ns'), encoding='utf-8')
    run(f'O{opt}-lto-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                              '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], lto)
    if run(f'O{opt}-lto-run', [str(lto / 'app.exe')], lto, expected=None).returncode != 42:
        raise SystemExit(f'O{opt}-lto: NativeSharp no devuelve 42; evidencia: {root}')
    output = run(f'O{opt}-lto-nslink', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                        '--no-artifact-cache', '-O' + str(opt), '-o', 'nslink.exe'], lto, expected=1,
                 extra_env={'NSC_LINKER': 'nslink.exe'}).stdout
    if 'not AMD64' not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'O{opt}-lto: NSLink no rechaza el bitcode con su motivo; evidencia: {root}')
    print(f'O{opt}-lto: biblioteca ThinLTO enlazada con lld-link (42); NSLink la rechaza con su motivo', flush=True)
print('OK: enlace separado, simbolos duplicados y LTO, frente a Clang; evidencia:', root)
