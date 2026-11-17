"""T19-001: parametros C de arreglo de longitud variable, frente a Clang 23.1.0.

Un parametro `T a[n]` es `T*`, como en C. Uno de varias dimensiones (`double a[n][m]`, `double a[*][*]`,
`double (*rows)[m]`) es en C un puntero a un VLA; NativeSharp lo toma como un puntero al elemento que queda despues
de la ultima dimension variable, en orden por filas, como dispone un VLA NativeSharp `T[n, m]`: `double*`, o
`double[2]*` si dentro quedan dimensiones constantes (`double a[n][m][2]`). El calificador `const` del elemento
se conserva. Los callbacks C con esos parametros tambien funcionan.

Compara un control C con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL, directo y
desde una plantilla precompilada con y sin encabezado. Rechazos genuinos, tambien en Clang: escribir por un
pasar un `const double*` a un parametro `double a[n][m]` (descarta `const`) y un `int*` a un parametro
`double a[n][m]`.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-vla-parametros-', dir='A:/temp'))
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
HEADER = '''API int sum_n(int n, const int a[n]);
API int first_s(int n, int a[static 2]);
API double sum2(int n, int m, double a[n][m]);
API double pick2(int n, int m, double a[*][*]);
API double sum3(int n, int m, double a[n][m][2]);
API void fill2(int n, int m, double a[n][m]);
API double rows2(int n, int m, double (*rows)[m]);
API double csum(int n, int m, const double a[n][m]);
typedef double (*Reducer)(int n, int m, double a[*][*]);
API double apply(Reducer f, int n, int m, double* a);
typedef double (*Reducer3)(int n, double a[*][3]);
API double apply3(Reducer3 f, int n, double* a);
'''
SOURCE = '''#include "api.h"
__declspec(selectany) int _fltused = 0;
int sum_n(int n, const int a[n]) { int s = 0; for (int i = 0; i < n; i++) s += a[i] * (i + 1); return s; }
int first_s(int n, int a[static 2]) { return a[0] * 10 + a[1] + n; }
double sum2(int n, int m, double a[n][m])
{ double s = 0; for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) s += a[i][j] * (i + 2); return s; }
double pick2(int n, int m, double a[n][m]) { return a[n - 1][m - 1] * 3 + a[0][1]; }
double sum3(int n, int m, double a[n][m][2])
{ double s = 0; for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) s += a[i][j][1] - a[i][j][0] * 2;
  return s; }
void fill2(int n, int m, double a[n][m])
{ for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) a[i][j] = i * 7 + j; }
double rows2(int n, int m, double (*rows)[m]) { return rows[n - 1][0] + rows[0][m - 1] * 5; }
double csum(int n, int m, const double a[n][m]) { return a[1][m - 1] + a[0][0] + n; }
double apply(Reducer f, int n, int m, double* a) { return f(n, m, (double (*)[m])a) + 1; }
double apply3(Reducer3 f, int n, double* a) { return f(n, (double (*)[3])a) * 2; }
'''
# The same statements in both languages; `mix` folds each result in. The C arrays have the shape the
# parameter names; NativeSharp passes a raw pointer to the first element or a NativeSharp VLA.
# Las mismas sentencias en ambos lenguajes; `mix` suma cada resultado.
STEPS = [
    ('int v[4] = {3, 1, 4, 1}; mix(sum_n(4, v)); mix(first_s(2, v));',
     'int[4] v; v[0] = 3; v[1] = 1; v[2] = 4; v[3] = 1; mix(sum_n(4, &v[0])); mix(first_s(2, &v[0]));'),
    ('double a[2][3]; for (int i = 0; i < 6; i++) a[i / 3][i % 3] = i + 1; mix((int)sum2(2, 3, a)); '
     'mix((int)pick2(2, 3, a)); mix((int)rows2(2, 3, a)); mix((int)csum(2, 3, a));',
     'double[6] a; for (int i = 0; i < 6; i++) { a[i] = i + 1; } mix((int)sum2(2, 3, &a[0])); '
     'mix((int)pick2(2, 3, &a[0])); mix((int)rows2(2, 3, &a[0])); mix((int)csum(2, 3, &a[0]));'),
    ('int n = 3; int m = 2; double b[3][2]; fill2(n, m, b); mix((int)(b[2][1] + b[1][0])); '
     'mix((int)sum2(n, m, b));',
     'int n = 3; int m = 2; double[n, m] b; fill2(n, m, b); mix((int)(b[2, 1] + b[1, 0])); '
     'mix((int)sum2(n, m, b));'),
    ('double c[2][2][2]; for (int i = 0; i < 8; i++) c[i / 4][(i / 2) % 2][i % 2] = i * 3; '
     'mix((int)sum3(2, 2, c));',
     'double[8] c; for (int i = 0; i < 8; i++) { c[i] = i * 3; } mix((int)sum3(2, 2, (double[2]*)&c[0]));'),
    ('double d[2][3] = {{1, 2, 3}, {4, 5, 6}}; mix((int)apply(reduce, 2, 3, &d[0][0])); '
     'mix((int)apply3(reduce3, 2, &d[0][0]));',
     'double[6] d; for (int i = 0; i < 6; i++) { d[i] = i + 1; } '
     'Reducer r = (int n, int m, double* a) => a[n * m - 1] * 2 + a[m] + n; mix((int)apply(r, 2, 3, &d[0])); '
     'Reducer3 r3 = (int n, double[3]* a) => a[1][2] - a[0][1] + n; mix((int)apply3(r3, 2, &d[0]));'),
]
NS_MIX = ('namespace T19V { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = ('__declspec(selectany) int _fltused = 0;\n'
         '#include <stddef.h>\n'
         'void* memset(void* d, int c, size_t n) { volatile unsigned char* p = d; while (n--) *p++ = (unsigned char)c; '
         'return d; }\n'
         'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n'
         'static double reduce(int n, int m, double a[n][m]) { return a[n - 1][m - 1] * 2 + a[1][0] + n; }\n'
         'static double reduce3(int n, double a[n][3]) { return a[1][2] - a[0][1] + n; }\n')


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


def reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C" Standard="c17" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


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
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(API + HEADER, encoding='utf-8')
    (case / 'api.c').write_text(SOURCE, encoding='utf-8')
    run(f'O{opt}-api', [*CC, '-O' + str(opt), *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.c', '-o',
                        'api.obj'], case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                            '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T19_IMPORT 1\n' + API + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    (case / 'control.c').write_text('#include "api.h"\n' + C_MIX + 'int main(void) { ' + c_body(STEPS) + ' }\n',
                                    encoding='utf-8')
    run(f'O{opt}-control', [*CC, '-O' + str(opt), 'control.c', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], case)
    expected = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text(NS_MIX + 'int Main() { ' + ns_body(STEPS) + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T19VlaParametros', reference('../'), 'Main.ns'), encoding='utf-8')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected)
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<Q>('
                                          'Q unused) { ' + ns_body(STEPS) + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T19VlaParametrosProducer', reference('../'), 'Producer.ns',
                                                      True), encoding='utf-8')
    run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                             '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
                             '-o', 'Producer.lib'], producer)
    for visibility in ('con-encabezado', 'sin-encabezado'):
        consumer = case / visibility
        consumer.mkdir()
        references = '<LibraryReference Include="../producer/Producer.lib" />'
        if visibility == 'con-encabezado':
            references += reference('../')
        else:
            (case / 'api.h').rename(case / 'api.h.oculto')
        (consumer / 'Main.ns').write_text('int Main() { return T19P.Bridge.Run<int>(0); }\n', encoding='utf-8')
        (consumer / 'App.nsproj').write_text(project('T19VlaParametrosConsumer', references, 'Main.ns'),
                                             encoding='utf-8')
        try:
            if args.dll:
                shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
            build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, expected)
        finally:
            if visibility == 'sin-encabezado':
                (case / 'api.h.oculto').rename(case / 'api.h')
    print(f'O{opt}: NativeSharp y C devuelven {expected}, directo y desde la plantilla precompilada con y sin '
          'encabezado', flush=True)

# Genuine rejections, by Clang too. / Rechazos genuinos, tambien en Clang.
NEGATIVES = [
    ('descarta-const', 'const double* cbuf(void); void fill2(int n, int m, double a[n][m]);',
     'static const double b[4]; const double* cbuf(void) { return b; } void fill2(int n, int m, double a[n][m]) { }',
     'void g(void) { fill2(2, 2, cbuf()); }', 'incompatible pointer types',
     'fill2(2, 2, cbuf()); return 0;', "no viable overload of namespace function 'fill2'"),
    ('tipo-elemento', 'double s2(int n, int m, double a[n][m]);',
     'double s2(int n, int m, double a[n][m]) { return 0; }',
     'int g(void) { int x[2][2]; return (int)s2(2, 2, x); }', 'incompatible pointer types',
     'int[4] x; return (int)s2(2, 2, &x[0]);', "no viable overload of namespace function 's2'"),
]
for name, header, implementation, c_use, c_message, ns_body_text, ns_message in NEGATIVES:
    negative = root / ('negativo-' + name)
    negative.mkdir()
    (negative / 'api.h').write_text('#pragma once\n' + header + '\n', encoding='utf-8')
    (negative / 'api.c').write_text('#include "api.h"\n' + implementation + '\n', encoding='utf-8')
    run(name + '-api', [*CC, '-c', 'api.c', '-o', 'api.obj'], negative)
    run(name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
    (negative / 'control.c').write_text('#include "api.h"\n' + c_use + '\n', encoding='utf-8')
    clang_output = run(name + '-clang', [*CC, '-Werror', '-c', 'control.c', '-o',
                                         'control.obj'], negative, expected=1).stdout
    if c_message not in clang_output:
        print(clang_output[-2000:], flush=True)
        raise SystemExit(f'{name}: Clang no da el rechazo esperado; evidencia: {root}')
    (negative / 'Main.ns').write_text('int Main() { ' + ns_body_text + ' }\n', encoding='utf-8')
    (negative / 'App.nsproj').write_text(project('T19VlaNeg', '<CppLibraryReference Header="api.h" Language="C" '
                                         'Standard="c17" Library="api.lib" />', 'Main.ns'), encoding='utf-8')
    output = run(name + '-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                 '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', 'app.exe'],
                 negative, expected=1).stdout
    if ns_message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado, igual que Clang', flush=True)
print('OK: parametros C de arreglo de longitud variable, frente a Clang; evidencia:', root)
