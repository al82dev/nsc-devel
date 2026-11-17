"""T19-036: vectores de Clang sueltos en funciones C (Language="C").

Una biblioteca C con vectores `float` x4, `double` x2, `int` x4 y `long long` x2 por valor como parametro y como
resultado de funciones `cdecl`, y `float` x4 con `__vectorcall`; el consumidor NativeSharp usa los portadores de
NativeSharp.Core.Numerics (Float32x4, Float64x2, Int32x4, Int64x2). Con --avx agrega vectores de 256 bits (`float`
x8, `double` x4, `int` x8; portadores Float32x8, Float64x4, Int32x8) con `cdecl` y `float` x8 con `__vectorcall`,
con el nivel de CPU `x86-64-v3` (`CpuLevel` del proyecto y `-march` de Clang).

Compara el resultado con un control C de Clang 23.1.0 y la
forma de cada declaracion del IR de NSC con la del IR de Clang, O0-O3, con la biblioteca estatica y, con --dll,
como DLL; repite el programa desde una plantilla NativeSharp precompilada, con y sin el encabezado C visible para
el consumidor.

Uso: validar-vectores-c.py --nsc <nsc.exe> --clang-root <raiz de Clang> [--dll] [--avx]
"""
import argparse
import os
import re
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
parser.add_argument('--avx', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-vectores-c-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CC = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c11'] + (['-march=x86-64-v3'] if args.avx else [])
CPU = '<CpuLevel>x86-64-v3</CpuLevel>' if args.avx else ''
CORE = '<ProjectReference Include="A:/nsc-devel/lib/NativeSharp.Core/NativeSharp.Core.nsproj" />'
HEADER = '''#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
typedef float f4 __attribute__((vector_size(16)));
typedef int i4 __attribute__((vector_size(16)));
typedef long long l2 __attribute__((vector_size(16)));
API f4 c_scale(f4 v, float k);
API float c_fsum(f4 v);
API i4 c_iadd(i4 a, int k, i4 b);
API int c_isum(i4 v);
API l2 c_lmake(long long a, long long b);
API long long c_lsum(l2 v, int k);
API f4 __vectorcall c_vscale(f4 v, double k);
typedef double d2 __attribute__((vector_size(16)));
API d2 c_dmake(double a, double b);
API double c_dsum(d2 v);
'''
AVX_HEADER = '''typedef float f8 __attribute__((vector_size(32)));
typedef double d4 __attribute__((vector_size(32)));
typedef int i8 __attribute__((vector_size(32)));
API f8 c_fscale8(f8 v, float k);
API float c_fsum8(f8 v);
API d4 c_dmake4(double a);
API double c_dsum4(d4 v);
API i8 c_iadd8(i8 a, i8 b);
API int c_isum8(i8 v);
API f8 __vectorcall c_vscale8(f8 v, double k);
'''
SOURCE = '''#include "api.h"
f4 c_scale(f4 v, float k) { f4 one = {1.0f, 0.0f, 0.0f, 0.5f}; return v * k + one; }
float c_fsum(f4 v) { return v[0] + v[1] * 10.0f + v[2] * 100.0f + v[3] * 1000.0f; }
i4 c_iadd(i4 a, int k, i4 b) { return a * k + b; }
int c_isum(i4 v) { return v[0] + v[1] * 10 + v[2] * 100 + v[3] * 1000; }
l2 c_lmake(long long a, long long b) { l2 r = {a * 3, b + 7}; return r; }
long long c_lsum(l2 v, int k) { return v[0] * k + v[1]; }
f4 __vectorcall c_vscale(f4 v, double k) { return v * (float)k; }
d2 c_dmake(double a, double b) { d2 r = {a * 3.0, b + 0.5}; return r; }
double c_dsum(d2 v) { return v[0] + v[1] * 100.0; }
'''
AVX_SOURCE = '''f8 c_fscale8(f8 v, float k) { return v * k; }
float c_fsum8(f8 v) { float s = 0.0f; for (int i = 0; i < 8; ++i) s += v[i] * (float)(i + 1); return s; }
d4 c_dmake4(double a) { d4 r = {a, a + 1.0, a * 2.0, a - 3.0}; return r; }
double c_dsum4(d4 v) { return v[0] + v[1] * 10.0 + v[2] * 100.0 + v[3] * 1000.0; }
i8 c_iadd8(i8 a, i8 b) { return a + b * 2; }
int c_isum8(i8 v) { int s = 0; for (int i = 0; i < 8; ++i) s += v[i] * (i + 1); return s; }
f8 __vectorcall c_vscale8(f8 v, double k) { return v * (float)k; }
'''
CONTROL = '''#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    f4 a = {1.0f, 2.0f, 3.0f, 4.0f}; f4 v = c_scale(a, 2.0f);
    i4 i = {1, 2, 3, 4}; i4 j = {5, 6, 7, 8}; i4 b = c_iadd(i, 3, j); l2 l = c_lmake(4, 5);
    d2 d = c_dmake(2.0, 4.0);
    return (int)c_fsum(v) + c_isum(b) * 3 + (int)c_lsum(l, 2) * 7 + (int)c_fsum(c_vscale(v, 0.5)) * 11 +
        (int)v[3] + b[2] + (int)l[1] + (int)c_dsum(d) * 13 + (int)d[1]AVX_EXPRESSION;
}
'''
AVX_CONTROL = '''
    f8 a8 = {1, 2, 3, 4, 5, 6, 7, 8}; f8 v8 = c_fscale8(a8, 2.0f); d4 d4v = c_dmake4(2.0);
    i8 i8v = {1, 2, 3, 4, 5, 6, 7, 8}; i8 j8 = c_iadd8(i8v, i8v);'''
AVX_C_EXPRESSION = (' + (int)c_fsum8(v8) + (int)c_dsum4(d4v) * 2 + c_isum8(j8) * 3 + '
                    '(int)c_fsum8(c_vscale8(v8, 0.5)) * 5 + (int)v8[7] + j8[6] + (int)d4v[2]')
BODY = '''
    Float32x4 a = new Float32x4(1.0f, 2.0f, 3.0f, 4.0f); Float32x4 v = c_scale(a, 2.0f);
    Int32x4 i; i.X = 1; i.Y = 2; i.Z = 3; i.W = 4; Int32x4 j; j.X = 5; j.Y = 6; j.Z = 7; j.W = 8;
    Int32x4 sum4 = c_iadd(i, 3, j); Int64x2 pair = c_lmake(4, 5);
    Float64x2 d = c_dmake(2.0, 4.0);
    return (int)c_fsum(v) + c_isum(sum4) * 3 + (int)c_lsum(pair, 2) * 7 + (int)c_fsum(c_vscale(v, 0.5)) * 11 +
        (int)v.W + sum4.Z + (int)pair.Y + (int)c_dsum(d) * 13 + (int)d.YAVX_EXPRESSION;
'''
AVX_BODY = '''
    Float32x8 a8; a8.X = 1.0f; a8.Y = 2.0f; a8.Z = 3.0f; a8.W = 4.0f; a8.A = 5.0f; a8.B = 6.0f; a8.C = 7.0f;
    a8.D = 8.0f; Float32x8 v8 = c_fscale8(a8, 2.0f); Float64x4 d4v = c_dmake4(2.0);
    Int32x8 i8v; i8v.X = 1; i8v.Y = 2; i8v.Z = 3; i8v.W = 4; i8v.A = 5; i8v.B = 6; i8v.C = 7; i8v.D = 8;
    Int32x8 j8 = c_iadd8(i8v, i8v);'''
AVX_NS_EXPRESSION = (' + (int)c_fsum8(v8) + (int)c_dsum4(d4v) * 2 + c_isum8(j8) * 3 + '
                     '(int)c_fsum8(c_vscale8(v8, 0.5)) * 5 + (int)v8.D + j8.C + (int)d4v.Z')
if args.avx:
    HEADER += AVX_HEADER
    SOURCE += AVX_SOURCE
    CONTROL = CONTROL.replace('    d2 d = c_dmake', AVX_CONTROL.lstrip('\n') + '\n    d2 d = c_dmake')
    CONTROL = CONTROL.replace('AVX_EXPRESSION', AVX_C_EXPRESSION)
    BODY = AVX_BODY + BODY
    BODY = BODY.replace('AVX_EXPRESSION', AVX_NS_EXPRESSION)
else:
    CONTROL = CONTROL.replace('AVX_EXPRESSION', '')
    BODY = BODY.replace('AVX_EXPRESSION', '')
EXPECTED = 16 if args.avx else 9
MAIN = 'using NativeSharp.Core.Numerics;\nint Main()\n{' + BODY + '}\n'


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected == 0 and result.returncode:
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def project(folder, source, references, library=False, name='T19VectoresC'):
    (folder / 'Main.ns').write_text(source, encoding='utf-8')
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>' + name + '</Name>' + ('<OutputType>Library</OutputType>' if library else '') +
        CPU +
        '</PropertyGroup><ItemGroup><Compile Include="Main.ns" />' + references + CORE + '</ItemGroup></Project>',
        encoding='utf-8')


def reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C" Standard="c11" Library="' + prefix +
            'api.lib"' + runtime + ' />')


def nsc_run(label, folder, opt):
    run(label + '-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
    return run(label + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode


def precompiled(folder, opt, control):
    """The same body from a precompiled NativeSharp generic, with and without the C header for the consumer."""
    # El mismo cuerpo desde una plantilla NativeSharp precompilada, con y sin el encabezado C en el consumidor.
    producer = folder / 'producer'
    producer.mkdir()
    project(producer, 'using NativeSharp.Core.Numerics;\nnamespace T19P { public class Bridge { public static int '
            'Run<U>(U unused) {' + BODY + '} } }\n', reference('../'), True, 'T19VectoresCProducer')
    run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                             '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
    for visibility in ('con-encabezado', 'sin-encabezado'):
        consumer = folder / visibility
        consumer.mkdir()
        references = '<LibraryReference Include="../producer/Producer.lib" />'
        if visibility == 'con-encabezado':
            references += reference('../')
        else:
            (folder / 'api.h').rename(folder / 'api.h.oculto')
        if args.dll:
            (consumer / 'api.dll').write_bytes((folder / 'api.dll').read_bytes())
        project(consumer, 'int Main() { return T19P.Bridge.Run<int>(0); }\n', references)
        try:
            result = nsc_run(f'O{opt}-{visibility}', consumer, opt)
        finally:
            if visibility == 'sin-encabezado':
                (folder / 'api.h.oculto').rename(folder / 'api.h')
        if result != control:
            raise SystemExit(f'O{opt} {visibility}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')


def declarations(ir):
    """Shape of each c_* declaration: convention, result and per-parameter type."""
    # Forma de cada declaracion c_*: convencion, resultado y tipo de cada parametro.
    found = {}
    for line in ir.splitlines():
        # A C `__vectorcall` symbol carries its decoration (`c_vscale@@24`).
        # Un simbolo C `__vectorcall` lleva su decoracion (`c_vscale@@24`).
        match = re.match(r'declare\s+(.*?)\s+@"?(?:\\01)?(c_\w+(?:@@\d+)?)"?\((.*)\)', line)
        if not match:
            continue
        head, symbol, parameters = match.groups()
        drop = ('dso_local', 'dllimport', 'noundef', 'nonnull', 'zeroext', 'signext', 'inreg')
        words = [w for w in head.split() if w not in drop and not w.startswith(('dereferenceable', 'align'))]
        result = 'vec' + head[head.index('<'):head.index('>') + 1].replace(' ', '') if '<' in head else words[-1]
        shapes = []
        for parameter in [p.strip() for p in parameters.split(',')] if parameters.strip() else []:
            shapes.append('vec' + parameter[:parameter.index('>') + 1].replace(' ', '') if parameter.startswith('<')
                          else 'sret' if 'sret' in parameter else parameter.split()[0])
        found[symbol] = ('x86_vectorcallcc' in head, result, tuple(shapes))
    return found


library = root / 'lib'
library.mkdir()
(library / 'api.h').write_text(HEADER, encoding='utf-8')
(library / 'api.c').write_text(('int _fltused = 0;\n' if args.dll else '') + SOURCE, encoding='utf-8')
run('api', [*CC, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.c', '-o', 'api.obj'], library)
if args.dll:
    run('api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll', '/implib:api.lib',
                    'api.obj'], library)
    header = '#define T19_IMPORT 1\n' + HEADER
else:
    run('api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
    header = HEADER
for opt in range(4):
    folder = root / f'O{opt}'
    folder.mkdir()
    (folder / 'api.h').write_text(header, encoding='utf-8')
    for item in ('api.lib', *(('api.dll',) if args.dll else ())):
        (folder / item).write_bytes((library / item).read_bytes())
    (folder / 'control.c').write_text(CONTROL, encoding='utf-8')
    run(f'O{opt}-control', [*CC, '-O' + str(opt), 'control.c', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], folder)
    control = run(f'O{opt}-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode
    project(folder, MAIN, reference())
    result = nsc_run(f'O{opt}', folder, opt)
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    clang = declarations(run(f'O{opt}-control-ir', [*CC, '-O0', '-S', '-emit-llvm', 'control.c', '-o', '-'],
                             folder).stdout)
    nsc = declarations(run(f'O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                              '--cpp-clang-root=' + args.clang_root], folder).stdout)
    if len(clang) != EXPECTED:
        raise SystemExit(f'O{opt}: el IR de Clang declara {len(clang)} funciones c_*; evidencia: {root}')
    for symbol, shape in clang.items():
        if nsc.get(symbol) != shape:
            raise SystemExit(f'O{opt}: {symbol} tiene {nsc.get(symbol)} en NSC y {shape} en Clang; evidencia: {root}')
    precompiled(folder, opt, control)
    print(f'vectores C O{opt}: NativeSharp y Clang devuelven {result}; {len(clang)} declaraciones con la forma de '
          'Clang; plantilla precompilada con y sin encabezado', flush=True)
print('OK: vectores' + (' de 128 y 256 bits' if args.avx else ' de 128 bits') + ' en funciones C frente a Clang; '
      'evidencia:', root, flush=True)
