"""T19-041: macros de objeto C cuya expansion no es una constante, usadas desde NativeSharp, frente a Clang 23.1.0.

Como en C, cada acceso evalua la expansion una vez:
- lvalues, que se leen, asignan, incrementan, cuyo campo se asigna y cuya direccion se toma: `my_errno`
  (`(*api_errno())`, como `errno` del CRT) y `CUR` (`(*cur_point())`, un record);
- valores de solo lectura: `NEXT_ID` (una llamada), `ORIGIN` (un record devuelto), `NAME_PTR` (una cadena elegida en
  ejecucion) y `COUNTER_ADDR` (`&counter`: en DLL la direccion de una global dllimport, que no es constante; en
  estatica sigue siendo el valor constante de T19-003).

Compara un control C con el consumidor NativeSharp, O0-O3, en estatica y con --dll en DLL, directo y desde una
plantilla precompilada con y sin encabezado. Rechazos genuinos, tambien en Clang: asignar `NEXT_ID` y tomar su
direccion.
"""
import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-macros-objeto-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CC = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17']
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
# EN: C thread-locals in the executable need the static CRT's TLS support, so the static variant links with it.
# ES: Las variables locales al hilo de C en el ejecutable necesitan el TLS del CRT estatico; la variante estatica
#     lo enlaza.
WITH_CRT = False

API = '''#pragma once
#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
HEADER = '''API int* api_errno(void);
#define my_errno (*api_errno())
extern API int counter;
#define COUNTER_ADDR (&counter)
API int next_id(void);
#define NEXT_ID (next_id())
typedef struct Pt { int x; int y; } Pt;
API Pt* cur_point(void);
API Pt make_point(int k);
#define CUR (*cur_point())
#define ORIGIN (make_point(4))
#define NAME_PTR (counter > 100 ? "big" : "small")
API int hstr(const char* s);
'''
TLS_HEADER = ''
SOURCE = '''#include "api.h"
static int e;
int* api_errno(void) { return &e; }
int counter = 3;
int next_id(void) { static int n = 10; return n++; }
static Pt p = {1, 2};
Pt* cur_point(void) { return &p; }
Pt make_point(int k) { Pt r = {k, k * 2}; return r; }
int hstr(const char* s) { int h = 7; while (*s) h = (h * 31 + (unsigned char)*s++) % 100003; return h; }
'''
TLS_SOURCE = ''
# The same statements in both languages; `mix` folds each result in. / Las mismas sentencias en ambos lenguajes.
STEPS = [
    ('my_errno = 7; my_errno += 5; mix(my_errno); my_errno++; mix(my_errno);',
     'my_errno = 7; my_errno += 5; mix(my_errno); my_errno++; mix(my_errno);'),
    ('int* q = &my_errno; *q = 40; mix(my_errno);', 'int* q = &my_errno; *q = 40; mix(my_errno);'),
    ('mix(NEXT_ID); mix(NEXT_ID * 2); mix(NEXT_ID);', 'mix(NEXT_ID); mix(NEXT_ID * 2); mix(NEXT_ID);'),
    ('CUR.x = 9; CUR.y += 3; mix(CUR.x * 100 + CUR.y);', 'CUR.x = 9; CUR.y += 3; mix(CUR.x * 100 + CUR.y);'),
    ('mix(ORIGIN.x + ORIGIN.y * 10);', 'mix(ORIGIN.x + ORIGIN.y * 10);'),
    ('*COUNTER_ADDR = *COUNTER_ADDR + 2; mix(counter); mix(hstr(NAME_PTR));',
     '*COUNTER_ADDR = *COUNTER_ADDR + 2; mix(counter); mix(hstr(NAME_PTR));'),
]
TLS_STEPS = []
NS_MIX = ('namespace T19E { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = 'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n'
MARKER = '__declspec(selectany) int _fltused = 0;\n'


def steps():
    return STEPS + (TLS_STEPS if WITH_CRT else [])


def c_body(items):
    return 'r = 0; ' + ' '.join(c for c, _ in items) + ' return r;'


def ns_body(items):
    return ('T19E.Mixer.r = 0; ' + ' '.join(n for _, n in items).replace('mix(', 'T19E.Mixer.mix(')
            + ' return T19E.Mixer.r;')


def run(label, command, cwd, expected=0, extra_env=None):
    result = subprocess.run([str(c) for c in command], cwd=cwd, env={**env, **(extra_env or {})},
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
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


def crt_args():
    if not WITH_CRT:
        return [], {}
    extra = ['--crt-startup']
    for item in CRT:
        extra += ['--link-lib', str(item)]
    return extra, {'NSC_LINKER': 'lld-link.exe'}


def nsc_exe(tag, folder, opt, expected=0):
    extra, extra_env = crt_args()
    return run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                '--no-artifact-cache', '-O' + str(opt), *extra, '-o', 'app.exe'], folder, expected,
               extra_env)


def build_and_run(tag, folder, opt, expected):
    nsc_exe(tag, folder, opt)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


def control(case, opt, body):
    (case / 'control.c').write_text('#include "api.h"\n' + ('' if WITH_CRT else MARKER) + C_MIX +
                                    'int main(void) { ' + body + ' }\n', encoding='utf-8')
    if WITH_CRT:
        run(f'{case.name}-control', [*CC, '-O' + str(opt), '-c', 'control.c', '-o', 'control.obj'], case)
        run(f'{case.name}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe',
                                          'control.obj', 'api.lib', *CRT], case)
    else:
        run(f'{case.name}-control', [*CC, '-O' + str(opt), 'control.c', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                                     '-Wl,/entry:main', '-o', 'control.exe'], case)
    return run(f'{case.name}-control-run', [str(case / 'control.exe')], case, expected=None).returncode


def atomic_accesses(ir):
    """Calls from NSC's IR to the C accessors of the macros. / Llamadas a las accesoras C de las macros."""
    return (len(re.findall(r'call [^\n]*@__nsc_c_inline_', ir)), 1, 1, 1)


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
header = HEADER + (TLS_HEADER if WITH_CRT else '')
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(API + header, encoding='utf-8')
    (case / 'api.c').write_text(SOURCE + (TLS_SOURCE if WITH_CRT else MARKER), encoding='utf-8')
    run(f'O{opt}-api', [*CC, '-O' + str(opt), *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.c', '-o',
                        'api.obj'], case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                            '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T19_IMPORT 1\n' + API + header, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    expected = control(case, opt, c_body(steps()))
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text(NS_MIX + 'int Main() { ' + ns_body(steps()) + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T19MacrosObjeto', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj', '--cpp-clang-root=' + args.clang_root],
             app).stdout
    loads, stores, updates, volatiles = atomic_accesses(ir)
    if not loads:
        raise SystemExit(f'O{opt}: faltan llamadas a las accesoras de macro en el IR de NSC; evidencia: {root}')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected)
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<Q>('
                                          'Q unused) { ' + ns_body(steps()) + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T19MacrosObjetoProducer', reference('../'), 'Producer.ns',
                                                      True),
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
        (consumer / 'App.nsproj').write_text(project('T19MacrosObjetoConsumer', references, 'Main.ns'),
                                             encoding='utf-8')
        try:
            if args.dll:
                shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
            build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, expected)
        finally:
            if visibility == 'sin-encabezado':
                (case / 'api.h.oculto').rename(case / 'api.h')
    print(f'O{opt}: NativeSharp y C devuelven {expected}, directo y desde la plantilla precompilada con y sin '
          f'encabezado; IR de NSC con {loads} llamadas a accesoras de macro',
          flush=True)

# Uses that Clang rejects are rejected by NativeSharp too. / Los usos que Clang rechaza tambien se rechazan.
case = root / 'O0'
for label, c_text, ns_text, clang_message, message in (
        ('asignar-valor', 'NEXT_ID = 1; return 0;', 'NEXT_ID = 1; return 0;', 'expression is not assignable',
         'left side of assignment is not assignable'),
        ('direccion-valor', 'int* q = &NEXT_ID; return *q;', 'int* q = &NEXT_ID; return *q;',
         'cannot take the address of an rvalue', "cannot take the address of C macro 'NEXT_ID'")):
    folder = root / ('rechazo-' + label)
    folder.mkdir()
    for item in ('api.h', 'api.lib', *(('api.dll',) if args.dll else ())):
        (folder / item).write_bytes((case / item).read_bytes())
    (folder / 'control.c').write_text('#include "api.h"\nint main(void) { ' + c_text + ' }\n', encoding='utf-8')
    clang = run(folder.name + '-clang', [*CC, '-c', 'control.c', '-o', 'control.obj'], folder, expected=1).stdout
    if clang_message not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(f'{folder.name}: Clang no lo rechaza; evidencia: {root}')
    (folder / 'Main.ns').write_text('int Main() { ' + ns_text + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T19MacrosObjetoRechazo', reference(), 'Main.ns'), encoding='utf-8')
    output = nsc_exe(folder.name, folder, 0, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{folder.name}: falta el diagnostico; evidencia: {root}')
    print(f'{folder.name}: se rechaza, igual que Clang', flush=True)
print('OK: macros de objeto C no constantes frente a Clang; evidencia:', root)
