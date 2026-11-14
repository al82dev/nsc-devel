"""Comprueba el atributo desde una plantilla NativeSharp precompilada."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--const-parametros', action='store_true')
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-object-size-pre-', dir='A:/temp'))
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

if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], source):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / ('O' + str(opt))
    producer = case / 'producer'
    producer.mkdir(parents=True)
    for name in ('api.h', 'api.cpp', 'Producer.ns'):
        shutil.copyfile(source / name, producer / name)
    if args.const_parametros:
        header = producer / 'api.h'
        header.write_text(header.read_text(encoding='utf-8').replace(
            'int *p __attribute__', 'int *const p __attribute__').replace(
            'int *q __attribute__', 'int *const q __attribute__'), encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    prefix = 'O' + str(opt)
    run(prefix + '-api', ['clang.exe', *flags, '-c', 'api.cpp', '-o', 'api.obj'], producer)
    run(prefix + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], producer)
    (producer / 'Producer.nsproj').write_text(
        '<Project><PropertyGroup><Name>T09Producer</Name><OutputType>Library</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib" />'
        '</ItemGroup></Project>', encoding='utf-8')
    run(prefix + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
        '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
        '-o', str(producer / 'Producer.lib')], producer)
    for visibility in ('con-encabezado', 'sin-encabezado'):
        consumer = case / visibility
        consumer.mkdir()
        shutil.copyfile(source / 'Consumer.ns', consumer / 'Consumer.ns')
        refs = '<LibraryReference Include="../producer/Producer.lib" />'
        if visibility == 'con-encabezado':
            refs += ('<CppLibraryReference Header="../producer/api.h" Language="C++" '
                     'Standard="c++20" Library="../producer/api.lib" />')
        else:
            (producer / 'api.h').rename(producer / 'api.h.oculto')
        (consumer / 'Consumer.nsproj').write_text(
            '<Project><PropertyGroup><Name>T09Consumer</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Consumer.ns" />' + refs + '</ItemGroup></Project>', encoding='utf-8')
        label = prefix + '-' + visibility
        run(label + '-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
            '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
            '-o', str(consumer / 'Consumer.exe')], consumer)
        run(label + '-run', [str(consumer / 'Consumer.exe')], consumer)
        print(label + ': codigo 0', flush=True)
print('OK: ocho consumidores precompilados; evidencia:', root)
