"""T19-002: la direccion de una global C `_Atomic` (tambien `_Thread_local`), frente a Clang 23.1.0.

Como en C, `&a` de una global C `_Atomic` (int, long long, puntero, bool, double, un record, y tambien `volatile`
y `const`) es un puntero a almacenamiento atomico: se pasa a un parametro C `_Atomic T*`, se guarda en un local
`var`, lo devuelve una funcion C, se compara, se convierte a `void*` y cada acceso por el (`*p`, `p[i]`, `*p = v`,
`*p += v`, `(*p)++`) es atomico y secuencialmente consistente, como el de Clang; un record se lee y se escribe
entero. Con la biblioteca estatica, el programa usa ademas globales `_Thread_local _Atomic` (con el CRT); con
--dll, Clang rechaza una global `_Thread_local` dllimport y NSC tambien.

Compara un control C con el consumidor NativeSharp, O0-O3, en estatica y con --dll en DLL, directo y desde una
plantilla precompilada con y sin encabezado, y exige en el IR de NSC las lecturas, escrituras y
lectura-modificacion-escritura atomicas. Rechazos genuinos, tambien en Clang: un `int*` desde `&a`, `&a` a un
parametro `int*`, un `int*` a un parametro `_Atomic int*`, un miembro de un record atomico por el puntero y la
escritura por un puntero a `const _Atomic int`. Rechazo propio de NativeSharp: pasar la global atomica
por `ref`, que no tendria acceso atomico.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-atomicas-', dir='A:/temp'))
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
WITH_CRT = not args.dll

API = '''#pragma once
#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
HEADER = '''typedef struct R { int a; int b; } R;
API extern _Atomic int a_int;
API extern _Atomic long long a_ll;
API extern _Atomic(int*) a_ptr;
API extern int target;
API extern _Atomic _Bool a_b;
API extern _Atomic double a_d;
API int read_atomic(_Atomic int* p);
API void add_atomic(_Atomic int* p, int v);
API _Atomic int* get_atomic(void);
API long long read_ll(_Atomic long long* p);
API int is_set(void* p);
API int read_plain(int* p);
API extern _Atomic R a_r;
API extern volatile _Atomic int a_vi;
API extern const _Atomic int a_ci;
API int sum_r(_Atomic R* p);
'''
TLS_HEADER = '''extern _Thread_local _Atomic int t_ai;
extern _Thread_local _Atomic long long t_all;
'''
SOURCE = '''#include "api.h"
_Atomic int a_int = 4;
_Atomic long long a_ll = 9;
_Atomic(int*) a_ptr = 0;
int target = 21;
_Atomic _Bool a_b = 0;
_Atomic double a_d = 1.5;
int read_atomic(_Atomic int* p) { return __c11_atomic_load(p, 5); }
void add_atomic(_Atomic int* p, int v) { __c11_atomic_fetch_add(p, v, 5); }
_Atomic int* get_atomic(void) { return &a_int; }
long long read_ll(_Atomic long long* p) { return __c11_atomic_load(p, 5); }
int is_set(void* p) { return p != 0; }
int read_plain(int* p) { return *p; }
_Atomic R a_r;
volatile _Atomic int a_vi = 3;
const _Atomic int a_ci = 3;
int sum_r(_Atomic R* p) { R v = __c11_atomic_load(p, 5); return v.a * 10 + v.b; }
'''
TLS_SOURCE = '''_Thread_local _Atomic int t_ai = 4;
_Thread_local _Atomic long long t_all = 6;
'''
# The same statements in both languages; `mix` folds each result in. / Las mismas sentencias en ambos lenguajes.
STEPS = [
    ('add_atomic(&a_int, 5); mix(read_atomic(&a_int) + a_int);',
     'add_atomic(&a_int, 5); mix(read_atomic(&a_int) + a_int);'),
    ('_Atomic int* p = &a_int; add_atomic(p, 2); *p = *p + 1; (*p)++; *p += 3; *p -= 1; mix(*p + p[0]);',
     'var p = &a_int; add_atomic(p, 2); *p = *p + 1; (*p)++; *p += 3; *p -= 1; mix(*p + p[0]);'),
    ('_Atomic int* g = get_atomic(); *g = 7; mix(read_atomic(g) + *g); mix(g == p ? 1 : 0);',
     'var g = get_atomic(); *g = 7; mix(read_atomic(g) + *g); mix(g == p ? 1 : 0);'),
    ('_Atomic long long* q = &a_ll; *q += 5; *q -= 1; *q |= 64; mix((int)read_ll(q));',
     'var q = &a_ll; *q += 5; *q -= 1; *q |= 64; mix((int)read_ll(q));'),
    ('_Atomic(int*)* pp = &a_ptr; *pp = &target; int* t = *pp; mix(*t);',
     'var pp = &a_ptr; *pp = &target; int* t = *pp; mix(*t);'),
    ('_Atomic _Bool* b = &a_b; _Atomic double* d = &a_d; *b = 1; *d = *d + 2.0; mix((*b ? 10 : 0) + (int)(*d * 2));',
     'var b = &a_b; var d = &a_d; *b = true; *d = *d + 2.0; mix((*b ? 10 : 0) + (int)(*d * 2));'),
    ('mix(is_set(&a_int) + is_set(&a_ll));', 'mix(is_set(&a_int) + is_set(&a_ll));'),
    ('_Atomic R* rp = &a_r; R v = *rp; v.a = 5; v.b = 2; *rp = v; R w = *rp; '
     'mix(w.a + w.b + sum_r(rp) + sum_r(&a_r));',
     'var rp = &a_r; R v = *rp; v.a = 5; v.b = 2; *rp = v; R w = *rp; mix(w.a + w.b + sum_r(rp) + sum_r(&a_r));'),
    ('volatile _Atomic int* vp = &a_vi; *vp += 2; mix(*vp);', 'var vp = &a_vi; *vp += 2; mix(*vp);'),
    ('const _Atomic int* cp = &a_ci; mix(*cp);', 'var cp = &a_ci; mix(*cp);'),
]
TLS_STEPS = [
    ('_Atomic int* t1 = &t_ai; add_atomic(t1, 3); *t1 += 1; mix(*t1 + read_atomic(&t_ai));',
     'var t1 = &t_ai; add_atomic(t1, 3); *t1 += 1; mix(*t1 + read_atomic(&t_ai));'),
    ('_Atomic long long* t2 = &t_all; *t2 = *t2 * 2; mix((int)read_ll(t2) + (int)t_all);',
     'var t2 = &t_all; *t2 = *t2 * 2; mix((int)read_ll(t2) + (int)t_all);'),
]
NS_MIX = ('namespace T19A { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')
C_MIX = 'static int r = 0;\nstatic void mix(int v) { r = (r * 31 + v) % 1000003; }\n'
MARKER = '__declspec(selectany) int _fltused = 0;\n'


def steps():
    return STEPS + (TLS_STEPS if WITH_CRT else [])


def c_body(items):
    return 'r = 0; ' + ' '.join(c for c, _ in items) + ' return r;'


def ns_body(items):
    return ('T19A.Mixer.r = 0; ' + ' '.join(n for _, n in items).replace('mix(', 'T19A.Mixer.mix(')
            + ' return T19A.Mixer.r;')


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
    """Atomic loads, stores and read-modify-writes in NSC's IR. / Accesos atomicos en el IR de NSC."""
    return (len(re.findall(r'load atomic [^\n]*seq_cst', ir)), len(re.findall(r'store atomic [^\n]*seq_cst', ir)),
            len(re.findall(r'(atomicrmw|cmpxchg) [^\n]*seq_cst', ir)))


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
    (app / 'App.nsproj').write_text(project('T19Atomicas', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj', '--cpp-clang-root=' + args.clang_root],
             app).stdout
    loads, stores, updates = atomic_accesses(ir)
    if not loads or not stores or not updates:
        raise SystemExit(f'O{opt}: faltan accesos atomicos en el IR de NSC ({loads}, {stores}, {updates}); '
                         f'evidencia: {root}')
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected)
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<Q>('
                                          'Q unused) { ' + ns_body(steps()) + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T19AtomicasProducer', reference('../'), 'Producer.ns', True),
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
        (consumer / 'App.nsproj').write_text(project('T19AtomicasConsumer', references, 'Main.ns'), encoding='utf-8')
        try:
            if args.dll:
                shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
            build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, expected)
        finally:
            if visibility == 'sin-encabezado':
                (case / 'api.h.oculto').rename(case / 'api.h')
    print(f'O{opt}: NativeSharp y C devuelven {expected}, directo y desde la plantilla precompilada con y sin '
          f'encabezado; IR de NSC con {loads} lecturas, {stores} escrituras y {updates} actualizaciones atomicas',
          flush=True)

# Uses that Clang rejects are rejected by NativeSharp too. / Los usos que Clang rechaza tambien se rechazan.
case = root / 'O0'
for label, c_text, ns_text, clang_message, message in (
        ('int-desde-atomica', 'int* x = &a_int; return *x;', 'int* x = &a_int; return *x;',
         'incompatible pointer types', 'would discard pointer pointee atomic qualification'),
        ('atomica-a-parametro-plano', 'return read_plain(&a_int);', 'return read_plain(&a_int);',
         'incompatible pointer types', "no viable overload of namespace function 'read_plain'"),
        ('plano-a-parametro-atomico', 'int x = 3; return read_atomic(&x);', 'int x = 3; return read_atomic(&x);',
         'incompatible pointer types', "no viable overload of namespace function 'read_atomic'"),
        ('miembro-de-record-atomico', '_Atomic R* rp = &a_r; return rp->a;', 'var rp = &a_r; return rp->a;',
         'atomic structure', "cannot access member 'a' of C atomic record storage"),
        ('escritura-por-const', 'const _Atomic int* cp = &a_ci; *cp = 4; return 0;',
         'var cp = &a_ci; *cp = 4; return 0;', 'not assignable', 'not assignable')):
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
    (folder / 'App.nsproj').write_text(project('T19AtomicasRechazo', reference(), 'Main.ns'), encoding='utf-8')
    output = nsc_exe(folder.name, folder, 0, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{folder.name}: falta el diagnostico; evidencia: {root}')
    print(f'{folder.name}: se rechaza, igual que Clang', flush=True)
# A NativeSharp `ref` would reach the storage without its atomic access. / Un `ref` llegaria al almacenamiento
# sin su acceso atomico.
folder = root / 'rechazo-ref'
folder.mkdir()
for item in ('api.h', 'api.lib', *(('api.dll',) if args.dll else ())):
    (folder / item).write_bytes((case / item).read_bytes())
(folder / 'Main.ns').write_text('int F(ref int x) { x = 1; return x; }\nint Main() { return F(ref a_int); }\n',
                                encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T19AtomicasRef', reference(), 'Main.ns'), encoding='utf-8')
output = nsc_exe(folder.name, folder, 0, expected=1).stdout
if 'C atomic storage cannot be passed by ref' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'rechazo-ref: falta el diagnostico; evidencia: {root}')
print('rechazo-ref: pasar una global atomica por ref se rechaza', flush=True)
if args.dll:
    # Clang rejects a dllimport thread-local, so the TLS part of the scope does not apply to a DLL.
    # Clang rechaza una variable local al hilo dllimport: la parte TLS del alcance no aplica a una DLL.
    folder = root / 'tls-dllimport'
    folder.mkdir()
    (folder / 'api.h').write_text('#pragma once\n__declspec(dllimport) extern _Thread_local _Atomic int t_ai;\n',
                                  encoding='utf-8')
    (folder / 'control.c').write_text('#include "api.h"\nint main(void) { return t_ai; }\n', encoding='utf-8')
    clang = run('tls-dllimport-clang', [*CC, '-c', 'control.c', '-o', 'control.obj'], folder, expected=1).stdout
    if 'thread' not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(f'tls-dllimport: Clang no lo rechaza; evidencia: {root}')
    (folder / 'Main.ns').write_text('int Main() { var t = &t_ai; return *t; }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T19AtomicasTls', '<CppLibraryReference Header="api.h" Language="C" '
                                       'Standard="c17" Library="../O0/api.lib" />', 'Main.ns'), encoding='utf-8')
    output = nsc_exe('tls-dllimport', folder, 0, expected=1).stdout
    if 'error' not in output:
        raise SystemExit(f'tls-dllimport: NSC no lo rechaza; evidencia: {root}')
    print('tls-dllimport: Clang y NSC rechazan una global _Thread_local dllimport', flush=True)
print('OK: direccion de globales C _Atomic, tambien _Thread_local, frente a Clang; evidencia:', root)
