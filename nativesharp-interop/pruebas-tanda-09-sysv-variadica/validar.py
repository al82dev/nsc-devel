"""Contrasta llamadas SysV variadicas con el control de registros del productor."""
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
parser.add_argument('--esperar-pendiente', action='store_true')
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-varargs-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command, expected=0):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
        str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode != expected:
        raise SystemExit(label + ': codigo inesperado; evidencia: ' + str(root) + '\n' + result.stdout)
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
    for opt in range(4):
        prefix = mode + '-O' + str(opt)
        case = root / prefix
        case.mkdir()
        for name in ('api.h', 'api-dll.h', 'producer.s', 'Control.c', 'Main.ns', 'Static.nsproj', 'Dll.nsproj'):
            shutil.copyfile(fixture / name, case / name)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-O' + str(opt)]
        run(prefix+'-producer', ['clang.exe', *flags, '-c', str(case/'producer.s'), '-o', str(case/'producer.obj')])
        if mode == 'estatica':
            run(prefix+'-library', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'producer.obj')])
            library, project, cflags = 'api.lib', 'Static.nsproj', []
        else:
            run(prefix+'-library', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                '/export:sysv_varargs', '/export:sysv_many', '/out:'+str(case/'api.dll'), '/implib:'+str(case/'api-dll.lib'), str(case/'producer.obj')])
            library, project, cflags = 'api-dll.lib', 'Dll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix+'-c-ir', ['clang.exe', *flags, '-std=c11', *cflags, '-S', '-emit-llvm', str(case/'Control.c'), '-o', str(case/'control.ll')])
        ir = (case/'control.ll').read_text(encoding='utf-8')
        if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_varargs\(i32[^\n]*i32[^\n]*double[^\n]*i64', ir):
            raise SystemExit(prefix + ': llamada o promociones C inesperadas; evidencia: ' + str(root))
        run(prefix+'-c-obj', ['clang.exe', *flags, '-std=c11', *cflags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
        run(prefix+'-c-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(prefix+'-c-run', [str(case/'control.exe')])
        command = [args.nsc, '--emit-project-oop-exe', str(case/project), '--cpp-clang-root='+args.clang_root,
                   '--library-reference-root', str(case), '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'result.exe')]
        if args.esperar_pendiente:
            output = run(prefix+'-ns-rechazo', command, expected=1)
            if 'variadic C++ or non-cdecl functions are not yet projected' not in output:
                raise SystemExit(prefix + ': rechazo inesperado; evidencia: ' + str(root))
            print(prefix + ': C paso; NSC aun rechaza la firma', flush=True)
        else:
            ns_ir = run(prefix+'-ns-ir', [args.nsc, '--dump-project-oop-llvm', str(case/project),
                '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)])
            for name in ('sysv_varargs', 'sysv_many'):
                if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@'+name+r'\(', ns_ir):
                    raise SystemExit(prefix+': falta convencion variadica de '+name+'; evidencia: '+str(root))
            run(prefix+'-ns', command)
            run(prefix+'-ns-run', [str(case/'result.exe')])
            print(prefix + ': C y NativeSharp pasaron', flush=True)
if args.esperar_pendiente:
    print('PENDIENTE: solo control C y rechazo', root)
    raise SystemExit(0)
case = root/'estatica-O0'
for name in ('Bad.ns', 'Bad.nsproj'):
    shutil.copyfile(fixture/name, case/name)
bad = run('record-rechazado', [args.nsc, '--dump-project-oop-llvm', str(case/'Bad.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)], expected=1)
if 'sysv_abi variadic argument type is not yet supported' not in bad:
    raise SystemExit('El record variadico no se rechazo correctamente; evidencia: '+str(root))
producer, consumer = root/'pre-producer', root/'pre-consumer'
producer.mkdir()
consumer.mkdir()
for name in ('api.h', 'api.lib'):
    shutil.copyfile(case/name, producer/name)
(producer/'Producer.ns').write_text(
    'namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
    'return sysv_varargs(3, (sbyte)-2, 2.25f, -5000000000L) == 1 ? 0 : 1; } } }', encoding='utf-8')
(producer/'Producer.nsproj').write_text(
    '<Project><PropertyGroup><Name>SysvVarProducer</Name><OutputType>Library</OutputType></PropertyGroup>'
    '<ItemGroup><Compile Include="Producer.ns"/><CppLibraryReference Header="api.h" Language="C" '
    'Standard="c11" Library="api.lib"/></ItemGroup></Project>', encoding='utf-8')
(consumer/'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }', encoding='utf-8')
(consumer/'Consumer.nsproj').write_text(
    '<Project><PropertyGroup><Name>SysvVarConsumer</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns"/><LibraryReference Include="../pre-producer/Producer.lib"/>'
    '</ItemGroup></Project>', encoding='utf-8')
run('pre-producer', [args.nsc, '--emit-project-oop-lib', str(producer/'Producer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(producer/'Producer.lib')])
(producer/'api.h').rename(producer/'api.h.oculto')
run('pre-consumer', [args.nsc, '--emit-project-oop-exe', str(consumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(consumer/'Consumer.exe')])
run('pre-run', [str(consumer/'Consumer.exe')])
print('OK: rechazo de record y plantilla precompilada sin encabezado; evidencia:', root)
print('OK: ocho comparaciones C/NS', root)
