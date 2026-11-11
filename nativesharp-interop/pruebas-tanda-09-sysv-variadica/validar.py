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
                '/export:sysv_record_ptrs',
                '/export:sysv_record_return_ptr',
                '/export:sysv_record_rptr',
                '/export:sysv_record_fixed_rptr',
                '/export:sysv_record_packed',
                '/export:sysv_record_packed_ptr',
                '/export:sysv_record_array',
                '/export:sysv_record_nested',
                '/export:sysv_record_record_array', '/export:sysv_record_union_outer',
                '/export:sysv_record_record_array_large',
                '/export:sysv_record_bits',
                '/export:sysv_record_bits_gap',
                '/export:sysv_record_bool_bits',
                '/out:'+str(case/'api.dll'), '/implib:'+str(case/'api-dll.lib'), str(case/'producer.obj')])
            library, project, cflags = 'api-dll.lib', 'Dll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix+'-c-ir', ['clang.exe', *flags, '-std=c11', *cflags, '-S', '-emit-llvm', str(case/'Control.c'), '-o', str(case/'control.ll')])
        ir = (case/'control.ll').read_text(encoding='utf-8')
        if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_varargs\(i32[^\n]*i32[^\n]*double[^\n]*i64', ir):
            raise SystemExit(prefix + ': llamada o promociones C inesperadas; evidencia: ' + str(root))
        for name, pieces in (('sysv_record_small', r'i64 [^,\)]+'),
                             ('sysv_record_mixed', r'double [^,]+, i32 [^,\)]+'),
                             ('sysv_record_two', r'i64 [^,]+, double [^,]+, i32 [^,\)]+'),
                             ('sysv_record_array', r'double [^,]+, double [^,\)]+'),
                             ('sysv_record_nested', r'double [^,]+, i64 [^,\)]+'),
                             ('sysv_record_record_array', r'i64 [^,]+, i64 [^,\)]+'),
                             ('sysv_record_union_outer', r'double [^,]+, i64 [^,\)]+'),
                             ('sysv_record_bits', r'i32 [^,\)]+'),
                             ('sysv_record_bits_gap', r'i64 [^,\)]+'),
                             ('sysv_record_bool_bits', r'i16 [^,\)]+')):
            if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@'+name+r'\(i32[^,]+, '+pieces, ir):
                raise SystemExit(prefix+': piezas C inesperadas de '+name+'; evidencia: '+str(root))
        if len(re.findall(r'call[^\n]*x86_64_sysvcc[^\n]*@sysv_record_nested\(i32[^,]+, double [^,]+, i64 [^,\)]+', ir)) < 2:
            raise SystemExit(prefix+': falta la llamada C con campos typedef; evidencia: '+str(root))
        for name in ('sysv_record_pressure', 'sysv_record_sse_pressure', 'sysv_record_large', 'sysv_record_packed',
                     'sysv_record_record_array_large'):
            if not re.search(r'call[^\n]*@'+name+r'\([^\n]*byval\([^\n]*\) align 8', ir):
                raise SystemExit(prefix+': falta byval C de '+name+'; evidencia: '+str(root))
        if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@sysv_record_ptrs\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', ir):
            raise SystemExit(prefix+': mezcla C de punteros y record inesperada; evidencia: '+str(root))
        if not re.search(r'call[^\n]*x86_64_sysvcc ptr \(ptr, \.\.\.\).*@sysv_record_return_ptr\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', ir):
            raise SystemExit(prefix+': retorno C de puntero inesperado; evidencia: '+str(root))
        if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_record_rptr\(i32[^,]+, double [^,]+, i32 [^,]+, ptr ', ir):
            raise SystemExit(prefix+': puntero C a record inesperado; evidencia: '+str(root))
        if not re.search(r'call[^\n]*x86_64_sysvcc ptr \(ptr, \.\.\.\).*@sysv_record_fixed_rptr\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', ir):
            raise SystemExit(prefix+': puntero C fijo/retornado a record inesperado; evidencia: '+str(root))
        if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_record_packed_ptr\(i32[^,]+, double [^,]+, i32 [^,]+, ptr ', ir):
            raise SystemExit(prefix+': puntero C empaquetado inesperado; evidencia: '+str(root))
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
                        if dump.count('cpp-variadic-plan=sysv_record_nested:2') < 2:
                            raise SystemExit(prefix+': falta plan para campos typedef en '+stage+'; evidencia: '+str(root))
                        for expected in ('cpp-variadic-plan=sysv_record_small:2', 'cpp-variadic-plan=sysv_record_mixed:2',
                                         'cpp-variadic-plan=sysv_record_two:3', 'cpp-variadic-plan=sysv_record_pressure:7',
                                         'cpp-variadic-plan=sysv_record_sse_pressure:10',
                                         'cpp-variadic-plan=sysv_record_large:2', 'cpp-variadic-plan=sysv_record_ptrs:3',
                                         'cpp-variadic-plan=sysv_record_return_ptr:3', 'cpp-variadic-plan=sysv_record_rptr:3',
                                         'cpp-variadic-plan=sysv_record_fixed_rptr:3', 'cpp-variadic-plan=sysv_record_packed:2',
                                         'cpp-variadic-plan=sysv_record_packed_ptr:3', 'cpp-variadic-plan=sysv_record_array:2',
                                         'cpp-variadic-plan=sysv_record_nested:2',
                                         'cpp-variadic-plan=sysv_record_record_array:2',
                                         'cpp-variadic-plan=sysv_record_union_outer:2',
                                         'cpp-variadic-plan=sysv_record_record_array_large:2',
                                         'cpp-variadic-plan=sysv_record_bits:2', 'cpp-variadic-plan=sysv_record_bits_gap:2', 'cpp-variadic-plan=sysv_record_bool_bits:2'):
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
            for name in ('sysv_record_pressure', 'sysv_record_sse_pressure', 'sysv_record_large', 'sysv_record_packed',
                         'sysv_record_record_array_large'):
                if not re.search(r'call[^\n]*@'+name+r'\([^\n]*byval\([^\n]*\) align 8', ns_ir):
                    raise SystemExit(prefix+': falta byval de '+name+'; evidencia: '+str(root))
            if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@sysv_record_ptrs\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', ns_ir):
                raise SystemExit(prefix+': mezcla NS de punteros y record inesperada; evidencia: '+str(root))
            if not re.search(r'call[^\n]*x86_64_sysvcc ptr \(ptr, \.\.\.\).*@sysv_record_return_ptr\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', ns_ir):
                raise SystemExit(prefix+': retorno NS de puntero inesperado; evidencia: '+str(root))
            if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_record_rptr\(i32[^,]+, double [^,]+, i32 [^,]+, ptr ', ns_ir):
                raise SystemExit(prefix+': puntero NS a record inesperado; evidencia: '+str(root))
            if not re.search(r'call[^\n]*x86_64_sysvcc ptr \(ptr, \.\.\.\).*@sysv_record_fixed_rptr\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', ns_ir):
                raise SystemExit(prefix+': puntero NS fijo/retornado a record inesperado; evidencia: '+str(root))
            if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_record_packed_ptr\(i32[^,]+, double [^,]+, i32 [^,]+, ptr ', ns_ir):
                raise SystemExit(prefix+': puntero NS empaquetado inesperado; evidencia: '+str(root))
            for name, pieces in (('sysv_record_small', r'i64 [^,\)]+'),
                                 ('sysv_record_mixed', r'double [^,]+, i32 [^,\)]+'),
                                 ('sysv_record_two', r'i64 [^,]+, double [^,]+, i32 [^,\)]+'),
                                 ('sysv_record_array', r'double [^,]+, double [^,\)]+'),
                                 ('sysv_record_nested', r'double [^,]+, i64 [^,\)]+'),
                                 ('sysv_record_record_array', r'i64 [^,]+, i64 [^,\)]+'),
                                 ('sysv_record_union_outer', r'double [^,]+, i64 [^,\)]+'),
                                 ('sysv_record_bits', r'i32 [^,\)]+'),
                                 ('sysv_record_bits_gap', r'i64 [^,\)]+'),
                                 ('sysv_record_bool_bits', r'i16 [^,\)]+')):
                if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@'+name+r'\(i32[^,]+, '+pieces, ns_ir):
                    raise SystemExit(prefix+': piezas NS inesperadas de '+name+'; evidencia: '+str(root))
            if len(re.findall(r'call[^\n]*x86_64_sysvcc[^\n]*@sysv_record_nested\(i32[^,]+, double [^,]+, i64 [^,\)]+', ns_ir)) < 2:
                raise SystemExit(prefix+': falta la llamada NS con campos typedef; evidencia: '+str(root))
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
    'T09VarRecord small; small.x = 1; small.y = 2; '
    'T09VarMixed record; record.x = 2.25; record.y = 7; '
    'T09VarLarge large; large.x = 11; large.y = 13; large.z = 17; '
    'if (sysv_record_small(1, small) != 1 || small.x != 1 || small.y != 2) return 1; '
    'if (sysv_record_mixed(1, record) != 1 || record.x != 2.25 || record.y != 7) return 2; '
    'if (sysv_record_two(2, small, record) != 1 || small.x != 1 || record.y != 7) return 3; '
    'if (sysv_record_pressure(6, 1, 2, 3, 4, 5, record) != 1 || record.y != 7) return 4; '
    'if (sysv_record_sse_pressure(9, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, record) != 1 || record.y != 7) return 5; '
    'if (sysv_record_large(1, large) != 1 || large.x != 11 || large.y != 13 || large.z != 17) return 6; '
    'int fixedValue = 41; short optionalValue = 12; '
    'if (sysv_record_ptrs(&fixedValue, record, &optionalValue) != 1 || fixedValue != 42 || optionalValue != 13 || record.y != 7) return 7; '
    'fixedValue = 41; optionalValue = 12; '
    'if (sysv_record_return_ptr(&fixedValue, record, &optionalValue) != &fixedValue || fixedValue != 42 || optionalValue != 13 || record.y != 7) return 8; '
    'if (sysv_record_rptr(2, record, &small) != 1 || small.x != 3 || small.y != 2 || record.y != 7) return 9; '
    'small.x = 1; optionalValue = 12; '
    'if (sysv_record_fixed_rptr(&small, record, &optionalValue) != &small || small.x != 3 || optionalValue != 13 || record.y != 7) return 10; '
    'T09VarPackedValue packedValue; packedValue.tag = 7; packedValue.value = 2.25; '
    'if (sysv_record_packed(1, packedValue) != 1 || packedValue.tag != 7 || packedValue.value != 2.25) return 11; '
    'if (sysv_record_packed_ptr(2, record, &packedValue) != 1 || packedValue.tag != 8 || record.y != 7) return 12; '
    'T09VarArray array; array.values[0] = 2.25; array.values[1] = 3.5; '
    'if (sysv_record_array(1, array) != 1 || array.values[0] != 2.25 || array.values[1] != 3.5) return 13; '
    'T09VarNested nested; nested.value = 2.25; nested.leaf.x = 1; nested.leaf.y = 2; '
    'if (sysv_record_nested(1, nested) != 1 || nested.leaf.x != 1 || nested.leaf.y != 2) return 14; '
    'T09VarAliasFields aliasFields; aliasFields.value = 2.25; aliasFields.leaf.x = 1; aliasFields.leaf.y = 2; '
    'if (sysv_record_nested(1, aliasFields) != 1 || aliasFields.leaf.x != 1 || aliasFields.leaf.y != 2) return 18; '
    'T09VarRecordArray recordArray; recordArray.leaves[0].x = 1; recordArray.leaves[0].y = 2; '
    'recordArray.leaves[1].x = 3; recordArray.leaves[1].y = 4; '
    'if (sysv_record_record_array(1, recordArray) != 1 || recordArray.leaves[1].y != 4) return 15; '
    'T09VarRecordArrayLarge recordArrayLarge; recordArrayLarge.leaves[0].x = 1; recordArrayLarge.leaves[0].y = 2; '
    'recordArrayLarge.leaves[1].x = 3; recordArrayLarge.leaves[1].y = 4; '
    'recordArrayLarge.leaves[2].x = 5; recordArrayLarge.leaves[2].y = 6; '
    'if (sysv_record_record_array_large(1, recordArrayLarge) != 1 || recordArrayLarge.leaves[2].y != 6) return 17; '
    'T09VarUnionOuter unionOuter; unionOuter.value = 2.25; unionOuter.choice.integer = 7; '
    'if (sysv_record_union_outer(1, unionOuter) != 1 || unionOuter.choice.integer != 7) return 16; '
    'T09VarBits bits; bits.low = (uint)5; bits.high = (uint)17; bits.middle = (uint)165; bits.upper = (uint)4660; '
    'if (sysv_record_bits(1, bits) != 1 || bits.low != (uint)5 || bits.upper != (uint)4660) return 19; '
    'T09VarBitsGap gap; gap.low = (uint)5; gap.high = (uint)165; gap.upper = (uint)4660; '
    'if (sysv_record_bits_gap(1, gap) != 1 || gap.low != (uint)5 || gap.high != (uint)165 || gap.upper != (uint)4660) return 20; '
    'T09VarBoolBits boolBits; boolBits.first = true; boolBits.second = false; boolBits.plain = true; '
    'if (sysv_record_bool_bits(1, boolBits) != 1 || !boolBits.first || boolBits.second || !boolBits.plain) return 21; '
    'return 0; } } } '
    'namespace T09 { public class PackedBridge { public static int Run<T>(T unused) { '
    'T09VarUnobserved record; record.x = 2.25; record.y = 7; return sysv_record_mixed(1, record); } } '
    'public class PackedPointerBridge { public static int Run<T>(T unused) { '
    'T09VarUnobserved packed; T09VarMixed value; value.x = 2.25; value.y = 7; '
    'return sysv_record_rptr(2, value, &packed); } } '
    'public class BadNestedBridge { public static int Run<T>(T unused) { '
    'T09BadOuter value; value.value = 2.25; value.inner.x = 1; '
    'return sysv_record_nested(1, value); } } '
    'public class BadAliasBridge { public static int Run<T>(T unused) { '
    'T09BadAliasFields value; value.value = 1; return sysv_record_nested(1, value); } } }', encoding='utf-8')
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
for stage in ('hir', 'mir'):
    for jobs in (1, 4):
        output = run('pre-record-'+stage+'-j'+str(jobs), [args.nsc, '--dump-project-oop-'+stage,
            str(recordConsumer/'Consumer.nsproj'), '--cpp-clang-root='+args.clang_root, '-j', str(jobs)])
        for expected in ('cpp-variadic-plan=sysv_record_small:2', 'cpp-variadic-plan=sysv_record_mixed:2',
                         'cpp-variadic-plan=sysv_record_two:3', 'cpp-variadic-plan=sysv_record_pressure:7',
                         'cpp-variadic-plan=sysv_record_sse_pressure:10', 'cpp-variadic-plan=sysv_record_large:2',
                         'cpp-variadic-plan=sysv_record_ptrs:3', 'cpp-variadic-plan=sysv_record_return_ptr:3',
                         'cpp-variadic-plan=sysv_record_rptr:3', 'cpp-variadic-plan=sysv_record_fixed_rptr:3',
                         'cpp-variadic-plan=sysv_record_packed:2', 'cpp-variadic-plan=sysv_record_packed_ptr:3',
                         'cpp-variadic-plan=sysv_record_array:2', 'cpp-variadic-plan=sysv_record_nested:2',
                         'cpp-variadic-plan=sysv_record_record_array:2', 'cpp-variadic-plan=sysv_record_union_outer:2',
                         'cpp-variadic-plan=sysv_record_record_array_large:2', 'cpp-variadic-plan=sysv_record_bits:2', 'cpp-variadic-plan=sysv_record_bits_gap:2', 'cpp-variadic-plan=sysv_record_bool_bits:2'):
            if expected not in output:
                raise SystemExit('Falta '+expected+' en plantilla '+stage+'; evidencia: '+str(root))
        if output.count('cpp-variadic-plan=sysv_record_nested:2') < 2:
            raise SystemExit('Falta plan para campos typedef en plantilla '+stage+'; evidencia: '+str(root))
preIr = run('pre-record-llvm', [args.nsc, '--dump-project-oop-llvm', str(recordConsumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root])
for name, pieces in (('sysv_record_small', r'i64 [^,\)]+'),
                     ('sysv_record_mixed', r'double [^,]+, i32 [^,\)]+'),
                     ('sysv_record_two', r'i64 [^,]+, double [^,]+, i32 [^,\)]+'),
                     ('sysv_record_array', r'double [^,]+, double [^,\)]+'),
                     ('sysv_record_nested', r'double [^,]+, i64 [^,\)]+'),
                     ('sysv_record_record_array', r'i64 [^,]+, i64 [^,\)]+'),
                     ('sysv_record_union_outer', r'double [^,]+, i64 [^,\)]+'),
                     ('sysv_record_bits', r'i32 [^,\)]+'),
                     ('sysv_record_bits_gap', r'i64 [^,\)]+'),
                     ('sysv_record_bool_bits', r'i16 [^,\)]+')):
    if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@'+name+r'\(i32[^,]+, '+pieces, preIr):
        raise SystemExit('Piezas LLVM precompiladas incorrectas: '+name+'; evidencia: '+str(root))
if len(re.findall(r'call[^\n]*x86_64_sysvcc[^\n]*@sysv_record_nested\(i32[^,]+, double [^,]+, i64 [^,\)]+', preIr)) < 2:
    raise SystemExit('Falta llamada LLVM precompilada con typedef; evidencia: '+str(root))
for name in ('sysv_record_pressure', 'sysv_record_sse_pressure', 'sysv_record_large', 'sysv_record_packed',
             'sysv_record_record_array_large'):
    if not re.search(r'call[^\n]*@'+name+r'\([^\n]*byval\([^\n]*\) align 8', preIr):
        raise SystemExit('Falta byval LLVM precompilado: '+name+'; evidencia: '+str(root))
if not re.search(r'call[^\n]*x86_64_sysvcc[^\n]*@sysv_record_ptrs\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', preIr):
    raise SystemExit('Mezcla LLVM precompilada de punteros y record incorrecta; evidencia: '+str(root))
if not re.search(r'call[^\n]*x86_64_sysvcc ptr \(ptr, \.\.\.\).*@sysv_record_return_ptr\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', preIr):
    raise SystemExit('Retorno LLVM precompilado de puntero incorrecto; evidencia: '+str(root))
if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_record_rptr\(i32[^,]+, double [^,]+, i32 [^,]+, ptr ', preIr):
    raise SystemExit('Puntero LLVM precompilado a record incorrecto; evidencia: '+str(root))
if not re.search(r'call[^\n]*x86_64_sysvcc ptr \(ptr, \.\.\.\).*@sysv_record_fixed_rptr\(ptr[^,]+, double [^,]+, i32 [^,]+, ptr ', preIr):
    raise SystemExit('Puntero LLVM fijo/retornado a record incorrecto; evidencia: '+str(root))
if not re.search(r'call[^\n]*x86_64_sysvcc i32 \(i32, \.\.\.\).*@sysv_record_packed_ptr\(i32[^,]+, double [^,]+, i32 [^,]+, ptr ', preIr):
    raise SystemExit('Puntero LLVM empaquetado incorrecto; evidencia: '+str(root))
run('pre-record-consumer', [args.nsc, '--emit-project-oop-exe', str(recordConsumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(recordConsumer/'Consumer.exe')])
run('pre-record-run', [str(recordConsumer/'Consumer.exe')])
cacheExe = recordConsumer/'cached.exe'
cacheCommand = [args.nsc, '--emit-project-oop-exe', str(recordConsumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--artifact-cache='+str(recordConsumer/'cache'),
    '--time', '-O0', '-o', str(cacheExe)]
for label, reason in (('pre-record-cache-fria', 'not-found'), ('pre-record-cache-caliente', 'hit')):
    cacheExe.unlink(missing_ok=True)
    output = run(label, cacheCommand)
    if 'compilation-bundle-cache: '+reason not in output:
        raise SystemExit(label+': estado inesperado; evidencia: '+str(root))
    run(label+'-run', [str(cacheExe)])
(recordConsumer/'BadPacked.ns').write_text('int Main() { return T09.PackedBridge.Run<int>(0); }', encoding='utf-8')
(recordConsumer/'BadPacked.nsproj').write_text(
    '<Project><PropertyGroup><Name>SysvPackedConsumer</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="BadPacked.ns"/><LibraryReference Include="../pre-record-producer/Producer.lib"/>'
    '</ItemGroup></Project>', encoding='utf-8')
badPacked = run('pre-record-packed-rechazo', [args.nsc, '--emit-project-oop-exe', str(recordConsumer/'BadPacked.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(recordConsumer/'BadPacked.exe')], expected=1)
if 'sysv_abi variadic argument type is not yet supported' not in badPacked:
    raise SystemExit('El record packed no se rechazo correctamente; evidencia: '+str(root))
(recordConsumer/'BadPacked.ns').write_text('int Main() { return T09.PackedPointerBridge.Run<int>(0); }', encoding='utf-8')
badPackedPointer = run('pre-record-packed-pointer-rechazo', [args.nsc, '--emit-project-oop-exe', str(recordConsumer/'BadPacked.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(recordConsumer/'BadPacked.exe')], expected=1)
if 'sysv_abi variadic argument type is not yet supported' not in badPackedPointer:
    raise SystemExit('El puntero a record packed no se rechazo correctamente; evidencia: '+str(root))
(recordConsumer/'BadPacked.ns').write_text('int Main() { return T09.BadNestedBridge.Run<int>(0); }', encoding='utf-8')
badNested = run('pre-record-nested-rechazo', [args.nsc, '--emit-project-oop-exe', str(recordConsumer/'BadPacked.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(recordConsumer/'BadPacked.exe')], expected=1)
if 'sysv_abi variadic argument type is not yet supported' not in badNested:
    raise SystemExit('El record anidado no observado no se rechazo correctamente; evidencia: '+str(root))
(recordConsumer/'BadPacked.ns').write_text('int Main() { return T09.BadAliasBridge.Run<int>(0); }', encoding='utf-8')
badAlias = run('pre-record-alias-atributo-rechazo', [args.nsc, '--emit-project-oop-exe', str(recordConsumer/'BadPacked.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(recordConsumer/'BadPacked.exe')], expected=1)
if 'sysv_abi variadic argument type is not yet supported' not in badAlias:
    raise SystemExit('El alias con atributo no se rechazo correctamente; evidencia: '+str(root))
for mode in ('estatica', 'dll'):
    for opt in range(4):
        if mode == 'estatica' and opt == 0:
            continue
        label = 'pre-record-'+mode+'-O'+str(opt)
        sourceCase = root/(mode+'-O'+str(opt))
        currentProducer = root/(label+'-producer')
        currentConsumer = root/(label+'-consumer')
        currentProducer.mkdir()
        currentConsumer.mkdir()
        shutil.copyfile(recordProducer/'Producer.ns', currentProducer/'Producer.ns')
        for name in (('api.h', 'api.lib') if mode == 'estatica' else ('api.h', 'api-dll.h', 'api-dll.lib', 'api.dll')):
            shutil.copyfile(sourceCase/name, currentProducer/name)
        if mode == 'dll':
            shutil.copyfile(sourceCase/'api.dll', currentConsumer/'api.dll')
        reference = ('Header="api.h" Language="C" Standard="c11" Library="api.lib"' if mode == 'estatica' else
                     'Header="api-dll.h" Language="C" Standard="c11" Library="api-dll.lib" Runtime="api.dll"')
        (currentProducer/'Producer.nsproj').write_text(
            '<Project><PropertyGroup><Name>SysvVarRecordProducer</Name><OutputType>Library</OutputType></PropertyGroup>'
            '<ItemGroup><Compile Include="Producer.ns"/><CppLibraryReference '+reference+'/></ItemGroup></Project>', encoding='utf-8')
        (currentConsumer/'Main.ns').write_text('int Main() { return T09.RecordBridge.Run<int>(0); }', encoding='utf-8')
        (currentConsumer/'Consumer.nsproj').write_text(
            '<Project><PropertyGroup><Name>SysvVarRecordConsumer</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns"/><LibraryReference Include="../'+currentProducer.name+'/Producer.lib"/>'
            '</ItemGroup></Project>', encoding='utf-8')
        run(label+'-lib', [args.nsc, '--emit-project-oop-lib', str(currentProducer/'Producer.nsproj'),
            '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(currentProducer/'Producer.lib')])
        (currentProducer/'api.h').rename(currentProducer/'api.h.oculto')
        run(label+'-exe', [args.nsc, '--emit-project-oop-exe', str(currentConsumer/'Consumer.nsproj'),
            '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(currentConsumer/'Consumer.exe')])
        run(label+'-run', [str(currentConsumer/'Consumer.exe')])
print('OK: rechazo de record NativeSharp y plantillas precompiladas escalar y record; evidencia:', root)
print('OK: plantilla con records y punteros, estatica y DLL en O0-O3, sin encabezado')
print('OK: ocho comparaciones C/NS', root)
