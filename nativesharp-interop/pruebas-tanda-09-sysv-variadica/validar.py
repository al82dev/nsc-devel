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
parser.add_argument('--esperar-punteros-pendientes', action='store_true')
parser.add_argument('--esperar-prefijo-pendiente', action='store_true')
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
                '/export:sysv_varargs', '/export:sysv_many', '/export:sysv_ptrs', '/export:sysv_empty',
                '/export:sysv_floatspill', '/export:sysv_fixed_ptrs', '/export:sysv_return_ptr',
                '/export:sysv_record_small', '/export:sysv_record_mixed', '/export:sysv_record_two', '/export:sysv_record_pressure',
                '/export:sysv_record_sse_pressure', '/export:sysv_record_large',
                '/out:'+str(case/'api.dll'), '/implib:'+str(case/'api-dll.lib'), str(case/'producer.obj')])
            library, project, cflags = 'api-dll.lib', 'Dll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix+'-c-ir', ['clang.exe', *flags, '-std=c11', *cflags, '-S', '-emit-llvm', str(case/'Control.c'), '-o', str(case/'control.ll')])
        ir = (case/'control.ll').read_text(encoding='utf-8')
        if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_varargs\(i32[^\n]*i32[^\n]*double[^\n]*i64', ir):
            raise SystemExit(prefix + ': llamada o promociones C inesperadas; evidencia: ' + str(root))
        for name, pieces in (('sysv_record_small', r'i64 [^,\)]+'),
                             ('sysv_record_mixed', r'double [^,]+, i32 [^,\)]+'),
                             ('sysv_record_two', r'i64 [^,]+, double [^,]+, i32 [^,\)]+')):
            if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@'+name+r'\(i32[^,]+, '+pieces, ir):
                raise SystemExit(prefix+': piezas C inesperadas de '+name+'; evidencia: '+str(root))
        for name in ('sysv_record_pressure', 'sysv_record_sse_pressure', 'sysv_record_large'):
            if not re.search(r'call[^\n]*@'+name+r'\([^\n]*byval\([^\n]*\) align 8', ir):
                raise SystemExit(prefix+': falta byval C de '+name+'; evidencia: '+str(root))
        run(prefix+'-c-obj', ['clang.exe', *flags, '-std=c11', *cflags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
        run(prefix+'-c-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(prefix+'-c-run', [str(case/'control.exe')])
        command = [args.nsc, '--emit-project-oop-exe', str(case/project), '--cpp-clang-root='+args.clang_root,
                   '--library-reference-root', str(case), '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'result.exe')]
        if args.esperar_pendiente or args.esperar_punteros_pendientes or args.esperar_prefijo_pendiente:
            output = run(prefix+'-ns-rechazo', command, expected=1)
            expected = ('sysv_abi currently supports only selected' if args.esperar_prefijo_pendiente else
                        'sysv_abi variadic argument type is not yet supported' if args.esperar_punteros_pendientes else
                        'variadic C++ or non-cdecl functions are not yet projected')
            if expected not in output:
                raise SystemExit(prefix + ': rechazo inesperado; evidencia: ' + str(root))
            print(prefix + ': C paso; NSC aun rechaza la firma', flush=True)
        else:
            if opt == 0:
                for stage in ('hir', 'mir'):
                    for jobs in (1, 4):
                        dump = run(prefix+'-'+stage+'-j'+str(jobs), [args.nsc, '--dump-project-oop-'+stage, str(case/project),
                            '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case), '-j', str(jobs)])
                        for expected in ('cpp-variadic-plan=sysv_empty:1', 'cpp-variadic-plan=sysv_floatspill:10'):
                            if expected not in dump:
                                raise SystemExit(prefix+': falta '+expected+' en '+stage+'; evidencia: '+str(root))
                        if dump.count('cpp-variadic-plan=sysv_varargs:4') < 2:
                            raise SystemExit(prefix+': falta plan en la llamada de metodo o Main en '+stage+'; evidencia: '+str(root))
                        for expected in ('cpp-variadic-plan=sysv_record_small:2', 'cpp-variadic-plan=sysv_record_mixed:2',
                                         'cpp-variadic-plan=sysv_record_two:3', 'cpp-variadic-plan=sysv_record_pressure:7',
                                         'cpp-variadic-plan=sysv_record_sse_pressure:10',
                                         'cpp-variadic-plan=sysv_record_large:2'):
                            if expected not in dump:
                                raise SystemExit(prefix+': falta '+expected+' en '+stage+'; evidencia: '+str(root))
            ns_ir = run(prefix+'-ns-ir', [args.nsc, '--dump-project-oop-llvm', str(case/project),
                '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)])
            for name in ('sysv_varargs', 'sysv_many', 'sysv_ptrs', 'sysv_empty', 'sysv_floatspill',
                         'sysv_record_small', 'sysv_record_mixed', 'sysv_record_two', 'sysv_record_pressure',
                         'sysv_record_sse_pressure', 'sysv_record_large'):
                if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@'+name+r'\(', ns_ir):
                    raise SystemExit(prefix+': falta convencion variadica de '+name+'; evidencia: '+str(root))
            if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(ptr, ptr, \.\.\.\).*@sysv_fixed_ptrs\(', ns_ir):
                raise SystemExit(prefix+': falta convencion del prefijo puntero; evidencia: '+str(root))
            if not re.search(r'call[^\n]*x86_64_sysvcc ptr \(ptr, \.\.\.\).*@sysv_return_ptr\(', ns_ir):
                raise SystemExit(prefix+': falta convencion del retorno puntero; evidencia: '+str(root))
            for name in ('sysv_record_pressure', 'sysv_record_sse_pressure', 'sysv_record_large'):
                if not re.search(r'call[^\n]*@'+name+r'\([^\n]*byval\([^\n]*\) align 8', ns_ir):
                    raise SystemExit(prefix+': falta byval de '+name+'; evidencia: '+str(root))
            for name, pieces in (('sysv_record_small', r'i64 [^,\)]+'),
                                 ('sysv_record_mixed', r'double [^,]+, i32 [^,\)]+'),
                                 ('sysv_record_two', r'i64 [^,]+, double [^,]+, i32 [^,\)]+')):
                if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@'+name+r'\(i32[^,]+, '+pieces, ns_ir):
                    raise SystemExit(prefix+': piezas NS inesperadas de '+name+'; evidencia: '+str(root))
            run(prefix+'-ns', command)
            run(prefix+'-ns-run', [str(case/'result.exe')])
            print(prefix + ': C y NativeSharp pasaron', flush=True)
if args.esperar_pendiente or args.esperar_punteros_pendientes or args.esperar_prefijo_pendiente:
    print('PENDIENTE: solo control C y rechazo', root)
    raise SystemExit(0)
case = root/'estatica-O0'
cachedExe = case/'cached.exe'
cacheCommand = [args.nsc, '--emit-project-oop-exe', str(case/'Static.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case),
    '--artifact-cache='+str(case/'cache'), '--time', '-O0', '-o', str(cachedExe)]
for label, reason in (('cache-fria', 'not-found'), ('cache-caliente', 'hit')):
    cachedExe.unlink(missing_ok=True)
    output = run(label, cacheCommand)
    if 'compilation-bundle-cache: '+reason not in output:
        raise SystemExit(label+': estado de cache inesperado; evidencia: '+str(root))
    run(label+'-run', [str(cachedExe)])
for name in ('Bad.ns', 'Bad.nsproj'):
    shutil.copyfile(fixture/name, case/name)
bad = run('record-rechazado', [args.nsc, '--dump-project-oop-llvm', str(case/'Bad.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)], expected=1)
if "no viable overload of namespace function 'sysv_varargs'" not in bad or 'not accessible' in bad:
    raise SystemExit('El record NativeSharp no se rechazo correctamente; evidencia: '+str(root))
producer, consumer = root/'pre-producer', root/'pre-consumer'
producer.mkdir()
consumer.mkdir()
for name in ('api.h', 'api.lib'):
    shutil.copyfile(case/name, producer/name)
(producer/'Producer.ns').write_text(
    'namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
    'if (sysv_varargs(3, (sbyte)-2, 2.25f, -5000000000L) != 1) return 1; '
    'short s = -1234; T09VarRecord record; record.x = 42; record.y = 0; sbyte b = -12; '
    'if (sysv_ptrs(3, &s, &record, &b) != 1 || s != 1234 || record.x != 43 || b != 12) return 2; '
    'if (sysv_empty(0) != 1 || sysv_floatspill(9, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 3.5) != 1) return 3; '
    's = -1234; record.x = 42; if (sysv_fixed_ptrs(&record, &s, (sbyte)-7, 2.25f) != 1 || s != 1234 || record.x != 43) return 4; '
    'if (sysv_return_ptr(&record, 7) != &record || record.x != 44) return 5; '
    'return 0; } } }', encoding='utf-8')
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
recordProducer, recordConsumer = root/'pre-record-producer', root/'pre-record-consumer'
recordProducer.mkdir()
recordConsumer.mkdir()
for name in ('api.h', 'api.lib'):
    shutil.copyfile(case/name, recordProducer/name)
(recordProducer/'Producer.ns').write_text(
    'namespace T09 { public class RecordBridge { public static int Run<T>(T unused) { '
    'T09VarMixed record; record.x = 2.25; record.y = 7; '
    'return sysv_record_mixed(1, record) == 1 ? 0 : 1; } } }', encoding='utf-8')
(recordProducer/'Producer.nsproj').write_text(
    '<Project><PropertyGroup><Name>SysvVarRecordProducer</Name><OutputType>Library</OutputType></PropertyGroup>'
    '<ItemGroup><Compile Include="Producer.ns"/><CppLibraryReference Header="api.h" Language="C" '
    'Standard="c11" Library="api.lib"/></ItemGroup></Project>', encoding='utf-8')
(recordConsumer/'Main.ns').write_text('int Main() { return T09.RecordBridge.Run<int>(0); }', encoding='utf-8')
(recordConsumer/'Consumer.nsproj').write_text(
    '<Project><PropertyGroup><Name>SysvVarRecordConsumer</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns"/><LibraryReference Include="../pre-record-producer/Producer.lib"/>'
    '</ItemGroup></Project>', encoding='utf-8')
run('pre-record-producer', [args.nsc, '--emit-project-oop-lib', str(recordProducer/'Producer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(recordProducer/'Producer.lib')])
(recordProducer/'api.h').rename(recordProducer/'api.h.oculto')
recordPending = run('pre-record-pendiente', [args.nsc, '--emit-project-oop-exe', str(recordConsumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(recordConsumer/'Consumer.exe')], expected=1)
if 'sysv_abi variadic argument type is not yet supported' not in recordPending:
    raise SystemExit('El limite de plantilla con record no se detecto correctamente; evidencia: '+str(root))
print('OK: rechazo de record NativeSharp y plantilla escalar precompilada; record precompilado pendiente; evidencia:', root)
print('OK: ocho comparaciones C/NS', root)
