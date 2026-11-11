"""Contraste del puntero SysV en estatica y DLL, O0 a O3."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-indirecta-matriz-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+
        str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label+': fallo; evidencia: '+str(root))
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
    for opt in range(4):
        prefix = mode+'-O'+str(opt)
        case = root/prefix
        case.mkdir()
        for name in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns', 'Static.nsproj', 'Dll.nsproj'):
            shutil.copyfile(fixture/name, case/name)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O'+str(opt)]
        run(prefix+'-productor', ['clang.exe', *flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
        if mode == 'estatica':
            run(prefix+'-biblioteca', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'api.obj')])
            library, project, cflags = 'api.lib', 'Static.nsproj', []
        else:
            run(prefix+'-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                '/export:t09_get_sysv_callback', '/out:'+str(case/'api.dll'),
                '/implib:'+str(case/'api-dll.lib'), str(case/'api.obj')])
            library, project, cflags = 'api-dll.lib', 'Dll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix+'-control-obj', ['clang.exe', *flags, *cflags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
        run(prefix+'-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(prefix+'-control-run', [str(case/'control.exe')])
        common = [str(case/project), '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)]
        ir = run(prefix+'-ns-ir', [args.nsc, '--dump-project-oop-llvm', *common])
        if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(i32', ir):
            raise SystemExit(prefix+': falta convencion indirecta; evidencia: '+str(root))
        run(prefix+'-ns-build', [args.nsc, '--emit-project-oop-exe', *common,
            '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'ns.exe')])
        run(prefix+'-ns-run', [str(case/'ns.exe')])
        print(prefix+': C y NativeSharp pasaron', flush=True)
producer, consumer = root/'pre-producer', root/'pre-consumer'
producer.mkdir()
consumer.mkdir()
for name in ('api.h', 'api.lib'):
    shutil.copyfile(root/'estatica-O0'/name, producer/name)
(producer/'Producer.ns').write_text(
    'namespace T09 { public class IndirectBridge { public static int Run<T>(T unused) { '
    'T09SysvCallback callback = t09_get_sysv_callback(); '
    'if (callback(1, 2, 3, 4, 5, 6) != 91) return 1; return 0; } } }', encoding='utf-8')
(producer/'Producer.nsproj').write_text(
    '<Project><PropertyGroup><Name>T09IndirectProducer</Name><OutputType>Library</OutputType>'
    '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/><CppLibraryReference '
    'Header="api.h" Language="C" Standard="c11" Library="api.lib"/></ItemGroup></Project>', encoding='utf-8')
(consumer/'Main.ns').write_text('int Main() { return T09.IndirectBridge.Run<int>(0); }', encoding='utf-8')
(consumer/'Consumer.nsproj').write_text(
    '<Project><PropertyGroup><Name>T09IndirectConsumer</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns"/><LibraryReference Include="../pre-producer/Producer.lib"/>'
    '</ItemGroup></Project>', encoding='utf-8')
run('pre-producer', [args.nsc, '--emit-project-oop-lib', str(producer/'Producer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(producer/'Producer.lib')])
(producer/'api.h').rename(producer/'api.h.oculto')
ir = run('pre-ir', [args.nsc, '--dump-project-oop-llvm', str(consumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root])
if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(i32', ir):
    raise SystemExit('Falta convencion indirecta precompilada; evidencia: '+str(root))
run('pre-consumer', [args.nsc, '--emit-project-oop-exe', str(consumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(consumer/'Consumer.exe')])
run('pre-run', [str(consumer/'Consumer.exe')])
print('OK: ocho comparaciones C/NativeSharp y plantilla precompilada; evidencia:', root, flush=True)
