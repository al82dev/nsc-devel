"""T09-003: contraste C/NS de un record SysV de 7 bytes."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--esperar-pendiente', action='store_true', help='Reproducir el rechazo actual sin afirmar soporte')
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-r7-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)


def run(label, command, expected=0):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+
                                     '\n'+result.stdout, encoding='utf-8')
    if result.returncode != expected:
        print(result.stdout)
        raise SystemExit(label+': resultado inesperado; evidencia: '+str(root))
    return result.stdout


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
    for opt in (range(1) if args.esperar_pendiente else range(4)):
        case = root/(mode+'-O'+str(opt))
        case.mkdir()
        for name in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns', 'Static.nsproj', 'Dll.nsproj'):
            shutil.copyfile(fixture/name, case/name)
        label = mode+'-O'+str(opt)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O'+str(opt)]
        run(label+'-c', ['clang.exe', *flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
        if mode == 'estatica':
            run(label+'-lib', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'api.obj')])
            library, project, cflags = 'api.lib', 'Static.nsproj', []
        else:
            run(label+'-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                '/export:sysv_r7', '/out:'+str(case/'api.dll'), '/implib:'+str(case/'api-dll.lib'), str(case/'api.obj')])
            library, project, cflags = 'api-dll.lib', 'Dll.nsproj', ['-DAPI_DLL_IMPORT']
        run(label+'-control-obj', ['clang.exe', *flags, *cflags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
        run(label+'-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(label+'-control-run', [str(case/'control.exe')])
        output = case/'out'/'SysvR7.exe'
        output.parent.mkdir()
        common = [str(case/project), '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)]
        if args.esperar_pendiente:
            text = run(label+'-nsc', [args.nsc, '--emit-project-oop-exe', *common,
                '--no-artifact-cache', '-O0', '-o', str(output)], expected=1)
            if 'sysv_abi currently supports only' not in text:
                raise SystemExit(label+': rechazo diferente; evidencia: '+str(root))
            print(label+': control C paso; NSC aun rechaza el caso', flush=True)
            continue
        ir = run(label+'-llvm', [args.nsc, '--dump-project-oop-llvm', *common])
        if 'x86_64_sysvcc i56 @sysv_r7(i56' not in ir:
            raise SystemExit(label+': firma fisica inesperada; evidencia: '+str(root))
        run(label+'-nsc', [args.nsc, '--emit-project-oop-exe', *common,
            '--no-artifact-cache', '-O'+str(opt), '-o', str(output)])
        run(label+'-ns-run', [str(output)])
        print(label+': C y NativeSharp pasaron', flush=True)
if args.esperar_pendiente:
    print('PENDIENTE: el rechazo observado no demuestra soporte; evidencia:', root)
else:
    producer, consumer = root/'pre-producer', root/'pre-consumer'
    producer.mkdir()
    consumer.mkdir()
    for name in ('api.h', 'api.lib'):
        shutil.copyfile(root/'estatica-O0'/name, producer/name)
    (producer/'Producer.ns').write_text(
        'namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
        'T09SysvR7 original; original.x = 1; original.y = 2; original.z = 3; original.w = 4; original.a = 5; original.b = 6; original.c = 7; '
        'T09SysvR7 result = sysv_r7(original); '
        'if (original.x != 1 || original.y != 2 || original.z != 3 || original.w != 4 || original.a != 5 || original.b != 6 || original.c != 7 || result.x != 11 || result.y != 2 || result.z != 3 || result.w != 4 || result.a != 5 || result.b != 6 || result.c != 7) return 1; '
        'return 0; } } }', encoding='utf-8')
    (producer/'Producer.nsproj').write_text(
        '<Project><PropertyGroup><Name>T09SysvR7Producer</Name><OutputType>Library</OutputType>'
        '</PropertyGroup><ItemGroup><Compile Include="Producer.ns"/><CppLibraryReference '
        'Header="api.h" Language="C" Standard="c11" Library="api.lib"/></ItemGroup></Project>', encoding='utf-8')
    (consumer/'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }', encoding='utf-8')
    (consumer/'Consumer.nsproj').write_text(
        '<Project><PropertyGroup><Name>T09SysvR7Consumer</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns"/><LibraryReference Include="../pre-producer/Producer.lib"/>'
        '</ItemGroup></Project>', encoding='utf-8')
    run('pre-producer', [args.nsc, '--emit-project-oop-lib', str(producer/'Producer.nsproj'),
        '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(producer/'Producer.lib')])
    (producer/'api.h').rename(producer/'api.h.oculto')
    run('pre-consumer', [args.nsc, '--emit-project-oop-exe', str(consumer/'Consumer.nsproj'),
        '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(consumer/'Consumer.exe')])
    run('pre-run', [str(consumer/'Consumer.exe')])
    print('OK: plantilla precompilada sin encabezado a O0', flush=True)
    print('OK: ocho comparaciones C/NS; evidencia:', root)
