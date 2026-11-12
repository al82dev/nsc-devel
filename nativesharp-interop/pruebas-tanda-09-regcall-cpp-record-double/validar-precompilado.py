"""Contrasta llamadas C++ regcall desde plantilla NS precompilada."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--solo-puntero', action='store_true')
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-record-double-pre-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=fixture, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root/(label+'.log')
    log.write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label+': fallo; evidencia: '+str(log))
    return result.stdout

if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
    for opt in range(4):
        label = mode+'-O'+str(opt)
        case = root/label
        producer = case/'producer'
        producer.mkdir(parents=True)
        for name in ('api.h', 'api-dll.h', 'api.cpp'):
            shutil.copyfile(fixture/name, producer/name)
        shutil.copyfile(fixture/('ProducerIndirect.ns' if args.solo_puntero else 'Producer.ns'), producer/'Producer.ns')
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O'+str(opt)]
        if mode == 'dll': flags.append('-DAPI_DLL_BUILD')
        run(label+'-c', ['clang.exe', *flags, '-c', str(producer/'api.cpp'), '-o', str(producer/'api.obj')])
        symbols = run(label+'-symbols', ['llvm-nm.exe', '--defined-only', str(producer/'api.obj')])
        exports = []
        for function in ('sum', 'fetch'):
            matches = re.findall(r'\?'+function+r'@T09Wide@@\S+', symbols)
            if len(matches) != 1: raise SystemExit(label+': simbolos inesperados')
            exports.append('/export:'+matches[0])
        if mode == 'estatica':
            header, library, runtime = 'api.h', 'api.lib', ''
            run(label+'-archive', ['llvm-lib.exe', '/nologo', '/out:'+str(producer/library), str(producer/'api.obj')])
        else:
            header, library, runtime = 'api-dll.h', 'api-dll.lib', ' Runtime="api.dll"'
            run(label+'-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                *exports,
                '/out:'+str(producer/'api.dll'), '/implib:'+str(producer/library), str(producer/'api.obj')])
        ref = '<CppLibraryReference Header="'+header+'" Language="C++" Standard="c++20" Library="'+library+'"'+runtime+' />'
        (producer/'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09HfaProducer</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'+ref+
            '</ItemGroup></Project>', encoding='utf-8')
        run(label+'-producer-ns', [args.nsc, '--emit-project-oop-lib', str(producer/'Producer.nsproj'),
            '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(producer/'Producer.lib')])
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case/visibility
            consumer.mkdir()
            shutil.copyfile(fixture/'Consumer.ns', consumer/'Consumer.ns')
            refs = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                refs += '<CppLibraryReference Header="../producer/'+header+'" Language="C++" Standard="c++20" Library="../producer/'+library+'"'+(' Runtime="../producer/api.dll"' if runtime else '')+' />'
            else:
                for name in ('api.h', 'api-dll.h'):
                    (producer/name).rename(producer/(name+'.oculto'))
            (consumer/'Consumer.nsproj').write_text('<Project><PropertyGroup><Name>T09HfaConsumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Consumer.ns" />'+refs+'</ItemGroup></Project>', encoding='utf-8')
            output = consumer/'out'/'Consumer.exe'
            output.parent.mkdir()
            target = label+'-'+visibility
            run(target+'-build', [args.nsc, '--emit-project-oop-exe', str(consumer/'Consumer.nsproj'),
                '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(output)])
            run(target+'-run', [str(output)])
            print(target+': codigo 0', flush=True)
print('OK: 16 consumidores precompilados; evidencia:', root, flush=True)


