"""T09-003: compara llamadas C sysv_abi enteras desde Clang y NativeSharp."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--solo-c', action='store_true')
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-prueba-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)


def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text(
        'COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' +
        result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
    for opt in range(4):
        prefix = mode + '-O' + str(opt)
        case = root / prefix
        case.mkdir()
        for name in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns', 'Static.nsproj', 'Dll.nsproj'):
            shutil.copyfile(fixture / name, case / name)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O' + str(opt)]
        run(prefix + '-productor', ['clang.exe', *flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
        if mode == 'estatica':
            run(prefix + '-archivo', ['llvm-lib.exe', '/nologo', '/out:' + str(case/'api.lib'), str(case/'api.obj')])
            library, project, cflags = 'api.lib', 'Static.nsproj', []
        else:
            run(prefix + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                '/export:sysv_mix', '/export:sysv_set', '/export:sysv_fp', '/export:sysv_float',
                '/export:sysv_pointer', '/export:sysv_wide', '/export:sysv_unsigned',
                '/export:sysv_unsigned64', '/export:sysv_value,DATA',
                '/out:' + str(case/'api.dll'), '/implib:' + str(case/'api-dll.lib'), str(case/'api.obj')])
            library, project, cflags = 'api-dll.lib', 'Dll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix + '-control-obj', ['clang.exe', *flags, *cflags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
        run(prefix + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
            '/nodefaultlib', '/out:' + str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(prefix + '-control-run', [str(case/'control.exe')])
        if args.solo_c:
            print(prefix + ': control C paso', flush=True)
            continue
        output = case/'out'/'Sysv.exe'
        output.parent.mkdir()
        common = [str(case/project), '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(case)]
        ir = run(prefix + '-llvm', [args.nsc, '--dump-project-oop-llvm', *common])
        for name in ('sysv_mix', 'sysv_set', 'sysv_fp', 'sysv_float', 'sysv_pointer',
                     'sysv_wide', 'sysv_unsigned', 'sysv_unsigned64'):
            for kind in ('declare', 'call'):
                if not re.search(kind + r'[^\n]*x86_64_sysvcc[^\n]*@' + name + r'\(', ir):
                    raise SystemExit(prefix + ': falta convencion en ' + kind + ' ' + name + '; evidencia: ' + str(root))
        run(prefix + '-nsc', [args.nsc, '--emit-project-oop-exe', *common,
            '--no-artifact-cache', '-O' + str(opt), '-o', str(output)])
        run(prefix + '-ns-run', [str(output)])
        print(prefix + ': C y NativeSharp pasaron', flush=True)
if args.solo_c:
    print('OK: ocho controles C; evidencia:', root)
    raise SystemExit(0)
producer, consumer = root/'pre-producer', root/'pre-consumer'
producer.mkdir()
consumer.mkdir()
for name in ('api.h', 'api.lib'):
    shutil.copyfile(root/'estatica-O0'/name, producer/name)
(producer/'Producer.ns').write_text(
    'namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
    'if (sysv_mix(1, 2, 3, 4, 5, 6) != 91) return 1; sysv_set(41); '
    'if (sysv_value != 41) return 2; if (sysv_fp(1.5, 2.25f, 3, 4.0) != 10.75) return 3; '
    'if (sysv_float(2.5f) != 3.5f) return 4; int value = 10; '
    'int* result = sysv_pointer(&value, 3); '
    'if (result != &value || value != 13 || *result != 13) return 5; '
    'if (sysv_wide(-5000000000L, (uint)7) != -4999999993L) return 6; '
    'if (sysv_unsigned((uint)4000000000) != (uint)4000000007) return 7; '
    'if (sysv_unsigned64((ulong)100) != (ulong)101) return 8; return 0; } } }', encoding='utf-8')
(producer/'Producer.nsproj').write_text(
    '<Project><PropertyGroup><Name>T09SysvProducer</Name><OutputType>Library</OutputType>'
    '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/><CppLibraryReference '
    'Header="api.h" Language="C" Standard="c11" Library="api.lib"/></ItemGroup></Project>', encoding='utf-8')
(consumer/'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }', encoding='utf-8')
(consumer/'Consumer.nsproj').write_text(
    '<Project><PropertyGroup><Name>T09SysvConsumer</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns"/><LibraryReference Include="../pre-producer/Producer.lib"/>'
    '</ItemGroup></Project>', encoding='utf-8')
run('pre-producer', [args.nsc, '--emit-project-oop-lib', str(producer/'Producer.nsproj'),
    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', str(producer/'Producer.lib')])
(producer/'api.h').rename(producer/'api.h.oculto')
run('pre-consumer', [args.nsc, '--emit-project-oop-exe', str(consumer/'Consumer.nsproj'),
    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', str(consumer/'Consumer.exe')])
run('pre-run', [str(consumer/'Consumer.exe')])
print('OK: plantilla precompilada sin encabezado a O0', flush=True)
case = root/'estatica-O0'
for name in ('bad.h', 'Bad.ns', 'Bad.nsproj'):
    shutil.copyfile(fixture/name, case/name)
command = [args.nsc, '--dump-project-oop-llvm', str(case/'Bad.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(case)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'no-admitido.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'sysv_abi currently supports only' not in result.stdout:
    raise SystemExit('La firma no soportada no fue rechazada; evidencia: ' + str(root))
print('OK: agregado sysv_abi rechazado hasta validar su ABI', flush=True)
for name in ('BadCallback.ns', 'BadCallback.nsproj'):
    shutil.copyfile(fixture/name, case/name)
command = [args.nsc, '--dump-project-oop-llvm', str(case/'BadCallback.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(case)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'callback-no-admitido.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'conversion of a sysv_abi function to a delegate is not yet supported' not in result.stdout:
    raise SystemExit('Conversion a callback no fue rechazada correctamente; evidencia: ' + str(root))
print('OK: conversion de sysv_abi a callback rechazada', flush=True)
print('OK: ocho comparaciones sysv_abi; evidencia:', root)
