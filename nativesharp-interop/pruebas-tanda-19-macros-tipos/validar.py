"""T19-042: tipos como argumento de macros de funcion C llamadas desde NativeSharp, frente a Clang 23.1.0.

Un tipo predefinido de NativeSharp, con o sin `*`, escrito como argumento (`SIZE(int)`, `CAST(v, byte)`,
`ZERO(long)`, `BYTES(float, v)`) llega a la macro con su escritura C (`long` es `long long`, `byte` es
`unsigned char`), como si la llamada C lo hubiera escrito; la macro lo usa con `sizeof`, `_Alignof` o un cast. Una
macro sin tipos (`SQ`) se expande como antes (T19-003).

Compara un control C con el consumidor NativeSharp, O0-O3, en estatica y con --dll en DLL, directo y desde una
plantilla precompilada con y sin encabezado. Rechazo genuino, tambien en Clang: un tipo como argumento de una funcion
C (`hstr(int)`).
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-macros-tipos-', dir='A:/temp'))
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
HEADER = '''
#define SIZE(t) ((int)sizeof(t))
#define CAST(x, t) ((t)(x))
#define ZERO(t) ((t)0)
#define ALIGN(t) ((int)_Alignof(t))
#define BYTES(t, n) ((int)(sizeof(t) * (n)))
#define SQ(x) ((x) * (x))
API int hstr(const char* s);
'''
TLS_HEADER = ''
SOURCE = '''#include "api.h"
int hstr(const char* s) { int h = 7; while (*s) h = (h * 31 + (unsigned char)*s++) % 100003; return h; }
'''
TLS_SOURCE = ''
# C first, then NativeSharp: the same types, each in its own spelling. / C primero, luego NativeSharp.
STEPS = [
    ('mix(SIZE(int) + SIZE(long long) * 3 + SIZE(unsigned char*) * 5 + SIZE(double) * 7);',
     'mix(SIZE(int) + SIZE(long) * 3 + SIZE(byte*) * 5 + SIZE(double) * 7);'),
    ('int v = 300; mix(CAST(v, unsigned char) + CAST(-1, unsigned short) + CAST(v, signed char));',
     'int v = 300; mix(CAST(v, byte) + CAST(-1, ushort) + CAST(v, sbyte));'),
    ('mix(ALIGN(double) + ALIGN(short) * 10 + BYTES(float, v) + BYTES(unsigned long long*, 3));',
     'mix(ALIGN(double) + ALIGN(short) * 10 + BYTES(float, v) + BYTES(ulong*, 3));'),
    ('mix((int)ZERO(long long) + SIZE(int**) + (int)CAST(2.75, float) * 4);',
     'mix((int)ZERO(long) + SIZE(int**) + (int)CAST(2.75, float) * 4);'),
    ('mix(SQ(v + 1) + SQ(2));', 'mix(SQ(v + 1) + SQ(2));'),
]
TLS_STEPS = []
NS_MIX = ('namespace T19F { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = 'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n'
MARKER = '__declspec(selectany) int _fltused = 0;\n'


def steps():
    return STEPS + (TLS_STEPS if WITH_CRT else [])


def c_body(items):
    return 'r = 0; ' + ' '.join(c for c, _ in items) + ' return r;'


def ns_body(items):
    return ('T19F.Mixer.r = 0; ' + ' '.join(n for _, n in items).replace('mix(', 'T19F.Mixer.mix(')
            + ' return T19F.Mixer.r;')


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
    """Calls from NSC's IR to the macro wrappers. / Llamadas a los envoltorios de macro."""
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
    (app / 'App.nsproj').write_text(project('T19MacrosTipos', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj', '--cpp-clang-root=' + args.clang_root],
             app).stdout
    loads, stores, updates, volatiles = atomic_accesses(ir)
    if not loads:
        raise SystemExit(f'O{opt}: faltan llamadas a los envoltorios de macro en el IR de NSC; evidencia: {root}')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected)
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<Q>('
                                          'Q unused) { ' + ns_body(steps()) + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T19MacrosTiposProducer', reference('../'), 'Producer.ns',
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
        (consumer / 'App.nsproj').write_text(project('T19MacrosTiposConsumer', references, 'Main.ns'),
                                             encoding='utf-8')
        try:
            if args.dll:
                shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
            build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, expected)
        finally:
            if visibility == 'sin-encabezado':
                (case / 'api.h.oculto').rename(case / 'api.h')
    print(f'O{opt}: NativeSharp y C devuelven {expected}, directo y desde la plantilla precompilada con y sin '
          f'encabezado; IR de NSC con {loads} llamadas a envoltorios de macro',
          flush=True)

# Uses that Clang rejects are rejected by NativeSharp too. / Los usos que Clang rechaza tambien se rechazan.
case = root / 'O0'
for label, c_text, ns_text, clang_message, message in (
        ('tipo-a-funcion', 'return hstr(int);', 'return hstr(int);', 'expected expression', 'a type is not a value'),):
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
    (folder / 'App.nsproj').write_text(project('T19MacrosTiposRechazo', reference(), 'Main.ns'), encoding='utf-8')
    output = nsc_exe(folder.name, folder, 0, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{folder.name}: falta el diagnostico; evidencia: {root}')
    print(f'{folder.name}: se rechaza, igual que Clang', flush=True)
print('OK: tipos como argumento de macros C frente a Clang; evidencia:', root)
