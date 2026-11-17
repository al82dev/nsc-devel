"""T19-002: `sizeof` de un tipo C como constante entera y `new` sobre structs y unions C, frente a Clang 23.1.0.

- `sizeof` y `alignof` de un tipo C (struct, union) o de un entero incorporado son constantes: como un literal,
  se convierten implicitamente a un tipo entero en el que caben (`int`, `unsigned`, `short`, `long long`), como
  C convierte `sizeof(z_stream)` en `deflateInit_`. El `sizeof` de un tipo NativeSharp sigue siendo `nuint`.
- `new S()` sin argumentos sobre un struct o una union C es su valor cero, como `(S){0}`: en un local, en una
  asignacion, en un campo y como argumento por valor de una funcion C.

Compara un control C con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL, directo y
desde una plantilla precompilada con y sin encabezado. Rechazo genuino, tambien en Clang: `sizeof` de un tipo C
incompleto.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-c-valores-', dir='A:/temp'))
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
HEADER = '''typedef struct S { int a; double b; char c; } S;
typedef union U { int i; float f; } U;
typedef struct W { S s; int k; } W;
typedef struct Big { char b[300]; } Big;
API int take(int n);
API unsigned takeu(unsigned n);
API int takes(short n);
API long long takel(long long n);
API int sum(S s);
API int ui(U u);
'''
SOURCE = '''#include "api.h"
__declspec(selectany) int _fltused = 0;
int take(int n) { return n; }
unsigned takeu(unsigned n) { return n * 2u; }
int takes(short n) { return n + 1; }
long long takel(long long n) { return n * 3; }
int sum(S s) { return s.a + (int)s.b + s.c; }
int ui(U u) { return u.i + 2; }
'''
# The same statements in both languages; `mix` folds each result in. / Las mismas sentencias en ambos lenguajes.
STEPS = [
    ('mix(take(sizeof(S))); mix((int)takeu(sizeof(U))); mix(takes(sizeof(W))); mix((int)takel(sizeof(Big)));',
     'mix(take(sizeof(S))); mix((int)takeu(sizeof(U))); mix(takes(sizeof(W))); mix((int)takel(sizeof(Big)));'),
    ('int n = sizeof(S); short m = sizeof(W); mix(n + m); mix(take(_Alignof(S)) * 100 + take(_Alignof(U)));',
     'int n = sizeof(S); short m = sizeof(W); mix(n + m); mix(take(alignof(S)) * 100 + take(alignof(U)));'),
    ('mix(take(sizeof(long long)) + take(sizeof(int)) + take(sizeof(short)));',
     'mix(take(sizeof(long)) + take(sizeof(int)) + take(sizeof(short)));'),
    ('S s = {0}; s.a = 4; mix(s.a + (int)s.b + s.c);', 'S s = new S(); s.a = 4; mix(s.a + (int)s.b + s.c);'),
    ('S t; t.a = 9; t = (S){0}; mix(t.a + 1); mix(sum((S){0}) + 3);',
     'S t; t.a = 9; t = new S(); mix(t.a + 1); mix(sum(new S()) + 3);'),
    ('U u = {0}; mix(ui(u)); mix(ui((U){0}));', 'U u = new U(); mix(ui(u)); mix(ui(new U()));'),
    ('W w; w.k = 3; w.s = (S){0}; mix(w.s.a + w.k);', 'W w; w.k = 3; w.s = new S(); mix(w.s.a + w.k);'),
]
NS_MIX = ('namespace T19V { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = ('__declspec(selectany) int _fltused = 0;\n'
         '#include <stddef.h>\n'
         'void* memset(void* d, int c, size_t n) { volatile unsigned char* p = d; while (n--) *p++ = (unsigned char)c; '
         'return d; }\n'
         'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n')


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
    (app / 'App.nsproj').write_text(project('T19CValores', reference('../'), 'Main.ns'), encoding='utf-8')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected)
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<Q>('
                                          'Q unused) { ' + ns_body(STEPS) + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T19CValoresProducer', reference('../'), 'Producer.ns', True),
                                              encoding='utf-8')
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
        (consumer / 'App.nsproj').write_text(project('T19CValoresConsumer', references, 'Main.ns'),
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

# The size of an incomplete C type is rejected, by Clang too. / El tamano de un tipo C incompleto se rechaza,
# tambien en Clang.
negative = root / 'negativo'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\ntypedef struct Opaque Opaque;\nint take(int n);\n', encoding='utf-8')
(negative / 'api.c').write_text('#include "api.h"\nint take(int n) { return n; }\n', encoding='utf-8')
run('negativo-api', [*CC, '-c', 'api.c', '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
(negative / 'control.c').write_text('#include "api.h"\nint main(void) { return take(sizeof(Opaque)); }\n',
                                    encoding='utf-8')
clang_output = run('negativo-clang', [*CC, '-c', 'control.c', '-o', 'control.obj'], negative, expected=1).stdout
if 'incomplete type' not in clang_output:
    print(clang_output[-2000:], flush=True)
    raise SystemExit(f'negativo: Clang no rechaza el tipo incompleto; evidencia: {root}')
(negative / 'Main.ns').write_text('int Main() { return take(sizeof(Opaque)); }\n', encoding='utf-8')
(negative / 'App.nsproj').write_text(project('T19CValoresNeg', '<CppLibraryReference Header="api.h" Language="C" '
                                     'Standard="c17" Library="api.lib" />', 'Main.ns'), encoding='utf-8')
output = run('negativo-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                              '--no-artifact-cache', '-O0', '-o', 'app.exe'], negative, expected=1).stdout
if 'incomplete' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo: falta el diagnostico de tipo incompleto; evidencia: {root}')
print('negativo sizeof-incompleto: rechazado, igual que Clang', flush=True)
print('OK: sizeof de tipos C como constante y new sobre structs y unions C, frente a Clang; evidencia:', root)
