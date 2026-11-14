"""Compara funciones C con tamanos ocultos en enlace estatico y DLL."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-object-size-', dir='A:/temp'))
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
for mode in ('estatica', 'dll'):
    for opt in range(4):
        prefix = mode + '-O' + str(opt)
        case = root / prefix
        case.mkdir()
        for name in ('api.h', 'api.c', 'Main.ns', 'Control.c'):
            shutil.copyfile(source / name, case / name)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O' + str(opt)]
        run(prefix + '-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if mode == 'dll' else []),
                             '-c', 'api.c', '-o', 'api.obj'], case)
        if mode == 'dll':
            run(prefix + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                                   '/out:api.dll', '/implib:api.lib', 'api.obj'], case)
            header = case / 'api.h'
            header.write_text('#define T09_IMPORT 1\n' + header.read_text(encoding='utf-8'), encoding='utf-8')
        else:
            run(prefix + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        run(prefix + '-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], case)
        run(prefix + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                      '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
        run(prefix + '-control-run', [str(case / 'control.exe')], case)
        runtime = ' Runtime="api.dll"' if mode == 'dll' else ''
        (case / 'Fixture.nsproj').write_text(
            '<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="api.lib"' + runtime + ' />'
            '</ItemGroup></Project>', encoding='utf-8')
        common = ['Fixture.nsproj', '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root]
        run(prefix + '-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                                '-O' + str(opt), '-o', str(case / 'native.exe')], case)
        run(prefix + '-native-run', [str(case / 'native.exe')], case)
        print(prefix + ': control C y NativeSharp terminaron con codigo 0', flush=True)
print('OK: ocho comparaciones; evidencia:', root)
