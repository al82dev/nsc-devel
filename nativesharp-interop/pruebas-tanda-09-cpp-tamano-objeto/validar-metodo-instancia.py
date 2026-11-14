"""Compara tamaños ocultos en métodos C++ de instancia contra Clang."""
import argparse
import os
import shutil
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
parser.add_argument('--precompilado', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-instance-object-size-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                   + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(log))
    return result.stdout

if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')

header = ['#pragma once', '#if defined(T09_EXPORT)', '#define API __declspec(dllexport)',
          '#elif defined(T09_IMPORT)', '#define API __declspec(dllimport)', '#else', '#define API', '#endif',
          'namespace T09Instance {', 'struct ArrayPair { int values[2]; };', 'struct Probe {', 'int marker;']
implementation = ['#include "api.h"', 'namespace T09Instance {', 'static Probe object{7};',
                  'Probe* make() { return &object; }']
ns_tests, cpp_tests = [], []
for dynamic in (False, True):
    for mode in range(4):
        name = ('dynamic' if dynamic else 'size') + str(mode)
        attr = 'pass_dynamic_object_size' if dynamic else 'pass_object_size'
        builtin = '__builtin_dynamic_object_size' if dynamic else '__builtin_object_size'
        header.append(f'API int {name}(int *p __attribute__(({attr}({mode}))), bool b) const;')
        implementation.append(f'int Probe::{name}(int *const p __attribute__(({attr}({mode}))), bool b) const '
                              f'{{ return marker + static_cast<int>({builtin}(p, {mode})) + b; }}')
        ns_tests.append(f'if (object->{name}(&value, true) != 12) return {40 + mode};')
        cpp_tests.append(f'if (object->{name}(&value, true) != 12) return {40 + mode};')
for name in ('size1', 'size3', 'dynamic1', 'dynamic3'):
    ns_tests.append(f'if (object->{name}(&pair.values[constantIndex], true) != 12) return 50;')
    cpp_tests.append(f'if (object->{name}(&pair.values[constantIndex], true) != 12) return 50;')
header.extend(['};', 'API Probe* make();', '}'])
implementation.append('}')
for opt in range(4):
    case = root / ('O' + str(opt)); case.mkdir()
    (case / 'api.h').write_text(('\n'.join(header) + '\n'), encoding='utf-8')
    (case / 'api.cpp').write_text(('\n'.join(implementation) + '\n'), encoding='utf-8')
    (case / 'Control.cpp').write_text('#include "api.h"\nint main() { auto object = T09Instance::make(); '
        'int value = 3; T09Instance::ArrayPair pair; const int constantIndex = 1; ' + ' '.join(cpp_tests) + ' return 0; }\n', encoding='utf-8')
    (case / 'Main.ns').write_text('int Main() { var object = T09Instance.make(); int value = 3; T09Instance.ArrayPair pair; const int constantIndex = 1; '
        + ' '.join(ns_tests) + ' return 0; }\n', encoding='utf-8')
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'Fixture.nsproj').write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
        'Library="api.lib"' + runtime + ' /></ItemGroup></Project>', encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    run(f'O{opt}-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []),
                         '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'O{opt}-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                             '/out:api.dll', '/implib:api.lib', 'api.obj'], case)
        path = case / 'api.h'; path.write_text('#define T09_IMPORT 1\n' + path.read_text(encoding='utf-8'), encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', ['clang.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                  '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(f'O{opt}-control-run', [str(case / 'control.exe')], case)
    common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    lines = [line for line in ir.splitlines() if 'size0@Probe@T09Instance' in line]
    if len(lines) != 2 or not all('i1 zeroext' in line for line in lines):
        raise SystemExit(f'O{opt}: firma de instancia inesperada; evidencia: {root}')
    run(f'O{opt}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                           '-O' + str(opt), '-o', 'native.exe'], case)
    run(f'O{opt}-native-run', [str(case / 'native.exe')], case)
    if args.precompilado:
        producer = case / 'producer'; producer.mkdir()
        for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
            shutil.copyfile(case / name, producer / name)
        (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
            'var object = T09Instance.make(); int value = 3; T09Instance.ArrayPair pair; const int constantIndex = 1; '
            + ' '.join(ns_tests) + ' return 0; } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name><OutputType>Library</OutputType>'
            '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" /><CppLibraryReference Header="api.h" '
            'Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' /></ItemGroup></Project>', encoding='utf-8')
        run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
            '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility; consumer.mkdir()
            (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }\n', encoding='utf-8')
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += ('<CppLibraryReference Header="../producer/api.h" Language="C++" Standard="c++20" '
                    'Library="../producer/api.lib"' + (' Runtime="../producer/api.dll"' if args.dll else '') + ' />')
            else:
                (producer / 'api.h').rename(producer / 'api.h.oculto')
            (consumer / 'Consumer.nsproj').write_text('<Project><PropertyGroup><Name>T09Consumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns" />' + references + '</ItemGroup></Project>', encoding='utf-8')
            run(f'O{opt}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'consumer.exe'], consumer)
            run(f'O{opt}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
    print(f'O{opt}: control C++ y NativeSharp pasaron', flush=True)
print('OK: métodos de instancia; evidencia:', root)
