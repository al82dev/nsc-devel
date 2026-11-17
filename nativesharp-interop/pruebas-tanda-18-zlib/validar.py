"""T18-008 y T18-009: zlib 1.3.2, una biblioteca C real, compilada con Clang 23.1.0 y consumida desde NativeSharp.

zlib ejercita funciones con punteros a bufer y a longitud (`compress`, `uncompress`, `crc32`, `adler32`,
`compressBound`), una cadena devuelta (`zlibVersion`), un struct con punteros, contadores y punteros a funcion
(`z_stream`) operado por la API de flujo (`deflateInit_`, `deflate`, `deflateEnd`, `inflateInit_`, `inflate`,
`inflateEnd`) y el asignador por defecto de la biblioteca, que usa el runtime de C. El programa NativeSharp y un
control C de Clang hacen las mismas llamadas y devuelven el mismo valor, O0-O3. En estatica, la biblioteca se
enlaza con el runtime estatico de MSVC (`--crt-startup`); con --dll, zlib es una DLL (`ZLIB_DLL`) con su propio
runtime y el ejecutable usa la entrada de NativeSharp. T19-002: `sizeof(z_stream)` pasa a `deflateInit_` sin
conversion explicita, como en C, y `new z_stream()` da el struct en cero, como el `memset` del control.
"""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t18-zlib-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
ZLIB = Path('A:/al82dev-clang-deps/zlib-1.3.2')
UNITS = ('adler32', 'compress', 'crc32', 'deflate', 'infback', 'inffast', 'inflate', 'inftrees', 'trees', 'uncompr', 'zutil')
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
SYSTEM = [TOOLSET / 'include', SDK / 'include/ucrt', SDK / 'include/um', SDK / 'include/shared']
CC = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', *[x for p in SYSTEM for x in ('-isystem', str(p))],
      '-I', str(ZLIB)]
# The same steps in NativeSharp and in C. / Los mismos pasos en NativeSharp y en C.
NS_PROGRAM = '''int Main()
{
    byte* source = stackalloc byte[256];
    for (int i = 0; i < 256; i = i + 1)
    {
        source[i] = (byte)(i % 7);
    }
    sbyte const* version = zlibVersion();
    if (version[0] != 49) return 10;
    uint bound = compressBound(256);
    byte* packed = stackalloc byte[512];
    uint packedLength = 512;
    if (compress(packed, &packedLength, source, 256) != 0) return 1;
    byte* back = stackalloc byte[256];
    uint backLength = 256;
    if (uncompress(back, &backLength, packed, packedLength) != 0) return 2;
    if (backLength != 256) return 3;
    for (int i = 0; i < 256; i = i + 1)
    {
        if (back[i] != source[i]) return 4;
    }
    z_stream stream = new z_stream();
    if (stream.total_in != 0 || stream.opaque != null) return 11;
    if (deflateInit_(&stream, 9, zlibVersion(), sizeof(z_stream)) != 0) return 5;
    byte* streamed = stackalloc byte[512];
    stream.next_in = source;
    stream.avail_in = 256;
    stream.next_out = streamed;
    stream.avail_out = 512;
    if (deflate(&stream, 4) != 1) return 6;
    uint streamedLength = (uint)stream.total_out;
    if (deflateEnd(&stream) != 0) return 7;
    z_stream inflater;
    inflater.next_in = streamed;
    inflater.avail_in = streamedLength;
    if (inflateInit_(&inflater, zlibVersion(), sizeof(z_stream)) != 0) return 8;
    byte* restored = stackalloc byte[256];
    inflater.next_out = restored;
    inflater.avail_out = 256;
    if (inflate(&inflater, 4) != 1 || inflater.total_out != 256) return 9;
    if (inflateEnd(&inflater) != 0) return 12;
    for (int i = 0; i < 256; i = i + 1)
    {
        if (restored[i] != source[i]) return 13;
    }
    uint check = crc32(0, source, 256);
    uint sum = adler32(1, source, 256);
    if (bound < 256) return 14;
    return 20 + (int)(check % 50) + (int)(sum % 50) + (int)(packedLength % 40) + (int)(streamedLength % 40);
}
'''
C_PROGRAM = '''#include <string.h>
#include "zlib.h"
int main(void)
{
    unsigned char source[256], packed[512], back[256], streamed[512], restored[256];
    for (int i = 0; i < 256; i = i + 1) source[i] = (unsigned char)(i % 7);
    if (zlibVersion()[0] != 49) return 10;
    uLong bound = compressBound(256);
    uLongf packedLength = 512;
    if (compress(packed, &packedLength, source, 256) != 0) return 1;
    uLongf backLength = 256;
    if (uncompress(back, &backLength, packed, packedLength) != 0) return 2;
    if (backLength != 256) return 3;
    if (memcmp(back, source, 256) != 0) return 4;
    z_stream stream;
    memset(&stream, 0, sizeof stream);
    if (deflateInit_(&stream, 9, zlibVersion(), (int)sizeof(z_stream)) != 0) return 5;
    stream.next_in = source;
    stream.avail_in = 256;
    stream.next_out = streamed;
    stream.avail_out = 512;
    if (deflate(&stream, 4) != 1) return 6;
    uLong streamedLength = stream.total_out;
    if (deflateEnd(&stream) != 0) return 7;
    z_stream inflater;
    memset(&inflater, 0, sizeof inflater);
    inflater.next_in = streamed;
    inflater.avail_in = (uInt)streamedLength;
    if (inflateInit_(&inflater, zlibVersion(), (int)sizeof(z_stream)) != 0) return 8;
    inflater.next_out = restored;
    inflater.avail_out = 256;
    if (inflate(&inflater, 4) != 1 || inflater.total_out != 256) return 9;
    if (inflateEnd(&inflater) != 0) return 12;
    if (memcmp(restored, source, 256) != 0) return 13;
    uLong check = crc32(0, source, 256);
    uLong sum = adler32(1, source, 256);
    if (bound < 256) return 14;
    return 20 + (int)(check % 50) + (int)(sum % 50) + (int)(packedLength % 40) + (int)(streamedLength % 40);
}
'''


def run(label, command, cwd, expected=0, extra_env=None):
    result = subprocess.run(command, cwd=cwd, env={**env, **(extra_env or {})}, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
library = root / 'zlib'
library.mkdir()
dll = ['-DZLIB_DLL', '-DZLIB_INTERNAL'] if args.dll else []
for unit in UNITS:
    run('cc-' + unit, [*CC, '-O2', *dll, '-c', str(ZLIB / (unit + '.c')), '-o', unit + '.obj'], library)
objects = [unit + '.obj' for unit in UNITS]
if args.dll:
    run('zlib-dll', ['lld-link.exe', '/nologo', '/dll', '/out:zlib.dll', '/implib:zlib.lib', *objects,
                     *[str(p) for p in CRT]], library)
else:
    run('zlib-lib', ['llvm-lib.exe', '/nologo', '/out:zlib.lib', *objects], library)
system = ''.join('<IncludeDirectory Path="' + str(p).replace('\\', '/') + '" Kind="System" />' for p in SYSTEM)
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    for name in ('zlib.lib', *(('zlib.dll',) if args.dll else ())):
        shutil.copyfile(library / name, case / name)
    (case / 'control.c').write_text(C_PROGRAM, encoding='utf-8')
    run(f'O{opt}-control-obj', [*CC, '-O' + str(opt), *(['-DZLIB_DLL'] if args.dll else []), '-c', 'control.c',
                                '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj',
                                 'zlib.lib', *[str(p) for p in CRT]], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control < 20:
        raise SystemExit(f'O{opt}: el control C devolvio {control}; evidencia: {root}')
    (case / 'Main.ns').write_text(NS_PROGRAM, encoding='utf-8')
    runtime = ' Runtime="zlib.dll"' if args.dll else ''
    define = '<Define Value="ZLIB_DLL" />' if args.dll else ''
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T18Zlib</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="' + str(ZLIB / 'zlib.h').replace('\\', '/') + '" Language="C" Standard="c17" '
        'Library="zlib.lib"' + runtime + '>' + system + define + '</CppLibraryReference></ItemGroup></Project>',
        encoding='utf-8')
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt)]
    extra = {}
    if not args.dll:
        command.append('--crt-startup')
        for path in CRT:
            command += ['--link-lib', str(path)]
        extra = {'NSC_LINKER': 'lld-link.exe'}
    else:
        extra = {'NSC_LINKER': 'nslink.exe'}
    run(f'O{opt}-nsc', command + ['-o', 'app.exe'], case, extra_env=extra)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, C {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result} (compresion, flujo, crc32 y adler32)', flush=True)
print('OK: zlib 1.3.2 consumida desde NativeSharp, frente a Clang; evidencia:', root)
