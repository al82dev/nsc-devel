"""T19-003: macros C y C++ de funcion y macros que dan cadenas, punteros, records o tipos, frente a Clang 23.1.0.

- Una llamada a una macro de funcion la expande Clang dentro de un envoltorio cuyos parametros tienen los tipos de
  los argumentos de la llamada (static inline en C, inline en el namespace global en C++): una expresion devuelve
  su valor y una sentencia (`do { ... } while (0)`) devuelve void. Un argumento `ref x` se pasa por direccion en C
  y por referencia en C++, asi la macro puede asignarlo. Macros variadicas, sin argumentos, anidadas, que usan
  otras macros (como `deflateInit` de zlib), anidadas con tipos distintos (`SQ(HALF(3))`, deducidas en rondas),
  que leen un record y, en C++, que devuelven un record por valor y llaman funciones de un namespace.
- Una macro de objeto C o C++ cuyo valor es un puntero, una cadena o un record constante es una constante con ese
  tipo, y una que nombra un tipo es un alias.
- Desde una plantilla precompilada, la macro viaja con la fuente preprocesada del productor, asi que tambien se
  expande sin el encabezado.

Compara un control de Clang con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL,
directo y desde una plantilla precompilada con y sin encabezado. Rechazos genuinos, tambien en Clang: una macro
llamada con un argumento de tipo que su expansion no acepta, y con un numero de argumentos que no toma.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-macros-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

API = '''#pragma once
#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
C_HEADER = '''API int init_(int* s, int level, const char* v, int size);
API int slen(const char* s);
API int sum3(int a, int b, int c);
API int next_id(void);
typedef struct P { int a; int b; } P;
#define VERSION "1.2.13"
#define init(s, level) init_((s), (level), VERSION, (int)sizeof(P))
#define SQ(x) ((x) * (x))
#define HALF(x) ((x) / 2.0)
#define SETZ(p) do { *(p) = 5; } while (0)
#define INC(x) ((x)++)
#define SWAPI(a, b) do { int t = (a); (a) = (b); (b) = t; } while (0)
#define SUM(...) sum3(__VA_ARGS__)
#define NEXT() next_id()
#define PSUM(p) ((p).a * 10 + (p).b)
#define NIL ((int*)0)
#define VPTR ((const char*)VERSION)
#define ORIGIN ((P){3, 4})
#define REAL double
#define IPTR int*
'''
C_SOURCE = '''#include "api.h"
__declspec(selectany) int _fltused = 0;
int init_(int* s, int level, const char* v, int size) { *s = level * 10 + size + slen(v); return 7; }
int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }
int sum3(int a, int b, int c) { return a + b * 10 + c * 100; }
static int counter = 40;
int next_id(void) { return ++counter; }
'''
CPP_HEADER = '''namespace A {
struct P { int a; int b; };
API int init_(int* s, int level, const char* v, int size);
API int slen(const char* s);
API int sum3(int a, int b, int c);
API int next_id();
inline int twice(int x) { return 2 * x; }
}
#define init(s, level) A::init_((s), (level), "1.2.13", (int)sizeof(A::P))
#define SQ(x) ((x) * (x))
#define HALF(x) ((x) / 2.0)
#define TWICE(x) A::twice(x)
#define SETZ(p) do { *(p) = 5; } while (0)
#define INC(x) ((x)++)
#define SWAPI(a, b) do { int t = (a); (a) = (b); (b) = t; } while (0)
#define SUM(...) A::sum3(__VA_ARGS__)
#define NEXT() A::next_id()
#define PSUM(p) ((p).a * 10 + (p).b)
#define MAKEP(x, y) (A::P{(x), (y)})
#define VERSION "1.2.13"
#define NIL ((int*)0)
#define VPTR ((const char*)VERSION)
#define ORIGIN (A::P{3, 4})
#define REAL double
#define IPTR int*
'''
CPP_SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace A {
int init_(int* s, int level, const char* v, int size) { *s = level * 10 + size + slen(v); return 7; }
int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }
int sum3(int a, int b, int c) { return a + b * 10 + c * 100; }
static int counter = 40;
int next_id() { return ++counter; }
}
'''
# The same statements in both languages: (C or C++, NativeSharp). / Las mismas sentencias en ambos lenguajes.
C_STEPS = [
    ('mix(SQ(7)); mix((int)SQ(1.5)); mix(SQ(SQ(2))); mix((int)SQ(HALF(3)));',
     'mix(SQ(7)); mix((int)SQ(1.5)); mix(SQ(SQ(2))); mix((int)SQ(HALF(3)));'),
    ('int s = 0; mix(init(&s, 6)); mix(s);', 'int s = 0; mix(init(&s, 6)); mix(s);'),
    ('int v = 1; SETZ(&v); mix(v);', 'int v = 1; SETZ(&v); mix(v);'),
    ('int w = 3; INC(w); INC(w); int a = 1; int b = 2; SWAPI(a, b); mix(w * 100 + a * 10 + b);',
     'int w = 3; INC(ref w); INC(ref w); int a = 1; int b = 2; SWAPI(ref a, ref b); mix(w * 100 + a * 10 + b);'),
    ('mix(SUM(1, 2, 3)); mix(NEXT() + NEXT());', 'mix(SUM(1, 2, 3)); mix(NEXT() + NEXT());'),
    ('mix(slen(VERSION));', 'mix(slen(VERSION));'),
    ('int* p = NIL; mix((p == 0 ? 10 : 0) + slen(VPTR));',
     'int* p = NIL; mix((p == null ? 10 : 0) + slen(VPTR));'),
    ('P o = ORIGIN; mix(o.a * 10 + o.b); mix(PSUM(o));', 'P o = ORIGIN; mix(o.a * 10 + o.b); mix(PSUM(o));'),
    ('REAL d = 2.5; int x = 4; IPTR px = &x; mix((int)(d * 2) + *px);',
     'REAL d = 2.5; int x = 4; IPTR px = &x; mix((int)(d * 2) + *px);'),
]
CPP_STEPS = [
    ('mix(SQ(7)); mix((int)SQ(1.5)); mix(SQ(SQ(2))); mix(TWICE(21)); mix((int)SQ(HALF(3)));',
     'mix(SQ(7)); mix((int)SQ(1.5)); mix(SQ(SQ(2))); mix(TWICE(21)); mix((int)SQ(HALF(3)));'),
    ('int s = 0; mix(init(&s, 6)); mix(s);', 'int s = 0; mix(init(&s, 6)); mix(s);'),
    ('int v = 1; SETZ(&v); mix(v);', 'int v = 1; SETZ(&v); mix(v);'),
    ('int w = 3; INC(w); INC(w); int a = 1; int b = 2; SWAPI(a, b); mix(w * 100 + a * 10 + b);',
     'int w = 3; INC(ref w); INC(ref w); int a = 1; int b = 2; SWAPI(ref a, ref b); mix(w * 100 + a * 10 + b);'),
    ('mix(SUM(1, 2, 3)); mix(NEXT() + NEXT());', 'mix(SUM(1, 2, 3)); mix(NEXT() + NEXT());'),
    ('A::P o = MAKEP(3, 4); mix(o.a * 10 + o.b); mix(PSUM(o));',
     'A.P o = MAKEP(3, 4); mix(o.a * 10 + o.b); mix(PSUM(o));'),
    ('mix(A::slen(VERSION));', 'mix(A.slen(VERSION));'),
    ('int* p = NIL; mix((p == 0 ? 10 : 0) + A::slen(VPTR));',
     'int* p = NIL; mix((p == null ? 10 : 0) + A.slen(VPTR));'),
    ('A::P g = ORIGIN; mix(g.a * 10 + g.b);', 'A.P g = ORIGIN; mix(g.a * 10 + g.b);'),
    ('REAL d = 2.5; int x = 4; IPTR px = &x; mix((int)(d * 2) + *px);',
     'REAL d = 2.5; int x = 4; IPTR px = &x; mix((int)(d * 2) + *px);'),
]
NS_MIX = ('namespace T19V { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = ('__declspec(selectany) int _fltused = 0;\n'
         '#include <stddef.h>\n'
         'void* memset(void* d, int c, size_t n) { volatile unsigned char* p = d; while (n--) *p++ = (unsigned char)c; '
         'return d; }\n'
         'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n')
CPP_MIX = ('extern "C" __declspec(selectany) int _fltused = 0;\n'
           '#include <stddef.h>\n'
           'extern "C" void* memset(void* d, int c, size_t n) { volatile unsigned char* p = (unsigned char*)d; '
           'while (n--) *p++ = (unsigned char)c; return d; }\n'
           'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n')
LANGUAGES = [
    {'name': 'C', 'tag': 'c', 'header': C_HEADER, 'source': C_SOURCE, 'mix': C_MIX, 'steps': C_STEPS,
     'cc': ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17'], 'ext': 'c',
     'reference': 'Language="C" Standard="c17"'},
    {'name': 'C++', 'tag': 'cpp', 'header': CPP_HEADER, 'source': CPP_SOURCE, 'mix': CPP_MIX, 'steps': CPP_STEPS,
     'cc': ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20'], 'ext': 'cpp',
     'reference': 'Language="C++" Standard="c++20"'},
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


def reference(language, prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" ' + language['reference'] + ' Library="'
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
for language in LANGUAGES:
    tag = language['tag']
    cc = language['cc']
    for opt in range(4):
        case = root / f'{tag}-O{opt}'
        case.mkdir()
        (case / 'api.h').write_text(API + language['header'], encoding='utf-8')
        (case / ('api.' + language['ext'])).write_text(language['source'], encoding='utf-8')
        run(f'{tag}-O{opt}-api', [*cc, '-O' + str(opt), *(['-DT19_EXPORT'] if args.dll else []), '-c',
                                  'api.' + language['ext'], '-o', 'api.obj'], case)
        if args.dll:
            run(f'{tag}-O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                      '/implib:api.lib', 'api.obj'], case)
            (case / 'api.h').write_text('#define T19_IMPORT 1\n' + API + language['header'], encoding='utf-8')
        else:
            run(f'{tag}-O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        (case / ('control.' + language['ext'])).write_text(
            '#include "api.h"\n' + language['mix'] + 'int main(void) { ' + c_body(language['steps']) + ' }\n',
            encoding='utf-8')
        run(f'{tag}-O{opt}-control', [*cc, '-O' + str(opt), 'control.' + language['ext'], 'api.lib', '-fuse-ld=lld',
                                      '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'], case)
        expected = run(f'{tag}-O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
        app = case / 'app'
        app.mkdir()
        (app / 'Main.ns').write_text(NS_MIX + 'int Main() { ' + ns_body(language['steps']) + ' }\n',
                                     encoding='utf-8')
        (app / 'App.nsproj').write_text(project('T19Macros', reference(language, '../'), 'Main.ns'),
                                        encoding='utf-8')
        if args.dll:
            shutil.copyfile(case / 'api.dll', app / 'api.dll')
        build_and_run(f'{tag}-directo-O{opt}', app, opt, expected)
        producer = case / 'producer'
        producer.mkdir()
        (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int '
                                              'Run<Q>(Q unused) { ' + ns_body(language['steps']) + ' } } }\n',
                                              encoding='utf-8')
        (producer / 'Producer.nsproj').write_text(project('T19MacrosProducer', reference(language, '../'),
                                                          'Producer.ns', True), encoding='utf-8')
        run(f'{tag}-O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                                       '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
                                       '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility
            consumer.mkdir()
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += reference(language, '../')
            else:
                (case / 'api.h').rename(case / 'api.h.oculto')
            (consumer / 'Main.ns').write_text('int Main() { return T19P.Bridge.Run<int>(0); }\n', encoding='utf-8')
            (consumer / 'App.nsproj').write_text(project('T19MacrosConsumer', references, 'Main.ns'),
                                                 encoding='utf-8')
            try:
                if args.dll:
                    shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
                build_and_run(f'{tag}-precompilado-O{opt}-{visibility}', consumer, opt, expected)
            finally:
                if visibility == 'sin-encabezado':
                    (case / 'api.h.oculto').rename(case / 'api.h')
        print(f'{language["name"]} O{opt}: NativeSharp y Clang devuelven {expected}, directo y desde la plantilla '
              'precompilada con y sin encabezado', flush=True)

# Genuine rejections, by Clang too. / Rechazos genuinos, tambien en Clang.
NEGATIVES = [
    (LANGUAGES[0], 'tipo-argumento', '#define FIELD(p) ((p).a)\ntypedef struct P { int a; } P;',
     'int use(void) { return FIELD(3); }', 'member reference base type', 'return FIELD(3);',
     "Clang cannot expand the C/C++ function-like macro 'FIELD'"),
    (LANGUAGES[0], 'aridad', '#define SQ(x) ((x) * (x))', 'int use(void) { return SQ(1, 2); }', 'too many arguments',
     'return SQ(1, 2);', 'the macro takes 1 arguments'),
    (LANGUAGES[1], 'tipo-argumento', '#define FIELD(p) ((p).a)\nstruct P { int a; };',
     'int use() { return FIELD(3); }', 'member reference base type', 'return FIELD(3);',
     "Clang cannot expand the C/C++ function-like macro 'FIELD'"),
    (LANGUAGES[1], 'aridad', '#define SQ(x) ((x) * (x))', 'int use() { return SQ(1, 2); }', 'too many arguments',
     'return SQ(1, 2);', 'the macro takes 1 arguments'),
]
for language, name, header, c_use, c_message, ns_body_text, ns_message in NEGATIVES:
    tag = language['tag'] + '-' + name
    negative = root / ('negativo-' + tag)
    negative.mkdir()
    (negative / 'api.h').write_text('#pragma once\n' + header + '\nint dummy(void);\n', encoding='utf-8')
    (negative / ('api.' + language['ext'])).write_text('#include "api.h"\nint dummy(void) { return 0; }\n',
                                                       encoding='utf-8')
    run(tag + '-api', [*language['cc'], '-c', 'api.' + language['ext'], '-o', 'api.obj'], negative)
    run(tag + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
    (negative / ('control.' + language['ext'])).write_text('#include "api.h"\n' + c_use + '\n', encoding='utf-8')
    clang_output = run(tag + '-clang', [*language['cc'], '-c', 'control.' + language['ext'], '-o', 'control.obj'],
                       negative, expected=1).stdout
    if c_message not in clang_output:
        print(clang_output[-2000:], flush=True)
        raise SystemExit(f'{tag}: Clang no da el rechazo esperado; evidencia: {root}')
    (negative / 'Main.ns').write_text('int Main() { ' + ns_body_text + ' }\n', encoding='utf-8')
    (negative / 'App.nsproj').write_text(project('T19MacrosNeg', '<CppLibraryReference Header="api.h" ' +
                                                 language['reference'] + ' Library="api.lib" />', 'Main.ns'),
                                         encoding='utf-8')
    output = run(tag + '-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', 'app.exe'],
                 negative, expected=1).stdout
    if ns_message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{tag}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {language["name"]} {name}: rechazado, igual que Clang', flush=True)
print('OK: macros C y C++ de funcion, de valor y de tipo, frente a Clang; evidencia:', root)
