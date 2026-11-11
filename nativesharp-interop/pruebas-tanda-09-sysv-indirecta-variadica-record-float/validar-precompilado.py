"""Comprueba consumo precompilado del record por puntero SysV sin header."""
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
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-indirecta-record-float-pre-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
        str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
producer, consumer = root / 'producer', root / 'consumer'
producer.mkdir()
consumer.mkdir()
for name in ('api.h', 'api.c', 'producer.s'):
    shutil.copyfile(fixture / name, producer / name)
run('c-producer', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O0',
    '-c', str(producer / 'api.c'), '-o', str(producer / 'api.obj')])
run('c-asm', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-c', str(producer / 'producer.s'), '-o', str(producer / 'producer.obj')])
run('c-library', ['llvm-lib.exe', '/nologo', '/out:' + str(producer / 'api.lib'), str(producer / 'api.obj'), str(producer / 'producer.obj')])
(producer / 'Producer.ns').write_text(
    'namespace T09 { public class RecordBridge { public static int Run<T>(T unused) { '
    'T09IndirectVarRecord record; record.x = 2.25; record.y = 7; '
    'float scalar = 3.5f; '
    'T09IndirectVarCallback callback = t09_get_var_record_callback(); '
    'if (callback(1, record, scalar) != 1 || record.x != 2.25 || record.y != 7) return 1; '
    'return 0; } } }', encoding='utf-8')
(producer / 'Producer.nsproj').write_text(
    '<Project><PropertyGroup><Name>T09RecordProducer</Name><OutputType>Library</OutputType>'
    '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/><CppLibraryReference '
    'Header="api.h" Language="C" Standard="c11" Library="api.lib"/></ItemGroup></Project>', encoding='utf-8')
(consumer / 'Main.ns').write_text('int Main() { return T09.RecordBridge.Run<int>(0); }', encoding='utf-8')
(consumer / 'Consumer.nsproj').write_text(
    '<Project><PropertyGroup><Name>T09RecordConsumer</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns"/><LibraryReference Include="../producer/Producer.lib"/>'
    '</ItemGroup></Project>', encoding='utf-8')
run('producer', [args.nsc, '--emit-project-oop-lib', str(producer / 'Producer.nsproj'),
    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', str(producer / 'Producer.lib')])
(producer / 'api.h').rename(producer / 'api.h.oculto')
for jobs in (1, 4):
    ir = run('consumer-ir-j' + str(jobs), [args.nsc, '--dump-project-oop-llvm',
        str(consumer / 'Consumer.nsproj'), '--cpp-clang-root=' + args.clang_root, '-j', str(jobs)])
    if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+\(i32,\s*\.\.\.\)\s+%[^\(]+\(i32\s+[^,]+,\s*double\s+[^,]+,\s*i32\s+[^,]+,\s*double\s+', ir):
        raise SystemExit('El consumidor precompilado no conserva las piezas SysV; evidencia: ' + str(root))
for opt in range(4):
    executable = consumer / ('Consumer-O' + str(opt) + '.exe')
    run('consumer-O' + str(opt), [args.nsc, '--emit-project-oop-exe', str(consumer / 'Consumer.nsproj'),
        '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', str(executable)])
    run('execute-O' + str(opt), [str(executable)])
cached_executable = consumer / 'Consumer-cache.exe'
cache_command = [args.nsc, '--emit-project-oop-exe', str(consumer / 'Consumer.nsproj'),
    '--cpp-clang-root=' + args.clang_root, '--artifact-cache=' + str(root / 'artifact-cache'),
    '--time', '-O0', '-o', str(cached_executable)]
for expected in ('not-found', 'hit'):
    cached_executable.unlink(missing_ok=True)
    output = run('cache-' + expected, cache_command)
    if 'compilation-bundle-cache: ' + expected not in output:
        raise SystemExit('Cache inesperada: ' + expected + '; evidencia: ' + str(root))
    run('cache-execute-' + expected, [str(cached_executable)])
dll_producer, dll_consumer = root / 'dll-producer', root / 'dll-consumer'
dll_producer.mkdir()
dll_consumer.mkdir()
shutil.copyfile(producer / 'api.obj', dll_producer / 'api.obj')
shutil.copyfile(producer / 'producer.obj', dll_producer / 'producer.obj')
shutil.copyfile(producer / 'Producer.ns', dll_producer / 'Producer.ns')
run('dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/export:t09_get_var_record_callback',
    '/out:' + str(dll_producer / 'api.dll'), '/implib:' + str(dll_producer / 'api-dll.lib'), str(dll_producer / 'api.obj'), str(dll_producer / 'producer.obj')])
(dll_producer / 'api-dll.h').write_text((producer / 'api.h.oculto').read_text(encoding='utf-8').replace(
    'T09IndirectVarCallback t09_get_var_record_callback',
    '__declspec(dllimport) T09IndirectVarCallback t09_get_var_record_callback'), encoding='utf-8')
(dll_producer / 'Producer.nsproj').write_text((producer / 'Producer.nsproj').read_text(encoding='utf-8').replace(
    'Header="api.h"', 'Header="api-dll.h"').replace('Library="api.lib"',
    'Library="api-dll.lib" Runtime="api.dll"'), encoding='utf-8')
shutil.copyfile(consumer / 'Main.ns', dll_consumer / 'Main.ns')
(dll_consumer / 'Consumer.nsproj').write_text((consumer / 'Consumer.nsproj').read_text(encoding='utf-8').replace(
    '../producer/Producer.lib', '../dll-producer/Producer.lib'), encoding='utf-8')
run('dll-producer', [args.nsc, '--emit-project-oop-lib', str(dll_producer / 'Producer.nsproj'),
    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', str(dll_producer / 'Producer.lib')])
(dll_producer / 'api-dll.h').rename(dll_producer / 'api-dll.h.oculto')
for opt in range(4):
    executable = dll_consumer / ('Consumer-O' + str(opt) + '.exe')
    run('dll-consumer-O' + str(opt), [args.nsc, '--emit-project-oop-exe', str(dll_consumer / 'Consumer.nsproj'),
        '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', str(executable)])
    run('dll-execute-O' + str(opt), [str(executable)])
negative = root / 'negative'
negative_producer, negative_consumer = negative / 'producer', negative / 'consumer'
negative_producer.mkdir(parents=True)
negative_consumer.mkdir()
for name in ('api.c', 'api.lib', 'Producer.ns', 'Producer.nsproj'):
    shutil.copyfile(producer / name, negative_producer / name)
(negative_producer / 'api.h').write_text((producer / 'api.h.oculto').read_text(encoding='utf-8').replace(
    'double x; int y;', 'double x; unsigned int y;'), encoding='utf-8')
for name in ('Main.ns', 'Consumer.nsproj'):
    shutil.copyfile(consumer / name, negative_consumer / name)
run('negative-producer', [args.nsc, '--emit-project-oop-lib', str(negative_producer / 'Producer.nsproj'),
    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', str(negative_producer / 'Producer.lib')])
(negative_producer / 'api.h').rename(negative_producer / 'api.h.oculto')
command = [args.nsc, '--emit-project-oop-exe', str(negative_consumer / 'Consumer.nsproj'),
    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', str(negative_consumer / 'Consumer.exe')]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'negative-consumer.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'sysv_abi function pointer signature is not yet supported' not in result.stdout:
    raise SystemExit('La variante no verificada no fue rechazada; evidencia: ' + str(root))
print('OK: record indirecto precompilado sin header; evidencia:', root, flush=True)
