"""T19-028: swiftcall, swiftasynccall e intel_ocl_bicc por puntero y con records por valor, frente a Clang 23.1.0.

- NativeSharp tiene tipos de funcion con estas convenciones: un puntero obtenido de C se llama con la convencion
  LLVM de Clang (swiftcc, swifttailcc, intel_ocl_bicc), y la direccion de una funcion C de la misma convencion se
  convierte a ese tipo.
- Records por valor:
  - con swiftcall y swiftasynccall, Clang los expande en piezas (coerce-and-expand), los pasa por la direccion de
    una copia o como un entero, y devuelve un struct de piezas; NSC sigue el plan de Clang de cada declaracion;
  - con intel_ocl_bicc, Clang usa la forma Win64, que NSC baja por su ruta de records Win64 con esa convencion.
- C con las tres convenciones; C++ con swiftcall y swiftasynccall (Clang no decora intel_ocl_bicc en C++).

Compara un control de Clang con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL,
directo y desde una plantilla precompilada con y sin encabezado. Rechazo genuino, tambien en Clang: una funcion
variadica swiftcall.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-convenciones-', dir='A:/temp'))
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


def header(cc, cpp):
    a = '__attribute__((' + cc + '))'
    open_ns, close_ns = ('namespace N {\n', '}\n') if cpp else ('', '')
    typedefs = ('struct R { int a; float b; double c; };\nstruct S { char c; short s; };\n'
                'struct B { int v[8]; };\nstruct G { int v[10]; };\n') if cpp else (
        'typedef struct R { int a; float b; double c; } R;\ntypedef struct S { char c; short s; } S;\n'
        'typedef struct B { int v[8]; } B;\ntypedef struct G { int v[10]; } G;\n')
    return (open_ns + typedefs +
            f'typedef int ({a} *P)(int, int);\n'
            f'typedef double ({a} *Q)(double, float, long long);\n'
            f'API int {a} add(int a, int b);\n'
            f'API double {a} mixd(double a, float b, long long c);\n'
            f'API P getp();\nAPI Q getq();\n'
            f'API R {a} mkr(int a, float b, double c);\n'
            f'API S {a} mks(char c, short s);\n'
            f'API R {a} rec(R r, int k);\n'
            f'API S {a} small(S s, char d);\n'
            f'API int {a} sumb(B b);\n'
            f'API B {a} makeb(int base);\n'
            f'API int {a} sumg(G g, int k);\n'
            f'API G {a} makeg(int base);\n' + close_ns)


def source(cc, cpp):
    a = '__attribute__((' + cc + '))'
    open_ns, close_ns = ('namespace N {\n', '}\n') if cpp else ('', '')
    fltused = 'extern "C" __declspec(selectany) int _fltused = 0;\n' if cpp else '__declspec(selectany) int _fltused = 0;\n'
    # The library brings its own memcpy for record copies. / La biblioteca trae su memcpy para copiar records.
    copy = '#include <stddef.h>\n' + ('extern "C" ' if cpp else '') + MEMCPY
    return ('#include "api.h"\n' + fltused + copy + open_ns +
            f'int {a} add(int a, int b) {{ return a * 10 + b; }}\n'
            f'double {a} mixd(double a, float b, long long c) {{ return a + b * 2 + (double)c; }}\n'
            'P getp() { return add; }\nQ getq() { return mixd; }\n'
            f'R {a} mkr(int a, float b, double c) {{ R r; r.a = a; r.b = b; r.c = c; return r; }}\n'
            f'S {a} mks(char c, short s) {{ S v; v.c = c; v.s = s; return v; }}\n'
            f'R {a} rec(R r, int k) {{ r.a += k; r.c = r.b * 2 + r.c; return r; }}\n'
            f'S {a} small(S s, char d) {{ s.c = (char)(s.c + d); s.s = (short)(s.s * 3); return s; }}\n'
            f'int {a} sumb(B b) {{ int s = 0; for (int i = 0; i < 8; ++i) s += b.v[i] * (i + 1); return s; }}\n'
            f'B {a} makeb(int base) {{ B b; for (int i = 0; i < 8; ++i) b.v[i] = base + i; return b; }}\n'
            f'int {a} sumg(G g, int k) {{ int s = k; for (int i = 0; i < 10; ++i) s += g.v[i] * (i + 2); return s; }}\n'
            f'G {a} makeg(int base) {{ G g; for (int i = 0; i < 10; ++i) g.v[i] = base * i; return g; }}\n' + close_ns)


def steps(cc, cpp):
    q = 'N::' if cpp else ''
    nq = 'N.' if cpp else ''
    a = '__attribute__((' + cc + '))'
    nsc = {'swiftcall': 'swiftcall', 'swiftasynccall': 'swiftasynccall', 'intel_ocl_bicc': 'intel_ocl_bicc'}[cc]
    return [
        (f'{q}P p = {q}getp(); mix(p(3, 4)); {q}Q qq = {q}getq(); mix((int)qq(1.5, 2.25f, 7));',
         f'var p = {nq}getp(); mix(p(3, 4)); var qq = {nq}getq(); mix((int)qq(1.5, 2.25f, 7));'),
        (f'int ({a} *own)(int, int) = {q}add; mix(own(5, 6));',
         f'fn* {nsc}(int, int) -> int own = {nq}add; mix(own(5, 6));'),
        # EN: the records come from the library: a precompiled template body cannot yet create a C++ record with
        #     `new` (T19-016). / ES: los records vienen de la biblioteca: el cuerpo de una plantilla precompilada
        #     todavia no crea un record C++ con `new` (T19-016).
        (f'{q}R rv = {q}mkr(1, 2.5f, 0.5); {q}R s = {q}rec(rv, 4); mix(s.a * 10 + (int)s.c); mix(rv.a);',
         f'{nq}R rv = {nq}mkr(1, 2.5f, 0.5); {nq}R s = {nq}rec(rv, 4); mix(s.a * 10 + (int)s.c); mix(rv.a);'),
        (f'{q}S t = {q}mks(2, 7); {q}S u = {q}small(t, 3); mix(u.c * 100 + u.s);',
         f'{nq}S t = {nq}mks(2, 7); {nq}S u = {nq}small(t, 3); mix(u.c * 100 + u.s);'),
        (f'{q}B b = {q}makeb(3); mix({q}sumb(b)); mix(b.v[7]);',
         f'{nq}B b = {nq}makeb(3); mix({nq}sumb(b)); mix(b.v[7]);'),
        (f'{q}G g = {q}makeg(2); mix({q}sumg(g, 5)); mix(g.v[9]);',
         f'{nq}G g = {nq}makeg(2); mix({nq}sumg(g, 5)); mix(g.v[9]);'),
    ]


NS_MIX = ('namespace T19V { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
MEMCPY = ('void* memcpy(void* d, const void* s, size_t n) { volatile unsigned char* p = (unsigned char*)d; '
          'const unsigned char* q = (const unsigned char*)s; while (n--) *p++ = *q++; return d; }\n')
MEMSET = ('void* memset(void* d, int c, size_t n) { volatile unsigned char* p = (unsigned char*)d; '
          'while (n--) *p++ = (unsigned char)c; return d; }\n')
C_MIX = ('__declspec(selectany) int _fltused = 0;\n#include <stddef.h>\n' + MEMSET +
         'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n')
CPP_MIX = ('extern "C" __declspec(selectany) int _fltused = 0;\n#include <stddef.h>\nextern "C" ' + MEMSET +
           'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n')
CASES = [(cc, False) for cc in ('swiftcall', 'swiftasynccall', 'intel_ocl_bicc')] + \
        [(cc, True) for cc in ('swiftcall', 'swiftasynccall')]


def c_body(items):
    return 'r = 0; ' + ' '.join(c for c, _ in items) + ' return r;'


def ns_body(items):
    return ('T19V.Mixer.r = 0; ' + ' '.join(n for _, n in items).replace('mix(', 'T19V.Mixer.mix(')
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


def reference(cpp, prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    language = 'Language="C++" Standard="c++20"' if cpp else 'Language="C" Standard="c17"'
    return ('<CppLibraryReference Header="' + prefix + 'api.h" ' + language + ' Library="' + prefix + 'api.lib"' +
            runtime + ' />')


def project(name, references, source_file, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source_file + '" />' + references + '</ItemGroup></Project>')


def build_and_run(tag, folder, opt, expected):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for cc, cpp in CASES:
    tag = ('cpp-' if cpp else 'c-') + cc
    ext = 'cpp' if cpp else 'c'
    cc_command = ['clang++.exe', '-std=c++20'] if cpp else ['clang.exe', '-std=c17']
    cc_command += ['-target', 'x86_64-pc-windows-msvc']
    items = steps(cc, cpp)
    for opt in range(4):
        case = root / f'{tag}-O{opt}'
        case.mkdir()
        (case / 'api.h').write_text(API + header(cc, cpp), encoding='utf-8')
        (case / ('api.' + ext)).write_text(source(cc, cpp), encoding='utf-8')
        run(f'{tag}-O{opt}-api', [*cc_command, '-O' + str(opt), *(['-DT19_EXPORT'] if args.dll else []), '-c',
                                  'api.' + ext, '-o', 'api.obj'], case)
        if args.dll:
            run(f'{tag}-O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                      '/implib:api.lib', 'api.obj'], case)
            (case / 'api.h').write_text('#define T19_IMPORT 1\n' + API + header(cc, cpp), encoding='utf-8')
        else:
            run(f'{tag}-O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        # A DLL does not export its memcpy, so the control brings one. / Una DLL no exporta su memcpy.
        copy = (('extern "C" ' if cpp else '') + MEMCPY) if args.dll else ''
        (case / ('control.' + ext)).write_text('#include "api.h"\n' + (CPP_MIX if cpp else C_MIX) + copy +
                                               'int main(void) { ' + c_body(items) + ' }\n', encoding='utf-8')
        run(f'{tag}-O{opt}-control', [*cc_command, '-O' + str(opt), 'control.' + ext, 'api.lib', '-fuse-ld=lld',
                                      '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'], case)
        expected = run(f'{tag}-O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
        app = case / 'app'
        app.mkdir()
        (app / 'Main.ns').write_text(NS_MIX + 'int Main() { ' + ns_body(items) + ' }\n', encoding='utf-8')
        (app / 'App.nsproj').write_text(project('T19Conv', reference(cpp, '../'), 'Main.ns'), encoding='utf-8')
        if args.dll:
            shutil.copyfile(case / 'api.dll', app / 'api.dll')
        build_and_run(f'{tag}-directo-O{opt}', app, opt, expected)
        producer = case / 'producer'
        producer.mkdir()
        (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int '
                                              'Run<Z>(Z unused) { ' + ns_body(items) + ' } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text(project('T19ConvProducer', reference(cpp, '../'), 'Producer.ns',
                                                          True), encoding='utf-8')
        run(f'{tag}-O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                                       '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt),
                                       '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility
            consumer.mkdir()
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += reference(cpp, '../')
            else:
                (case / 'api.h').rename(case / 'api.h.oculto')
            (consumer / 'Main.ns').write_text('int Main() { return T19P.Bridge.Run<int>(0); }\n', encoding='utf-8')
            (consumer / 'App.nsproj').write_text(project('T19ConvConsumer', references, 'Main.ns'), encoding='utf-8')
            try:
                if args.dll:
                    shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
                build_and_run(f'{tag}-precompilado-O{opt}-{visibility}', consumer, opt, expected)
            finally:
                if visibility == 'sin-encabezado':
                    (case / 'api.h.oculto').rename(case / 'api.h')
        print(f'{tag} O{opt}: NativeSharp y Clang devuelven {expected}, directo y desde la plantilla precompilada '
              'con y sin encabezado', flush=True)

# A genuine rejection, by Clang too: a variadic swiftcall function. / Un rechazo genuino, tambien en Clang.
negative = root / 'negativo-swift-variadica'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nint __attribute__((swiftcall)) sumv(int n, ...);\n', encoding='utf-8')
(negative / 'api.c').write_text('int dummy(void) { return 0; }\n', encoding='utf-8')
run('negativo-api', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-c', 'api.c', '-o', 'api.obj'],
    negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
(negative / 'control.c').write_text('#include "api.h"\nint use(void) { return sumv(1, 2); }\n', encoding='utf-8')
clang_output = run('negativo-clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-c', 'control.c',
                                      '-o', 'control.obj'], negative, expected=1).stdout
if 'variadic function cannot use swiftcall calling convention' not in clang_output:
    print(clang_output[-2000:], flush=True)
    raise SystemExit(f'negativo: Clang no da el rechazo esperado; evidencia: {root}')
(negative / 'Main.ns').write_text('int Main() { return sumv(1, 2); }\n', encoding='utf-8')
(negative / 'App.nsproj').write_text(project('T19ConvNeg', '<CppLibraryReference Header="api.h" Language="C" '
                                             'Standard="c17" Library="api.lib" />', 'Main.ns'), encoding='utf-8')
output = run('negativo-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                              '--no-artifact-cache', '-O0', '-o', 'app.exe'], negative, expected=1).stdout
if 'variadic function cannot use swiftcall calling convention' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo: NSC no da el diagnostico de Clang; evidencia: {root}')
print('negativo swiftcall variadica: rechazada, igual que Clang', flush=True)
print('OK: swiftcall, swiftasynccall e intel_ocl_bicc por puntero y con records, frente a Clang; evidencia:', root)
