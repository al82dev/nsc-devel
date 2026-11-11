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
                '/export:t09_get_sysv_callback', '/export:t09_sysv_sum', '/export:t09_get_float_callback',
                '/export:t09_get_float_one_callback', '/export:t09_get_wide_callback',
                '/export:t09_get_uint_callback', '/export:t09_get_ulong_callback',
                '/export:t09_get_s8_callback', '/export:t09_get_u8_callback', '/export:t09_get_s16_callback',
                '/export:t09_get_u16_callback', '/export:t09_get_bool_callback',
                '/export:t09_get_pointer_callback', '/export:t09_get_void_callback', '/out:'+str(case/'api.dll'),
                '/implib:'+str(case/'api-dll.lib'), str(case/'api.obj')])
            library, project, cflags = 'api-dll.lib', 'Dll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix+'-control-obj', ['clang.exe', *flags, *cflags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
        run(prefix+'-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(prefix+'-control-run', [str(case/'control.exe')])
        common = [str(case/project), '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)]
        ir = run(prefix+'-ns-ir', [args.nsc, '--dump-project-oop-llvm', *common])
        for result_type in ('i32', 'i64', 'double', 'float', 'ptr', 'void'):
            if not re.search(r'call\s+x86_64_sysvcc\s+'+result_type+r'\s+%[^\(]+\(', ir):
                raise SystemExit(prefix+': falta convencion indirecta de '+result_type+'; evidencia: '+str(root))
        for bits, extension in ((8, 'signext'), (8, 'zeroext'), (16, 'signext'), (16, 'zeroext'), (1, 'zeroext')):
            pattern = r'call\s+x86_64_sysvcc\s+'+extension+r'\s+i'+str(bits)+r'\s+%[^\(]+\(i'+str(bits)+r'\s+(?:noundef\s+)?'+extension
            if not re.search(pattern, ir):
                raise SystemExit(prefix+': falta '+extension+' i'+str(bits)+'; evidencia: '+str(root))
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
    'T09SysvCallback direct = t09_sysv_sum; if (direct != callback || direct(1, 2, 3, 4, 5, 6) != 91) return 12; '
    'if (callback(1, 2, 3, 4, 5, 6) != 91) return 1; '
    'T09FloatCallback floating = t09_get_float_callback(); if (floating(1.5, 2.25f, 3, 4.0) != 10.75) return 2; '
    'T09FloatOneCallback one = t09_get_float_one_callback(); if (one(2.5f) != 3.5f) return 3; '
    'T09WideCallback wide = t09_get_wide_callback(); if (wide(-5000000000L, (uint)7) != -4999999993L) return 4; '
    'T09UIntCallback uintCallback = t09_get_uint_callback(); if (uintCallback((uint)4000000000) != (uint)4000000007) return 7; '
    'T09ULongCallback ulongCallback = t09_get_ulong_callback(); if (ulongCallback((ulong)5000000000, (uint)7) != (ulong)5000000007) return 8; '
    'T09S8Callback signed8 = t09_get_s8_callback(); T09U8Callback unsigned8 = t09_get_u8_callback(); '
    'if (signed8((sbyte)-7) != (sbyte)-6 || unsigned8((byte)250) != (byte)251) return 9; '
    'T09S16Callback signed16 = t09_get_s16_callback(); T09U16Callback unsigned16 = t09_get_u16_callback(); '
    'if (signed16((short)-1000) != (short)-999 || unsigned16((ushort)65000) != (ushort)65001) return 10; '
    'T09BoolCallback boolean = t09_get_bool_callback(); if (boolean(true) != false || boolean(false) != true) return 11; '
    'int x = 10; T09PointerCallback pointer = t09_get_pointer_callback(); '
    'if (pointer(&x, 3) != &x || x != 13) return 5; '
    'T09VoidCallback setter = t09_get_void_callback(); setter(&x); if (x != 47) return 6; '
    'return 0; } } }', encoding='utf-8')
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
for result_type in ('i32', 'i64', 'double', 'float', 'ptr', 'void'):
    if not re.search(r'call\s+x86_64_sysvcc\s+'+result_type+r'\s+%[^\(]+\(', ir):
        raise SystemExit('Falta convencion indirecta precompilada de '+result_type+'; evidencia: '+str(root))
for bits, extension in ((8, 'signext'), (8, 'zeroext'), (16, 'signext'), (16, 'zeroext'), (1, 'zeroext')):
    pattern = r'call\s+x86_64_sysvcc\s+'+extension+r'\s+i'+str(bits)+r'\s+%[^\(]+\(i'+str(bits)+r'\s+(?:noundef\s+)?'+extension
    if not re.search(pattern, ir):
        raise SystemExit('Falta '+extension+' i'+str(bits)+' precompilado; evidencia: '+str(root))
run('pre-consumer', [args.nsc, '--emit-project-oop-exe', str(consumer/'Consumer.nsproj'),
    '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O0', '-o', str(consumer/'Consumer.exe')])
run('pre-run', [str(consumer/'Consumer.exe')])
print('OK: ocho comparaciones C/NativeSharp y plantilla precompilada; evidencia:', root, flush=True)
