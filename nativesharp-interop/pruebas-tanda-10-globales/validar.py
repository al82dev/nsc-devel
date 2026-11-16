"""T10-004, T10-006, T10-014 y T10-015: globales y campos estaticos C++ de tipo record, enum y arreglo.

Una global C++ de namespace o un campo estatico de clase guarda un record C++ trivial, un enum o un
arreglo fijo de escalares o records; una global thread_local usa el TLS de Windows como en C. NativeSharp
los lee y escribe sobre el mismo almacenamiento que la biblioteca. Compara un control C++ compilado
por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL (sin
thread_local, que no se importa de una DLL), y una plantilla precompilada que llama a la biblioteca.
Los controles negativos comprueban que un campo estatico const no se escribe y que un record con
destructor no trivial no se importa por valor. Una global thread_local sin constinit (inicializacion dinamica)
se compara con Clang: los dos acceden con la guarda TLS bajo demanda del CRT (T19-036).
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-globales-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

TLS = not args.dll
HEADER = '''#pragma once
#if defined(T10_EXPORT)
#define API __declspec(dllexport)
#elif defined(T10_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T10 {
struct P { int x; int y; };
enum class Level : short { Low = 2, High = 9 };
API extern P origin;
API extern P path[2];
API extern int grid[2][3];
API extern Level level;
struct S {
    API static int table[3];
    API static P corner;
    API static Level mode;
    API static const P fixed;
};
API int bump();
''' + ('''extern constinit thread_local int tls;
extern constinit thread_local P tlsPoint;
int tlsRead();
''' if TLS else '') + '''}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T10 {
P origin = {4, 5};
P path[2] = {{1, 2}, {3, 4}};
int grid[2][3] = {{1, 2, 3}, {4, 5, 6}};
Level level = Level::High;
int S::table[3] = {10, 20, 30};
P S::corner = {6, 8};
Level S::mode = Level::Low;
const P S::fixed = {11, 12};
int bump() { origin.x += 1; S::table[0] += 1; return origin.x + S::table[0]; }
}
'''
TLS_SOURCE = '''#include "api.h"
namespace T10 {
constinit thread_local int tls = 7;
constinit thread_local P tlsPoint = {1, 2};
int tlsRead() { return tls + tlsPoint.y; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    using namespace T10;
    int r = origin.y + path[1].x + grid[1][2] + (int)level + S::table[2] + S::corner.y + (int)S::mode + S::fixed.x;
    origin.y = 10; S::corner.x = 1; S::table[1] = 2; grid[0][0] = 3; path[0].y = 4; S::mode = Level::High;
    r += origin.y + S::corner.x + S::table[1] + grid[0][0] + path[0].y + (int)S::mode;
    r += bump();
''' + ('''    tls = tls + 1; tlsPoint.y = tlsPoint.y + 3;
    r += tlsRead() + tlsPoint.x;
''' if TLS else '') + '''    return r;
}
'''
NS_BODY = ('int r = T10.origin.y + T10.path[1].x + T10.grid[1][2] + (int)T10.level + T10.S.table[2] + '
           'T10.S.corner.y + (int)T10.S.mode + T10.S.fixed.x; '
           'T10.origin.y = 10; T10.S.corner.x = 1; T10.S.table[1] = 2; T10.grid[0][0] = 3; T10.path[0].y = 4; '
           'T10.S.mode = T10.Level.High; '
           'r += T10.origin.y + T10.S.corner.x + T10.S.table[1] + T10.grid[0][0] + T10.path[0].y + (int)T10.S.mode; '
           'r += T10.bump(); ' + ('T10.tls = T10.tls + 1; T10.tlsPoint.y = T10.tlsPoint.y + 3; '
                                   'r += T10.tlsRead() + T10.tlsPoint.x; ' if TLS else '') + 'return r;')
EXPECTED = 74 + 29 + 16 + (8 + 5 + 1 if TLS else 0)
CRT = Path('A:/vc2026-runtime/lib/x64/release')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib')
CRT_LIBRARIES = [CRT / 'libcmt.lib', CRT / 'libvcruntime.lib', SDK / 'ucrt/x64/libucrt.lib', SDK / 'um/x64/kernel32.lib']
TEMPLATE_BODY = 'return T10.bump();'
TEMPLATE_EXPECTED = 16
DECLARATIONS = 1 + (1 if TLS else 0)
def run(label, command, cwd, expected=0, environment=None):
    result = subprocess.run(command, cwd=cwd, env=environment or env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
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
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


def project(name, references, source, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def declarations(ir):
    """Physical parameter and return types of every T10 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T10@@Y[^"(]*)"?\((.*)\)', line)
        if not match:
            continue
        head, symbol, parameters = match.groups()
        result = [w for w in head.split() if w not in ('dso_local', 'dllimport', 'noundef', 'nonnull', 'zeroext',
                                                       'signext') and not w.startswith(('dereferenceable', 'align'))]
        shapes = []
        for parameter in parameters.split(', ') if parameters.strip() else []:
            words = parameter.split()
            shapes.append(words[0])
        found[symbol] = (result[-1] if result else '', tuple(shapes))
    return found


def build_library(case, opt):
    case.mkdir(parents=True)
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT10_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T10_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        (case / 'tls.cpp').write_text(TLS_SOURCE, encoding='utf-8')
        run(case.name + '-tls', ['clang++.exe', *flags, '-c', 'tls.cpp', '-o', 'tls.obj'], case)
        run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj', 'tls.obj'], case)
    control_ir = run(case.name + '-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                 '-O0', '-S', '-emit-llvm', 'Control.cpp', '-o', '-'], case).stdout
    run(case.name + '-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    # The TLS directory (_tls_used, _tls_index) comes from the CRT, as for C thread-locals.
    tls_link = ['/include:_tls_used', *[str(path) for path in CRT_LIBRARIES]] if TLS else []
    run(case.name + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                      '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib', *tls_link], case)
    if args.dll:
        pass
    control = run(case.name + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'{case.name}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    clang = declarations(control_ir)
    if len(clang) != DECLARATIONS:
        raise SystemExit(f'{case.name}: se esperaban {DECLARATIONS} declaraciones de Clang, hay {len(clang)}; evidencia: {root}')
    return clang


def compare(tag, clang, ir):
    nsc = declarations(ir)
    for symbol, shape in clang.items():
        if symbol not in nsc:
            raise SystemExit(f'{tag}: falta la declaracion {symbol} en el IR de NSC; evidencia: {root}')
        if nsc[symbol] != shape:
            raise SystemExit(f'{tag}: {symbol} tiene {nsc[symbol]} en NSC y {shape} en Clang; evidencia: {root}')


# An executable with thread-locals is linked with lld-link, which writes the TLS directory, as the existing C TLS
# tests do; NSLink does not write it.
TLS_ENV = dict(env, NSC_LINKER='C:/clang-llvm/bin/lld-link.exe')


def build_and_run(tag, folder, opt, expected=EXPECTED, tls=False):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder,
        environment=TLS_ENV if tls else None)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'directo-O{opt}'
    clang = build_library(case, opt)
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text('int Main() { ' + NS_BODY + ' }\n', encoding='utf-8')
    crt = ''
    if TLS:
        (case / 'crt.h').write_text('#pragma once\n', encoding='utf-8')
        # Only the CRT's TLS support (tlssup in libcmt) is needed; the NativeSharp runtime brings the rest.
        crt = ''.join('<CppLibraryReference Header="../crt.h" Language="C" Standard="c11" Library="' + path.as_posix()
                      + '" />' for path in CRT_LIBRARIES[:1])
    (app / 'App.nsproj').write_text(project('T10App', reference('../') + crt, 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, tls=TLS)
    print(f'directo-O{opt}: control C++, IR de Clang y NativeSharp coinciden ({len(clang)} declaraciones)', flush=True)

    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text('namespace T10N { public class Bridge { public static int Run<T>(T unused) { '
                                          + TEMPLATE_BODY + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T10Producer', reference('../'), 'Producer.ns', True),
                                              encoding='utf-8')
    run(f'precompilado-O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
        '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
    for visibility in ('con-encabezado', 'sin-encabezado'):
        consumer = case / visibility
        consumer.mkdir()
        references = '<LibraryReference Include="../producer/Producer.lib" />'
        if visibility == 'con-encabezado':
            references += reference('../')
        elif (case / 'api.h').exists():
            (case / 'api.h').rename(case / 'api.h.oculto')
        (consumer / 'Main.ns').write_text('int Main() { return T10N.Bridge.Run<int>(0); }\n', encoding='utf-8')
        (consumer / 'App.nsproj').write_text(project('T10Consumer', references, 'Main.ns'), encoding='utf-8')
        ir = run(f'precompilado-O{opt}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                          '--cpp-clang-root=' + args.clang_root], consumer).stdout
        compare(f'precompilado-O{opt}-{visibility}', {k: v for k, v in clang.items() if 'bump' in k}, ir)
        if args.dll:
            shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
        build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, TEMPLATE_EXPECTED)
    print(f'precompilado-O{opt}: plantilla precompilada con y sin encabezado coincide con Clang', flush=True)

negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T10 { struct P { int x; }; struct N { ~N(); int v; }; '
                                'struct S { static const P fixed; static P open; static N value; static int keep(); }; '
                                'extern thread_local int dyn; extern constinit thread_local int fixedTls; }\n',
                                encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T10 { N::~N() {} const P S::fixed = {1}; P S::open = {2}; '
                                  'int S::keep() { return 0; } }\n', encoding='utf-8')
(negative / 'tls.cpp').write_text('#include "api.h"\nnamespace T10 { thread_local int dyn = S::keep() + 9; '
                                  'constinit thread_local int fixedTls = 0; }\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-tls', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'tls.cpp', '-o', 'tls.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj', 'tls.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
for name, body, message in (
        ('campo-const', 'T10.S.fixed.x = 3; return T10.S.keep();', 'not assignable'),
        ('record-no-trivial', 'return T10.S.keep() + T10.S.value.v;', "no compatible member named 'value'")):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T10Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado', flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T10.S.open.x = 5; return T10.S.fixed.x + T10.S.open.x - 6 + T10.S.keep(); }\n',
                                encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T10Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: el campo const se lee y el record trivial se escribe', flush=True)
# EN: T19-036. A thread_local without constinit used to be rejected here. Clang reaches it through the CRT's
#     on-demand TLS initialization (__tls_guard, __dyn_tls_on_demand_init); NSC emits the same guard. Both programs
#     start through the CRT and are compared.
# ES: T19-036. Un thread_local sin constinit se rechazaba aqui. Clang llega a el con la inicializacion TLS bajo
#     demanda del CRT (__tls_guard, __dyn_tls_on_demand_init); NSC emite la misma guarda. Los dos programas arrancan
#     por el CRT y se comparan.
folder = negative / 'tls-sin-constinit'
folder.mkdir()
(folder / 'control.cpp').write_text('#include "../api.h"\nint main() { return T10::S::keep() + T10::dyn + 5; }\n',
                                    encoding='utf-8')
control_ir = run('tls-sin-constinit-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                  '-S', '-emit-llvm', 'control.cpp', '-o', '-'], folder).stdout
run('tls-sin-constinit-control-obj', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c',
                                      'control.cpp', '-o', 'control.obj'], folder)
run('tls-sin-constinit-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe',
                                       'control.obj', '../api.lib', *[str(path) for path in CRT_LIBRARIES]], folder)
expected = run('tls-sin-constinit-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode
(folder / 'Main.ns').write_text('int Main() { return T10.S.keep() + T10.dyn + 5; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T10Tls', plain, 'Main.ns'), encoding='utf-8')
nsc_ir = run('tls-sin-constinit-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root], folder).stdout
if '@__dyn_tls_on_demand_init()' not in control_ir or '@__dyn_tls_on_demand_init()' not in nsc_ir:
    raise SystemExit(f'tls-sin-constinit: falta la guarda TLS bajo demanda en Clang o en NSC; evidencia: {root}')
command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
           '--no-artifact-cache', '-O0', '--crt-startup']
for path in CRT_LIBRARIES:
    command += ['--link-lib', str(path)]
run('tls-sin-constinit-build', command + ['-o', 'app.exe'], folder, environment=TLS_ENV)
result = run('tls-sin-constinit-run', [str(folder / 'app.exe')], folder, expected=None).returncode
if result != expected:
    raise SystemExit(f'tls-sin-constinit: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')
print(f'tls-sin-constinit: NativeSharp y Clang devuelven {result}, con la guarda TLS bajo demanda', flush=True)
print('OK: globales y campos estaticos C++ de tipo record, enum, arreglo y thread_local, frente a Clang; evidencia:', root)
