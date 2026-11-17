"""T19-008: funciones y miembros inline C++ declarados dllimport, frente a Clang 23.1.0.

El encabezado declara con `API` (dllexport al construir la DLL, dllimport al consumirla y vacio con la biblioteca
estatica) funciones inline y constexpr, metodos inline de una clase exportada (de instancia, estaticos, un
constructor y un destructor y un operador), un miembro estatico `inline` de datos, una funcion inline que llama a
otra no exportada y una funcion inline con una variable estatica local. Con la DLL, NativeSharp las llama por su
importacion, como Clang: la DLL tiene la unica copia, asi que la variable estatica local que el programa incrementa
es la misma que incrementa la DLL. Con la biblioteca estatica, Clang emite sus definiciones inline (T10-012) y el
COMDAT las fusiona con las de la biblioteca.

Compara un control C++ con el consumidor NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL, directo y
desde una plantilla precompilada con y sin encabezado, y exige que cada declaracion del IR de Clang este en el de
NSC con los mismos tipos fisicos y, en la DLL, tambien dllimport. Control negativo: una variable inline dllimport
con inicializador, que Clang rechaza (`definition of dllimport data`) y NSC tambien.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t19-dllimport-inline-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']

API = '''#pragma once
#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
HEADER = '''namespace T {
API inline int sq(int x) noexcept { return x * x; }
API constexpr int cube(int x) noexcept { return x * x * x; }
API int base() noexcept;
struct API S {
    int v;
    int get() const noexcept { return v + 1; }
    static int twice(int x) noexcept { return x * 2; }
    int out() const noexcept;
};
struct API W {
    int v;
    W(int a) noexcept : v(a * 2) {}
    ~W() {}
    int get() const noexcept { return v; }
};
struct API P { int v; int operator+(int k) const noexcept { return v + k; } };
inline int helper(int x) noexcept { return x + 1; }
API inline int wrap(int x) noexcept { return helper(x) * 3; }
struct API K { static inline int n = 5; static int get() noexcept { return n; } };
API inline int next() noexcept { static int c = 0; return ++c; }
API int fromLibrary() noexcept;
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T {
int base() noexcept { return 7; }
int S::out() const noexcept { return v * 100; }
int fromLibrary() noexcept { return next() * 10; }
}
'''
# The same statements in both languages; `mix` folds each result in. / Las mismas sentencias en ambos lenguajes.
STEPS = [
    ('mix(T::sq(5)); mix(T::base()); mix(T::cube(3));', 'mix(T.sq(5)); mix(T.base()); mix(T.cube(3));'),
    ('T::S s; s.v = 4; mix(s.get()); mix(T::S::twice(3)); mix(s.out());',
     'T.S s; s.v = 4; mix(s.get()); mix(T.S.twice(3)); mix(s.out());'),
    ('T::P p; p.v = 5; mix(p + 3);', 'T.P p; p.v = 5; mix(p + 3);'),
    ('mix(T::wrap(4)); mix(T::helper(1));', 'mix(T.wrap(4)); mix(T.helper(1));'),
    ('T::K::n = 9; mix(T::K::get()); mix(T::K::n);', 'T.K.n = 9; mix(T.K.get()); mix(T.K.n);'),
    ('int a = T::next(); int b = T::fromLibrary(); int c = T::next(); mix(a); mix(b); mix(c);',
     'int a = T.next(); int b = T.fromLibrary(); int c = T.next(); mix(a); mix(b); mix(c);'),
]
# Constructors from a precompiled template stay in T19-016. / Los constructores precompilados siguen en T19-016.
CTOR_STEPS = [('{ T::W w(4); mix(w.get()); }', 'T.W w = new T.W(4); mix(w.get());')]
NS_MIX = ('namespace T19D { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')


def cpp_body(steps):
    return ('int r = 0; auto mix = [&](int v) { r = (r * 31 + v) % 1000003; }; '
            + ' '.join(c for c, _ in steps) + ' return r;')


def ns_body(steps):
    return ('T19D.Mixer.r = 0; ' + ' '.join(n for _, n in steps).replace('mix(', 'T19D.Mixer.mix(')
            + ' return T19D.Mixer.r;')


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
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


def project(name, references, source, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def declarations(ir):
    """Physical types and dllimport of every T declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(declare|define)\s+(.*?)\s+@"?(\?[^"(]*@T@@[^"(]*)"?\((.*)\)', line)
        if not match:
            continue
        kind, head, symbol, parameters = match.groups()
        words = head.split()
        result = [w for w in words if w not in ('dso_local', 'dllimport', 'noundef', 'nonnull', 'zeroext', 'signext',
                                                'linkonce_odr', 'weak_odr', 'available_externally', 'comdat')
                  and not w.startswith(('dereferenceable', 'align'))]
        shapes = tuple(p.split()[0] for p in parameters.split(', ')) if parameters.strip() else ()
        found[symbol] = ('dllimport' in words, result[-1] if result else '', shapes)
    return found


def compare(tag, clang, ir):
    nsc = declarations(ir)
    for symbol, shape in clang.items():
        # A member operator is called through a Clang adapter (T12-004). / Un operador miembro, por un adaptador.
        if symbol.startswith('??') and symbol[2] not in '01':
            continue
        if symbol not in nsc:
            raise SystemExit(f'{tag}: falta la declaracion {symbol} en el IR de NSC; evidencia: {root}')
        # A static library: Clang defines an inline function in the control, NSC declares it and Clang emits it.
        # Biblioteca estatica: Clang define una funcion inline en el control, NSC la declara y Clang la emite.
        if nsc[symbol][1:] != shape[1:] or (args.dll and nsc[symbol][0] != shape[0]):
            raise SystemExit(f'{tag}: {symbol} tiene {nsc[symbol]} en NSC y {shape} en Clang; evidencia: {root}')
    return len(clang)


def build_and_run(tag, folder, opt, expected):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
    result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')


def control(case, opt, steps, name):
    (case / (name + '.cpp')).write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n'
                                       'int main() { ' + cpp_body(steps) + ' }\n', encoding='utf-8')
    ir = run(f'{case.name}-{name}-ir', [*CXX, '-O0', '-S', '-emit-llvm', name + '.cpp', '-o', '-'], case).stdout
    run(f'{case.name}-{name}-obj', [*CXX, '-O' + str(opt), '-c', name + '.cpp', '-o', name + '.obj'], case)
    run(f'{case.name}-{name}-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                     '/out:' + name + '.exe', name + '.obj', 'api.lib'], case)
    code = run(f'{case.name}-{name}-run', [str(case / (name + '.exe'))], case, expected=None).returncode
    return code, declarations(ir)


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(API + HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    flags = [*CXX, '-O' + str(opt)]
    run(f'O{opt}-api', [*flags, *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                            '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T19_IMPORT 1\n' + API + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    expected, clang = control(case, opt, STEPS + CTOR_STEPS, 'control')
    app = case / 'app'
    app.mkdir()
    (app / 'Main.ns').write_text(NS_MIX + 'int Main() { ' + ns_body(STEPS + CTOR_STEPS) + ' }\n', encoding='utf-8')
    (app / 'App.nsproj').write_text(project('T19DllImportInline', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj', '--cpp-clang-root=' + args.clang_root],
             app).stdout
    count = compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt, expected)
    print(f'directo-O{opt}: NativeSharp y Clang devuelven {expected}; {count} declaraciones iguales a las de Clang',
          flush=True)

    pre_expected, pre_clang = control(case, opt, STEPS, 'control-pre')
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<U>('
                                          'U unused) { ' + ns_body(STEPS) + ' } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T19DllImportProducer', reference('../'), 'Producer.ns', True),
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
        (consumer / 'App.nsproj').write_text(project('T19DllImportConsumer', references, 'Main.ns'),
                                             encoding='utf-8')
        try:
            ir = run(f'O{opt}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                  '--cpp-clang-root=' + args.clang_root], consumer).stdout
            compare(f'precompilado-O{opt}-{visibility}', pre_clang, ir)
            if args.dll:
                shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
            build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt, pre_expected)
        finally:
            if visibility == 'sin-encabezado':
                (case / 'api.h.oculto').rename(case / 'api.h')
    print(f'precompilado-O{opt}: plantilla precompilada con y sin encabezado devuelve {pre_expected}, como Clang',
          flush=True)

# A dllimport variable cannot be defined: Clang rejects the header and so does NSC.
# Una variable dllimport no se puede definir: Clang rechaza el encabezado y NSC tambien.
negative = root / 'negativo'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T { __declspec(dllimport) inline int counter = 3; }\n',
                                encoding='utf-8')
(negative / 'control.cpp').write_text('#include "api.h"\nint main() { return T::counter; }\n', encoding='utf-8')
clang_output = run('negativo-clang', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], negative, expected=1).stdout
if 'definition of dllimport data' not in clang_output:
    print(clang_output[-2000:], flush=True)
    raise SystemExit(f'negativo: Clang no rechaza la variable; evidencia: {root}')
(negative / 'empty.cpp').write_text('int t19_empty() { return 0; }\n', encoding='utf-8')
run('negativo-empty', [*CXX, '-c', 'empty.cpp', '-o', 'empty.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'empty.obj'], negative)
(negative / 'Main.ns').write_text('int Main() { return T.counter; }\n', encoding='utf-8')
(negative / 'App.nsproj').write_text(project('T19DllImportNeg', '<CppLibraryReference Header="api.h" Language="C++" '
                                     'Standard="c++20" Library="api.lib" />', 'Main.ns'), encoding='utf-8')
output = run('negativo-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                              '--no-artifact-cache', '-O0', '-o', 'app.exe'], negative, expected=1).stdout
if 'definition of dllimport data' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo: falta el diagnostico de Clang en NSC; evidencia: {root}')
print('negativo variable-inline-dllimport: rechazada, igual que Clang', flush=True)
print('OK: funciones y miembros inline C++ declarados dllimport, frente a Clang; evidencia:', root)
