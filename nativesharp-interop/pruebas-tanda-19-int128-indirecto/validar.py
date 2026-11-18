"""T19-044: `__int128` por punteros a funcion C, callbacks y llamadas variadicas, y bit-fields `volatile` de
mas de 64 bits, frente a Clang 23.1.0.

Con la convencion C de Win64, como Clang, un entero de 128 bits se pasa por la direccion de una copia alineada a 16
(tambien como argumento variadico) y se devuelve en XMM0:
- una llamada desde NativeSharp por un puntero a funcion C (`get_twice()`), y por uno a una funcion NativeSharp;
- funciones NativeSharp pasadas a C como callbacks (`apply_u`, `apply_i`), que C llama;
- `vsum(3, ...)` con argumentos variadicos `unsigned __int128`;
- bit-fields `volatile unsigned __int128 big : 100` y `low : 20`, accedidos por su unidad de 16 bytes.

Compara un control C con el consumidor NativeSharp, O0-O3, en estatica y con --dll en DLL, directo y desde una
plantilla precompilada con y sin encabezado. Rechazo genuino, tambien en Clang: un puntero a funcion de otra firma.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-int128-', dir='A:/temp'))
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
HEADER = '''typedef unsigned __int128 U128;
typedef __int128 I128;
typedef U128 (*UOp)(U128);
typedef I128 (*IOp)(I128, int);
typedef struct VW { volatile unsigned __int128 big : 100; volatile unsigned __int128 low : 20; } VW;
API U128 c_twice(U128 v);
API UOp get_twice(void);
API U128 apply_u(UOp f, U128 v);
API I128 apply_i(IOp f, I128 v, int k);
API U128 vsum(int n, ...);
API int fold(U128 v);
'''
TLS_HEADER = ''
SOURCE = '''#include "api.h"
#include <stdarg.h>
U128 c_twice(U128 v) { return v * 2; }
UOp get_twice(void) { return c_twice; }
U128 apply_u(UOp f, U128 v) { return f(v) + 1; }
I128 apply_i(IOp f, I128 v, int k) { return f(v, k) * 3; }
U128 vsum(int n, ...) { va_list l; va_start(l, n); U128 t = 0; for (int i = 0; i < n; ++i)
    t += va_arg(l, U128) * (unsigned)(i + 1); va_end(l); return t; }
int fold(U128 v) { return (int)((v ^ (v >> 64) ^ (v >> 100)) % 1000003); }
'''
TLS_SOURCE = ''
# C first, then NativeSharp. / C primero, luego NativeSharp.
STEPS = [
    ('UOp f = get_twice(); U128 a = (U128)7 << 90; mix(fold(f(a))); mix(fold(apply_u(f, a)));',
     'UOp f = get_twice(); u128 a = (u128)7 << 90; mix(fold(f(a))); mix(fold(apply_u(f, a)));'),
    ('UOp g = cb_twice; mix(fold(apply_u(g, (U128)5 << 80))); mix(fold(g((U128)9 << 70)));'
     ' IOp h = cb_sub; mix(fold((U128)apply_i(h, -((I128)3 << 70), 9)));',
     'UOp g = T19G.Cb.twice; mix(fold(apply_u(g, (u128)5 << 80))); mix(fold(g((u128)9 << 70)));'
     ' IOp h = T19G.Cb.sub; mix(fold((u128)apply_i(h, -((i128)3 << 70), 9)));'),
    ('mix(fold(vsum(3, (U128)1 << 70, (U128)2, (U128)3 << 100)));',
     'mix(fold(vsum(3, (u128)1 << 70, (u128)2, (u128)3 << 100)));'),
    ('VW w = {0}; w.big = ((U128)5 << 94) | 3; w.low = 77; w.big += 1; mix(fold(w.big)); mix((int)w.low);',
     'VW w = new VW(); w.big = ((u128)5 << 94) | 3; w.low = 77; w.big += 1; mix(fold(w.big)); mix((int)w.low);'),
]
TLS_STEPS = []
NS_MIX = ('namespace T19G { public class Cb { public static u128 twice(u128 v) { return v * 2 + 5; } public static i128 sub(i128 v, int k) { return v - k; } } public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = 'static int r = 0;\nstatic U128 cb_twice(U128 v) { return v * 2 + 5; }\nstatic I128 cb_sub(I128 v, int k) { return v - k; }\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n'
MARKER = '__declspec(selectany) int _fltused = 0;\n'


def steps():
    return STEPS + (TLS_STEPS if WITH_CRT else [])


def c_body(items):
    return 'r = 0; ' + ' '.join(c for c, _ in items) + ' return r;'


def ns_body(items):
    return ('T19G.Mixer.r = 0; ' + ' '.join(n for _, n in items).replace('mix(', 'T19G.Mixer.mix(')
            + ' return T19G.Mixer.r;')


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
    """Volatile 128-bit accesses in NSC's IR. / Accesos volatile de 128 bits en el IR de NSC."""
    return (len(re.findall(r'(?:load|store) volatile i128', ir)), 1, 1, 1)


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
    (app / 'App.nsproj').write_text(project('T19Int128', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj', '--cpp-clang-root=' + args.clang_root],
             app).stdout
    loads, stores, updates, volatiles = atomic_accesses(ir)
    if not loads:
        raise SystemExit(f'O{opt}: faltan accesos volatile de 128 bits en el IR de NSC; evidencia: {root}')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected)
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<Q>('
                                          'Q unused) { ' + ns_body(steps()) + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T19Int128Producer', reference('../'), 'Producer.ns',
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
        (consumer / 'App.nsproj').write_text(project('T19Int128Consumer', references, 'Main.ns'),
                                             encoding='utf-8')
        try:
            if args.dll:
                shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
            build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, expected)
        finally:
            if visibility == 'sin-encabezado':
                (case / 'api.h.oculto').rename(case / 'api.h')
    print(f'O{opt}: NativeSharp y C devuelven {expected}, directo y desde la plantilla precompilada con y sin '
          f'encabezado; IR de NSC con {loads} accesos volatile de 128 bits',
          flush=True)

# Uses that Clang rejects are rejected by NativeSharp too. / Los usos que Clang rechaza tambien se rechazan.
case = root / 'O0'
for label, c_text, ns_text, clang_message, message in (
        ('firma-distinta', 'IOp bad = c_twice; return 0;', 'IOp bad = c_twice; return 0;',
         'incompatible function pointer types', 'exactly matches delegate signature'),):
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
    (folder / 'App.nsproj').write_text(project('T19Int128Rechazo', reference(), 'Main.ns'), encoding='utf-8')
    output = nsc_exe(folder.name, folder, 0, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{folder.name}: falta el diagnostico; evidencia: {root}')
    print(f'{folder.name}: se rechaza, igual que Clang', flush=True)
print('OK: __int128 por punteros, callbacks y variadicas, y bit-fields volatile de 128 bits, frente a Clang; evidencia:', root)
