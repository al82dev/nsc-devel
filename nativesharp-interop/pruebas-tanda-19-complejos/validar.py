"""T19-001: valores C `_Complex`, frente a Clang 23.1.0.

Un `_Complex T` de C (T entero o flotante estandar) es en NativeSharp el struct C sintetico `__complex_T`
(`__complex_double`, `__complex_float`, `__complex_long_double`, `__complex_int`, `__complex_long_long`...) con los
campos `real` e `imag` de tipo T: la representacion que da C11 6.2.5p13. Se importa como cualquier struct C, asi
que viaja como un record C por valor: parametros y resultados (`cdecl`, `__vectorcall`, `sysv_abi`), campos de
records, globales y arreglos globales, punteros, callbacks y typedefs.

El mismo encabezado se lee tambien como C++ (`_Complex` es alli una extension de Clang): sus funciones `extern "C"`
y una funcion C++ decorada toman y devuelven el portador como record C++ trivial.

Compara un control de Clang con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL,
directo y desde una plantilla precompilada con y sin encabezado. Rechazo genuino, tambien en Clang: pasar un
`double _Complex*` a un parametro `float _Complex*`.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-complejos-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CC = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17']

API = '''#pragma once
#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
HEADER = '''#ifdef __cplusplus
extern "C" {
#endif
typedef struct ZR { int k; double _Complex z; float _Complex w; } ZR;
typedef double _Complex cplx;
typedef double _Complex (*CMap)(double _Complex z);
API double _Complex cmk(double re, double im);
API float _Complex fmk(float re, float im);
API double cabs2(double _Complex z);
API double _Complex cmul(double _Complex a, double _Complex b);
API float _Complex fadd(float _Complex a, float _Complex b);
API long double _Complex lmk(long double re, long double im);
API _Complex int imk(int re, int im);
API _Complex long long llmk(long long re, long long im);
API void cscale(double _Complex* z, double k);
API double zsum(ZR v);
API ZR zmake(int k);
API extern double _Complex gz;
API extern double _Complex gza[3];
API double capply(CMap f, double _Complex z);
API cplx cconj(cplx z);
API double _Complex __vectorcall vmul(double _Complex a, double _Complex b);
API float _Complex __vectorcall vfmk(float re, float im);
API double _Complex __attribute__((sysv_abi)) smul(double _Complex a, double _Complex b);
API void fscale(float _Complex* z, float k);
#ifdef __cplusplus
}
namespace cx { API double _Complex mul(double _Complex a, double _Complex b); }
#endif
'''
SOURCE = '''#include "api.h"
#ifdef __cplusplus
extern "C" {
#endif
__declspec(selectany) int _fltused = 0;
static double _Complex mkd(double re, double im) { double _Complex z; __real__ z = re; __imag__ z = im; return z; }
static float _Complex mkf(float re, float im) { float _Complex z; __real__ z = re; __imag__ z = im; return z; }
static double _Complex mul(double _Complex a, double _Complex b)
{ return mkd(__real__ a * __real__ b - __imag__ a * __imag__ b,
             __real__ a * __imag__ b + __imag__ a * __real__ b); }
double _Complex cmk(double re, double im) { return mkd(re, im); }
float _Complex fmk(float re, float im) { return mkf(re, im); }
double cabs2(double _Complex z) { return __real__ z * __real__ z + __imag__ z * __imag__ z; }
double _Complex cmul(double _Complex a, double _Complex b) { return mul(a, b); }
float _Complex fadd(float _Complex a, float _Complex b)
{ return mkf(__real__ a + __real__ b, __imag__ a + __imag__ b); }
long double _Complex lmk(long double re, long double im)
{ long double _Complex z; __real__ z = re; __imag__ z = im; return z; }
_Complex int imk(int re, int im) { _Complex int z; __real__ z = re; __imag__ z = im; return z; }
_Complex long long llmk(long long re, long long im)
{ _Complex long long z; __real__ z = re; __imag__ z = im; return z; }
void cscale(double _Complex* z, double k) { *z = mkd(__real__ *z * k, __imag__ *z * k); }
double zsum(ZR v) { return v.k * 100 + __real__ v.z * 10 + __imag__ v.z + __real__ v.w + __imag__ v.w; }
ZR zmake(int k) { ZR v; v.k = k; v.z = mkd(2.0, 3.0); v.w = mkf(4.0f, 5.0f); return v; }
double _Complex gz = {1.0, 1.0};
double _Complex gza[3] = {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}};
double capply(CMap f, double _Complex z) { double _Complex r = f(z); return __real__ r * 10 + __imag__ r; }
cplx cconj(cplx z) { return mkd(__real__ z, -__imag__ z); }
double _Complex __vectorcall vmul(double _Complex a, double _Complex b) { return mul(a, b); }
float _Complex __vectorcall vfmk(float re, float im) { return mkf(re, im); }
double _Complex __attribute__((sysv_abi)) smul(double _Complex a, double _Complex b) { return mul(a, b); }
void fscale(float _Complex* z, float k) { *z = mkf(__real__ *z * k, __imag__ *z * k); }
#ifdef __cplusplus
}
double _Complex cx::mul(double _Complex a, double _Complex b) { return mkd(__real__ a + __imag__ b, __imag__ a); }
#endif
'''
# The same statements in both languages; `mix` folds each result in. C reads the parts with __real__ and
# __imag__; NativeSharp with the fields real and imag. / Las mismas sentencias en ambos lenguajes.
STEPS = [
    ('double _Complex a = cmk(1.5, 2.5); mix((int)(__real__ a * 10)); mix((int)(__imag__ a * 10)); '
     'mix((int)cabs2(a));',
     'var a = cmk(1.5, 2.5); mix((int)(a.real * 10)); mix((int)(a.imag * 10)); mix((int)cabs2(a));'),
    ('double _Complex p = cmul(cmk(1, 2), cmk(3, 4)); mix((int)__real__ p + 100); mix((int)__imag__ p);',
     'var p = cmul(cmk(1.0, 2.0), cmk(3.0, 4.0)); mix((int)p.real + 100); mix((int)p.imag);'),
    ('float _Complex f = fadd(fmk(1.5f, 2.0f), fmk(0.5f, 1.0f)); mix((int)(__real__ f * 10) + (int)__imag__ f); '
     'fscale(&f, 2.0f); mix((int)(__real__ f + __imag__ f));',
     'var f = fadd(fmk(1.5f, 2.0f), fmk(0.5f, 1.0f)); mix((int)(f.real * 10) + (int)f.imag); '
     'fscale(&f, 2.0f); mix((int)(f.real + f.imag));'),
    ('long double _Complex l = lmk(2.0L, 3.0L); mix((int)(__real__ l + __imag__ l)); _Complex int i = imk(4, 5); '
     'mix(__real__ i * 10 + __imag__ i); _Complex long long q = llmk(6, 7); mix((int)(__real__ q * 10 + __imag__ q));',
     'var l = lmk(2.0, 3.0); mix((int)(l.real + l.imag)); var i = imk(4, 5); mix(i.real * 10 + i.imag); '
     'var q = llmk(6, 7); mix((int)(q.real * 10 + q.imag));'),
    ('double _Complex z = cmk(2, 3); cscale(&z, 4); mix((int)(__real__ z + __imag__ z));',
     'var z = cmk(2.0, 3.0); cscale(&z, 4.0); mix((int)(z.real + z.imag));'),
    ('ZR zr = zmake(3); mix((int)zsum(zr)); zr.z = cmk(1, 1); zr.w = fmk(2, 2); mix((int)zsum(zr));',
     'ZR zr = zmake(3); mix((int)zsum(zr)); zr.z = cmk(1.0, 1.0); zr.w = fmk(2.0f, 2.0f); mix((int)zsum(zr));'),
    ('gz = cmk(5, 6); mix((int)(__real__ gz * 10 + __imag__ gz)); mix((int)(__real__ gza[2] + __imag__ gza[1]));',
     'gz = cmk(5.0, 6.0); mix((int)(gz.real * 10 + gz.imag)); mix((int)(gza[2].real + gza[1].imag));'),
    ('mix((int)capply(swap2, cmk(1, 2)));',
     'CMap m = (__complex_double v) => cmk(v.imag * 2.0, v.real); mix((int)capply(m, cmk(1.0, 2.0)));'),
    ('cplx c = cconj(cmk(3, 4)); mix((int)(__real__ c * 10 - __imag__ c));',
     'cplx c = cconj(cmk(3.0, 4.0)); mix((int)(c.real * 10 - c.imag));'),
    ('double _Complex n = cmk(7, 8); mix((int)cabs2(n));',
     '__complex_double n = new __complex_double(); n.real = 7.0; n.imag = 8.0; mix((int)cabs2(n));'),
    ('double _Complex v = vmul(cmk(1, 2), cmk(2, 1)); mix((int)(__real__ v + 10 * __imag__ v)); '
     'float _Complex vf = vfmk(1.5f, 2.5f); mix((int)(__real__ vf * 10 + __imag__ vf)); '
     'double _Complex s = smul(cmk(2, 0), cmk(3, 1)); mix((int)(__real__ s * 10 + __imag__ s));',
     'var v = vmul(cmk(1.0, 2.0), cmk(2.0, 1.0)); mix((int)(v.real + 10 * v.imag)); '
     'var vf = vfmk(1.5f, 2.5f); mix((int)(vf.real * 10 + vf.imag)); '
     'var s = smul(cmk(2.0, 0.0), cmk(3.0, 1.0)); mix((int)(s.real * 10 + s.imag));'),
]
NS_MIX = ('namespace T19V { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = ('#include <stddef.h>\n'
         '#ifdef __cplusplus\n#define T19_C extern "C"\n#else\n#define T19_C\n#endif\n'
         'T19_C __declspec(selectany) int _fltused = 0;\n'
         'T19_C void* memset(void* d, int c, size_t n) { volatile unsigned char* p = (volatile unsigned char*)d; '
         'while (n--) *p++ = (unsigned char)c; return d; }\n'
         'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n'
         'static double _Complex swap2(double _Complex v) { return cmk(__imag__ v * 2.0, __real__ v); }\n')
# C++ only: a C++ function (mangled) of the same header. / Solo C++: una funcion C++ (decorada) del encabezado.
STEPS_CPP = [
    ('double _Complex mm = cx::mul(cmk(1, 2), cmk(3, 4)); mix((int)(__real__ mm * 10 + __imag__ mm));',
     'var mm = cx.mul(cmk(1.0, 2.0), cmk(3.0, 4.0)); mix((int)(mm.real * 10 + mm.imag));'),
]
LANGUAGES = [
    ('C', 'c17', 'c', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17'], STEPS),
    ('C++', 'c++20', 'cpp', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20'],
     [step for step in STEPS if 'new __complex_double' not in step[1]] + STEPS_CPP),
]


def c_body(steps):
    return 'r = 0; ' + ' '.join(c for c, _ in steps) + ' return r;'


def ns_body(steps):
    return ('T19V.Mixer.r = 0; ' + ' '.join(n for _, n in steps).replace('mix(', 'T19V.Mixer.mix(')
            + ' return T19V.Mixer.r;')


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def reference(prefix='', language='C', standard='c17'):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="' + language + '" Standard="' + standard
            + '" Library="' + prefix + 'api.lib"' + runtime + ' />')


def project(name, references, source, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def build_and_run(tag, folder, opt, expected):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for language, standard, extension, compiler, steps in LANGUAGES:
    for opt in range(4):
        tag = extension + f'-O{opt}'
        case = root / tag
        case.mkdir()
        (case / 'api.h').write_text(API + HEADER, encoding='utf-8')
        (case / ('api.' + extension)).write_text(SOURCE, encoding='utf-8')
        run(f'{tag}-api', [*compiler, '-O' + str(opt), *(['-DT19_EXPORT'] if args.dll else []), '-c',
                           'api.' + extension, '-o', 'api.obj'], case)
        if args.dll:
            run(f'{tag}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                '/implib:api.lib', 'api.obj'], case)
            (case / 'api.h').write_text('#define T19_IMPORT 1\n' + API + HEADER, encoding='utf-8')
        else:
            run(f'{tag}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        (case / ('control.' + extension)).write_text('#include "api.h"\n' + C_MIX + 'int main(void) { ' + c_body(steps)
                                                     + ' }\n', encoding='utf-8')
        run(f'{tag}-control', [*compiler, '-O' + str(opt), 'control.' + extension, 'api.lib', '-fuse-ld=lld',
                               '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'], case)
        expected = run(f'{tag}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
        app = case / 'app'
        app.mkdir()
        (app / 'Main.ns').write_text(NS_MIX + 'int Main() { ' + ns_body(steps) + ' }\n', encoding='utf-8')
        (app / 'App.nsproj').write_text(project('T19Complejos', reference('../', language, standard), 'Main.ns'),
                                        encoding='utf-8')
        if args.dll:
            shutil.copyfile(case / 'api.dll', app / 'api.dll')
        build_and_run(f'directo-{tag}', app, opt, expected)
        producer = case / 'producer'
        producer.mkdir()
        (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int '
                                              'Run<Q>(Q unused) { ' + ns_body(steps) + ' } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text(project('T19ComplejosProducer', reference('../', language, standard),
                                                          'Producer.ns', True), encoding='utf-8')
        run(f'{tag}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                                 '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
                                 '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility
            consumer.mkdir()
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += reference('../', language, standard)
            else:
                (case / 'api.h').rename(case / 'api.h.oculto')
            (consumer / 'Main.ns').write_text('int Main() { return T19P.Bridge.Run<int>(0); }\n', encoding='utf-8')
            (consumer / 'App.nsproj').write_text(project('T19ComplejosConsumer', references, 'Main.ns'),
                                                 encoding='utf-8')
            try:
                if args.dll:
                    shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
                build_and_run(f'precompilado-{tag}-{visibility}', consumer, opt, expected)
            finally:
                if visibility == 'sin-encabezado':
                    (case / 'api.h.oculto').rename(case / 'api.h')
        print(f'{language} O{opt}: NativeSharp y Clang devuelven {expected}, directo y desde la plantilla precompilada '
              'con y sin encabezado', flush=True)

# A genuine rejection, by Clang too. / Un rechazo genuino, tambien en Clang.
negative = root / 'negativo-puntero'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nvoid fscale(float _Complex* z, float k);\n'
                                'double _Complex cmk(double re, double im);\n', encoding='utf-8')
(negative / 'api.c').write_text('#include "api.h"\nvoid fscale(float _Complex* z, float k) { }\n'
                                'double _Complex cmk(double re, double im) { return __builtin_complex(re, im); }\n',
                                encoding='utf-8')
run('negativo-api', [*CC, '-c', 'api.c', '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
(negative / 'control.c').write_text('#include "api.h"\nvoid g(void) { double _Complex z = cmk(1, 2); '
                                    'fscale(&z, 2.0f); }\n', encoding='utf-8')
clang_output = run('negativo-clang', [*CC, '-Werror', '-c', 'control.c', '-o', 'control.obj'], negative,
                   expected=1).stdout
if 'incompatible pointer types' not in clang_output:
    print(clang_output[-2000:], flush=True)
    raise SystemExit(f'negativo: Clang no da el rechazo esperado; evidencia: {root}')
(negative / 'Main.ns').write_text('int Main() { var z = cmk(1.0, 2.0); fscale(&z, 2.0f); return 0; }\n',
                                  encoding='utf-8')
(negative / 'App.nsproj').write_text(project('T19ComplejosNeg', '<CppLibraryReference Header="api.h" Language="C" '
                                     'Standard="c17" Library="api.lib" />', 'Main.ns'), encoding='utf-8')
output = run('negativo-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                              '--no-artifact-cache', '-O0', '-o', 'app.exe'], negative, expected=1).stdout
if "no viable overload of namespace function 'fscale'" not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo: falta el diagnostico esperado; evidencia: {root}')
print('negativo puntero: rechazado, igual que Clang', flush=True)
print('OK: valores C _Complex, frente a Clang; evidencia:', root)
