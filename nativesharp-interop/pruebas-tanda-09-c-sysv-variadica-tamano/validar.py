"""Contrasta llamadas C SysV variadicas con argumento oculto de tamano."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-sysv-var-size-', dir='A:/temp'))
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
        producer = case / 'producer'
        producer.mkdir(parents=True)
        for name in ('api.h', 'api.c', 'Control.c', 'Main.ns', 'Producer.ns'):
            shutil.copyfile(source / name, producer / name)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O' + str(opt)]
        run(prefix + '-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if mode == 'dll' else []),
                              '-c', 'api.c', '-o', 'api.obj'], producer)
        if mode == 'dll':
            run(prefix + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                                  '/out:api.dll', '/implib:api.lib', 'api.obj'], producer)
            header = producer / 'api.h'
            header.write_text('#define T09_IMPORT 1\n' + header.read_text(encoding='utf-8'), encoding='utf-8')
        else:
            run(prefix + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], producer)
        run(prefix + '-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], producer)
        run(prefix + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                       '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], producer)
        run(prefix + '-control-run', [str(producer / 'control.exe')], producer)
        runtime = ' Runtime="api.dll"' if mode == 'dll' else ''
        reference = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="api.lib"' + runtime + ' />'
        (producer / 'Direct.nsproj').write_text('<Project><PropertyGroup><Name>Direct</Name></PropertyGroup>'
            '<ItemGroup><Compile Include="Main.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')
        common = ['Direct.nsproj', '--library-reference-root', str(producer), '--cpp-clang-root=' + args.clang_root]
        ir = run(prefix + '-ir', [args.nsc, '--dump-project-oop-llvm', *common], producer)
        lines = [line for line in ir.splitlines() if '@t09_csum(' in line]
        if len(lines) != 2 or not all('x86_64_sysvcc' in line and 'i8 signext' in line and 'i1 zeroext' in line for line in lines):
            raise SystemExit(prefix + ': firma o atributos incorrectos; evidencia: ' + str(root))
        run(prefix + '-direct-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                                      '-O' + str(opt), '-o', str(producer / 'direct.exe')], producer)
        run(prefix + '-direct-run', [str(producer / 'direct.exe')], producer)
        (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
            + reference + '</ItemGroup></Project>', encoding='utf-8')
        run(prefix + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
            '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
            '-o', str(producer / 'Producer.lib')], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility
            consumer.mkdir()
            shutil.copyfile(source / 'Consumer.ns', consumer / 'Consumer.ns')
            refs = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                refs += ('<CppLibraryReference Header="../producer/api.h" Language="C" Standard="c11" '
                         'Library="../producer/api.lib"' + (' Runtime="../producer/api.dll"' if mode == 'dll' else '') + ' />')
            else:
                (producer / 'api.h').rename(producer / 'api.h.oculto')
            (consumer / 'Consumer.nsproj').write_text('<Project><PropertyGroup><Name>T09Consumer</Name>'
                '</PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />' + refs + '</ItemGroup></Project>', encoding='utf-8')
            label = prefix + '-' + visibility
            run(label + '-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
                '-o', str(consumer / 'Consumer.exe')], consumer)
            run(label + '-run', [str(consumer / 'Consumer.exe')], consumer)
        print(prefix + ': control C y tres consumidores NativeSharp terminaron con codigo 0', flush=True)
print('OK: ocho controles C, ocho llamadas directas y dieciseis consumidores precompilados; evidencia:', root)
