"""T19-002: `null` en punteros a funcion de una biblioteca C, en asignacion, argumento y comparacion.

La biblioteca C declara `typedef int (*t19_cb)(int)` y un struct con un campo de ese tipo. NativeSharp asigna
`null` al campo, lo pasa como argumento y compara el campo y un puntero devuelto con `null`, como un control C
compilado por Clang 23.1.0 con `NULL`, O0-O3, en estatica y DLL. Rechazo genuino aparte: `null` a un parametro
`int` se rechaza, igual que Clang (`incompatible pointer to integer conversion`).
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-nulos-', dir='A:/temp'))
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
HEADER = '''typedef int (*t19_cb)(int);
typedef struct t19_ops { t19_cb f; int v; } t19_ops;
API int t19_call(t19_cb f, int v);
API int t19_ops_call(const t19_ops* o);
API int t19_is_null(t19_cb f);
API t19_cb t19_none(void);
API int t19_twice(int x);
'''
SOURCE = '''#include "api.h"
int t19_call(t19_cb f, int v) { return f ? f(v) : v + 40; }
int t19_ops_call(const t19_ops* o) { return o->f ? o->f(o->v) : o->v * 3; }
int t19_is_null(t19_cb f) { return f == 0; }
t19_cb t19_none(void) { return 0; }
int t19_twice(int x) { return x * 2; }
'''
CONTROL = '''#include "api.h"
#include <stddef.h>
int main(void) {
    t19_ops o;
    o.f = NULL;
    o.v = 5;
    int r = t19_call(NULL, 2);
    r = r + t19_ops_call(&o);
    r = r + t19_is_null(NULL) * 100;
    if (o.f == NULL) r = r + 1000;
    t19_cb g = t19_none();
    if (g == NULL) r = r + 10000;
    if (NULL != g) r = r + 1;
    return r;
}
'''
NS_PROGRAM = '''int Main()
{
    t19_ops o;
    o.f = null;
    o.v = 5;
    int r = t19_call(null, 2);
    r = r + t19_ops_call(&o);
    r = r + t19_is_null(null) * 100;
    if (o.f == null) r = r + 1000;
    t19_cb g = t19_none();
    if (g == null) r = r + 10000;
    if (null != g) r = r + 1;
    return r;
}
'''
EXPECTED = 42 + 15 + 100 + 1000 + 10000


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


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
library = root / 'lib'
library.mkdir()
(library / 'api.h').write_text(API + HEADER, encoding='utf-8')
(library / 'api.c').write_text(SOURCE, encoding='utf-8')
run('api', [*CC, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.c', '-o', 'api.obj'], library)
if args.dll:
    run('api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll', '/implib:api.lib',
                    'api.obj'], library)
    header = '#define T19_IMPORT 1\n' + API + HEADER
else:
    run('api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
    header = API + HEADER
runtime = ' Runtime="api.dll"' if args.dll else ''
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(header, encoding='utf-8')
    for name in ('api.lib', *(('api.dll',) if args.dll else ())):
        (case / name).write_bytes((library / name).read_bytes())
    (case / 'control.c').write_text(CONTROL, encoding='utf-8')
    run(f'O{opt}-control', [*CC, '-O' + str(opt), 'control.c', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'O{opt}: el control C devolvio {control}; evidencia: {root}')
    (case / 'Main.ns').write_text(NS_PROGRAM, encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19Nulos</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C" Standard="c17" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')
    run(f'O{opt}-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                        '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], case)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, C {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y C devuelven {result} (asignacion, argumento y comparacion con null)', flush=True)
# A genuine rejection: null is not an int. / Un rechazo genuino: null no es un int.
case = root / 'O0'
(case / 'rechazo.c').write_text('#include "api.h"\n#include <stddef.h>\nint main(void) { return t19_twice(NULL); }\n',
                                encoding='utf-8')
clang = run('rechazo-control', [*CC, '-c', 'rechazo.c', '-o', 'rechazo.obj'], case, expected=1).stdout
if 'incompatible pointer to integer conversion' not in clang:
    print(clang[-2000:], flush=True)
    raise SystemExit(f'rechazo: Clang no rechaza NULL como int; evidencia: {root}')
(case / 'Main.ns').write_text('int Main() { return t19_twice(null); }\n', encoding='utf-8')
output = run('rechazo-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                             '--no-artifact-cache', '-o', 'rechazo.exe'], case, expected=1).stdout
if "argument 1 of type 'null' does not convert to 'int'" not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'rechazo: falta el diagnostico; evidencia: {root}')
print('rechazo: null como int se rechaza, igual que Clang', flush=True)
print('OK: null en punteros a funcion C, frente a Clang; evidencia:', root)
