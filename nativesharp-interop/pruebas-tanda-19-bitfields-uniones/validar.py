"""T19-004: bit-fields de tipo enum o de mas de 64 bits, `__int128` y unions C++, frente a Clang 23.1.0.

- Un bit-field de tipo enum (C y C++, con ambito o sin el) se lee y escribe por el tipo entero subyacente del
  enum, con su signo: en el ABI de Microsoft un enum C es `int`, asi que `BLUE` en `enum Color c : 2` se lee -2.
- `__int128` y `unsigned __int128` son `i128` y `u128` en campos, bit-fields de mas de 64 bits (`big : 100`),
  parametros (por la direccion de una copia alineada a 16) y resultados (en XMM0), como en el plan de Clang.
- Una union C++ trivial con un miembro record y una union anonima trivial dentro de una clase C++ tienen la
  semantica de una union C.

Compara un control de Clang con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL,
directo y desde una plantilla precompilada con y sin encabezado. Rechazo genuino, tambien en Clang: asignar un
entero a un bit-field de un enum con ambito.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-bitfields-', dir='A:/temp'))
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
C_HEADER = '''enum Color { RED, GREEN, BLUE };
enum Sign { NEG = -2, ZERO = 0, POS = 1 };
typedef struct S { enum Color c : 2; unsigned x : 3; enum Color d : 3; enum Sign v : 2; } S;
typedef struct W { unsigned __int128 big : 100; unsigned x : 4; __int128 neg : 70; } W;
typedef struct V { unsigned __int128 v; int t; } V;
typedef struct K { int tag; union { int i; float f; }; } K;
API int useS(S* s);
API int useW(W* w);
API unsigned __int128 twice(unsigned __int128 a);
API __int128 addi(__int128 a, int b);
API int useK(K* k);
'''
C_SOURCE = '''#include "api.h"
__declspec(selectany) int _fltused = 0;
int useS(S* s) { return s->c * 10 + s->x + s->d * 100 + s->v * 1000; }
int useW(W* w) { return (int)(w->big >> 94) + w->x * 16 + (int)(w->neg >> 66); }
unsigned __int128 twice(unsigned __int128 a) { return a * 2; }
__int128 addi(__int128 a, int b) { return a + b; }
int useK(K* k) { return k->tag * 100 + k->i; }
'''
CPP_HEADER = '''enum class E : unsigned char { A = 1, B = 2, C = 3 };
enum Small : signed char { LOW = -1, HIGH = 1 };
struct S { E e : 2; int y : 5; E f : 2; Small m : 2; };
struct P { int a; int b; };
union U { P p; long long w; };
struct K { int tag; union { int i; float f; }; };
struct W { unsigned __int128 big : 100; unsigned x : 4; };
API int useS(S* s);
API int useU(U* u);
API int useK(K* k);
API int useW(W* w);
API unsigned __int128 twice(unsigned __int128 a);
API S* makeS();
API U* makeU();
API K* makeK();
API W* makeW();
'''
CPP_SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int useS(S* s) { return (int)s->e * 10 + s->y + (int)s->f * 100 + (int)s->m * 1000; }
int useU(U* u) { return u->p.a * 10 + u->p.b; }
int useK(K* k) { return k->tag * 100 + k->i; }
int useW(W* w) { return (int)(w->big >> 94) + w->x * 16; }
unsigned __int128 twice(unsigned __int128 a) { return a * 2; }
static S gs; static U gu; static K gk; static W gw;
// Zero-initialized statics; each one is used once per program. / Estaticos en cero; cada uno se usa una vez.
S* makeS() { return &gs; }
U* makeU() { return &gu; }
K* makeK() { return &gk; }
W* makeW() { return &gw; }
'''
# The same statements in both languages: (C or C++, NativeSharp). / Las mismas sentencias en ambos lenguajes.
C_STEPS = [
    ('S s = {0}; s.c = BLUE; s.x = 5; s.d = GREEN; s.v = NEG; mix(useS(&s)); mix((int)s.c + 10); mix((int)s.d);'
     ' mix((int)s.v + 10);',
     'S s = new S(); s.c = BLUE; s.x = 5; s.d = GREEN; s.v = NEG; mix(useS(&s)); mix((int)s.c + 10); mix((int)s.d);'
     ' mix((int)s.v + 10);'),
    ('W w = {0}; w.x = 3; w.big = (unsigned __int128)5 << 94; w.neg = -((__int128)3 << 66); mix(useW(&w));'
     ' mix((int)(w.big >> 94)); mix((int)(w.neg >> 66) + 10);',
     'W w = new W(); w.x = 3; w.big = (u128)5 << 94; w.neg = -((i128)3 << 66); mix(useW(&w));'
     ' mix((int)(w.big >> 94)); mix((int)(w.neg >> 66) + 10);'),
    ('V v = {0}; v.v = (unsigned __int128)3 << 70; v.t = 1; mix((int)(twice(v.v) >> 70) + v.t);'
     ' mix((int)(addi(-((__int128)1 << 80), 5) >> 80) + 10); mix((int)(addi((__int128)7, 5)));',
     'V v = new V(); v.v = (u128)3 << 70; v.t = 1; mix((int)(twice(v.v) >> 70) + v.t);'
     ' mix((int)(addi(-((i128)1 << 80), 5) >> 80) + 10); mix((int)(addi((i128)7, 5)));'),
    ('K k = {0}; k.tag = 2; k.i = 7; mix(useK(&k)); k.f = 1.5f; mix(k.i >> 20);',
     'K k = new K(); k.tag = 2; k.i = 7; mix(useK(&k)); k.f = 1.5f; mix(k.i >> 20);'),
]
# EN: the C++ objects come from the library, since a precompiled template body cannot yet create a C++ record with
#     `new` (T19-016). / ES: los objetos C++ vienen de la biblioteca, porque el cuerpo de una plantilla precompilada
#     todavia no crea un record C++ con `new` (T19-016).
CPP_STEPS = [
    ('S* s = makeS(); s->e = E::C; s->y = -3; s->f = E::A; s->m = LOW; mix(useS(s)); mix((int)s->e);'
     ' mix((int)s->m + 10);',
     'S* s = makeS(); s->e = E.C; s->y = -3; s->f = E.A; s->m = LOW; mix(useS(s)); mix((int)s->e);'
     ' mix((int)s->m + 10);'),
    ('U* u = makeU(); u->p.a = 3; u->p.b = 4; mix(useU(u)); mix((int)(u->w >> 32)); u->w = 9; mix(u->p.a);',
     'U* u = makeU(); u->p.a = 3; u->p.b = 4; mix(useU(u)); mix((int)(u->w >> 32)); u->w = 9; mix(u->p.a);'),
    ('K* k = makeK(); k->tag = 2; k->i = 7; mix(useK(k)); k->f = 1.5f; mix(k->i >> 20);',
     'K* k = makeK(); k->tag = 2; k->i = 7; mix(useK(k)); k->f = 1.5f; mix(k->i >> 20);'),
    ('W* w = makeW(); w->x = 3; w->big = (unsigned __int128)5 << 94; mix(useW(w));'
     ' mix((int)(twice(w->big) >> 95));',
     'W* w = makeW(); w->x = 3; w->big = (u128)5 << 94; mix(useW(w)); mix((int)(twice(w->big) >> 95));'),
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
        (app / 'App.nsproj').write_text(project('T19Bitfields', reference(language, '../'), 'Main.ns'),
                                        encoding='utf-8')
        if args.dll:
            shutil.copyfile(case / 'api.dll', app / 'api.dll')
        build_and_run(f'{tag}-directo-O{opt}', app, opt, expected)
        producer = case / 'producer'
        producer.mkdir()
        (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int '
                                              'Run<Q>(Q unused) { ' + ns_body(language['steps']) + ' } } }\n',
                                              encoding='utf-8')
        (producer / 'Producer.nsproj').write_text(project('T19BitfieldsProducer', reference(language, '../'),
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
            (consumer / 'App.nsproj').write_text(project('T19BitfieldsConsumer', references, 'Main.ns'),
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

# A genuine rejection, by Clang too: a scoped enum is not an integer. / Un rechazo genuino, tambien en Clang.
negative = root / 'negativo-enum-con-ambito'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nenum class E : unsigned char { A = 1 };\nstruct S { E e : 2; };\n'
                                'int dummy();\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nint dummy() { return 0; }\n', encoding='utf-8')
run('negativo-api', [*LANGUAGES[1]['cc'], '-c', 'api.cpp', '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
(negative / 'control.cpp').write_text('#include "api.h"\nint use() { S s{}; s.e = 1; return 0; }\n', encoding='utf-8')
clang_output = run('negativo-clang', [*LANGUAGES[1]['cc'], '-c', 'control.cpp', '-o', 'control.obj'], negative,
                   expected=1).stdout
if "assigning to 'E' from incompatible type 'int'" not in clang_output:
    print(clang_output[-2000:], flush=True)
    raise SystemExit(f'negativo: Clang no da el rechazo esperado; evidencia: {root}')
(negative / 'Main.ns').write_text('int Main() { S s = new S(); s.e = 1; return 0; }\n', encoding='utf-8')
(negative / 'App.nsproj').write_text(project('T19BitfieldsNeg', '<CppLibraryReference Header="api.h" ' +
                                             LANGUAGES[1]['reference'] + ' Library="api.lib" />', 'Main.ns'),
                                     encoding='utf-8')
output = run('negativo-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                              '--no-artifact-cache', '-O0', '-o', 'app.exe'], negative, expected=1).stdout
if "cannot implicitly convert assignment RHS from 'builtin:int' to 'type:T19BitfieldsNeg::E'" not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo: NSC no rechaza la asignacion; evidencia: {root}')
print('negativo enum con ambito: rechazado, igual que Clang', flush=True)
print('OK: bit-fields enum y de 128 bits, __int128 y unions C++, frente a Clang; evidencia:', root)
