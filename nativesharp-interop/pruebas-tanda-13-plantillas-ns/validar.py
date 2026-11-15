"""T13-002, T13-005, T13-006, T13-012, T13-013, T13-014, T13-016 y T13-017: especializaciones de plantillas C++
escritas en NativeSharp.

El programa NativeSharp escribe especializaciones de plantillas de clase con argumentos de tipo basico,
punteros, records y enums C++, especializaciones anidadas (con `>>`), constantes enteras negativas y bool,
una especializacion parcial y una explicita, una plantilla con concept, un parametro plantilla de plantilla, y llama a plantillas de funcion con
argumentos explicitos, uno de retorno no deducible, un parametro no tipo, un paquete y dos plantillas
sobrecargadas. Sema de la sesion instancia cada una; NSC la importa con el nombre de su tipo canonico y Clang
emite los miembros que el programa llama. Compara un control C++ compilado por Clang 23.1.0 con el
consumidor NativeSharp, O0-O3, y exige los tipos fisicos de Clang en cada declaracion C++ que NSC llama.
Los rechazos (concept, static_assert, argumentos de mas, argumento no tipo invalido) llevan el diagnostico
de Clang y tienen su contraparte valida.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t13-plantillas-ns-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T13_EXPORT)
#define API __declspec(dllexport)
#elif defined(T13_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T13 {
template <class T> struct Box {
    T v;
    T get() const noexcept { return v; }
    T twice() const noexcept { return v + v; }
};
template <class T> struct Box<T*> { T* p; T deref() const noexcept { return *p; } };
template <> struct Box<bool> { int flag; int get() const noexcept { return flag ? 100 : 200; } };
template <int N> struct Arr { int a[N]; int size() const noexcept { return N; } };
template <long long N, bool B> struct Tag { int pad; int value() const noexcept { return B ? (int)N : -(int)N; } };
template <class A, class B> struct Pair { A a; B b; int total() const noexcept { return (int)a + (int)b; } };
struct Point { int x; int y; };
enum class Color : int { Red = 1, Green = 2 };
template <class T> struct Wrap { T inner; };
template <template <class> class C> struct Holder { C<int> c; int get() const noexcept { return c.get(); } };
template <class T> concept Small = sizeof(T) <= 4;
template <Small T> struct Tiny { T v; int size() const noexcept { return (int)sizeof(T); } };
template <class T> T twice(T x) noexcept { return x + x; }
template <class T, class U> T convert(U x) noexcept { return (T)x; }
template <int N> int scaled(int x) noexcept { return x * N; }
template <class... Ts> int count(Ts... xs) noexcept { return (int)sizeof...(Ts) + (0 + ... + (int)xs); }
template <class T> T pick(T a, T b) noexcept { return a > b ? a : b; }
template <class T> T pick(T a) noexcept { return a; }
API int libValue(const Box<int>* b) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T13 {
int libValue(const Box<int>* b) noexcept { return b->get() * 10; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    int r = 0;
    T13::Box<int> b; b.v = 3; r += b.get() + b.twice();
    int x = 7; T13::Box<int*> pb; pb.p = &x; r += pb.deref();
    T13::Box<bool> bb; bb.flag = 1; r += bb.get();
    T13::Arr<3> a; a.a[0] = 1; r += a.size();
    T13::Tag<-5, false> t; t.pad = 0; r += t.value();
    T13::Pair<short, double> p; p.a = 2; p.b = 3.0; r += p.total();
    T13::Wrap<T13::Point> w; w.inner.x = 4; w.inner.y = 5; r += w.inner.x + w.inner.y;
    T13::Wrap<T13::Box<int>> wb; wb.inner.v = 6; r += wb.inner.get();
    T13::Wrap<T13::Color> wc; wc.inner = T13::Color::Green; r += (int)wc.inner;
    T13::Wrap<T13::Point*> wp; wp.inner = &w.inner; r += wp.inner->y;
    T13::Tiny<short> ty; ty.v = 1; r += ty.size();
    T13::Holder<T13::Box> h; h.c.v = 11; r += h.get();
    r += T13::twice<int>(21);
    r += (int)T13::twice<double>(1.5);
    r += T13::convert<int, double>(7.9);
    r += T13::scaled<4>(5);
    r += T13::count<int, int, int>(1, 2, 3);
    r += T13::pick<int>(4, 9) + T13::pick<int>(8);
    r += T13::libValue(&b);
    return r;
}
'''
NS_BODY = ('int r = 0; '
           'T13.Box<int> b; b.v = 3; r += b.get() + b.twice(); '
           'int x = 7; T13.Box<int*> pb; pb.p = &x; r += pb.deref(); '
           'T13.Box<bool> bb; bb.flag = 1; r += bb.get(); '
           'T13.Arr<3> a; a.a[0] = 1; r += a.size(); '
           'T13.Tag<-5, false> t; t.pad = 0; r += t.value(); '
           'T13.Pair<short, double> p; p.a = 2; p.b = 3.0; r += p.total(); '
           'T13.Wrap<T13.Point> w; w.inner.x = 4; w.inner.y = 5; r += w.inner.x + w.inner.y; '
           'T13.Wrap<T13.Box<int>> wb; wb.inner.v = 6; r += wb.inner.get(); '
           'T13.Wrap<T13.Color> wc; wc.inner = T13.Color.Green; r += (int)wc.inner; '
           'T13.Wrap<T13.Point*> wp; wp.inner = &w.inner; r += wp.inner->y; '
           'T13.Tiny<short> ty; ty.v = 1; r += ty.size(); '
           'T13.Holder<T13.Box> h; h.c.v = 11; r += h.get(); '
           'r += T13.twice<int>(21); '
           'r += (int)T13.twice<double>(1.5); '
           'r += T13.convert<int, double>(7.9); '
           'r += T13.scaled<4>(5); '
           'r += T13.count<int, int, int>(1, 2, 3); '
           'r += T13.pick<int>(4, 9) + T13.pick<int>(8); '
           'r += T13.libValue(&b); '
           'return r;')
EXPECTED = 9 + 7 + 100 + 3 + 5 + 5 + 9 + 6 + 2 + 5 + 2 + 11 + 42 + 3 + 7 + 20 + 9 + 17 + 30
# Direct calls: the members of the specializations, the function template specializations and libValue.
DIRECT = 17


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
    """Physical parameter and return types of every T13 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T13@@[^"(]*)"?\((.*)\)', line)
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
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT13_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T13_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    control_ir = run(case.name + '-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                 '-O0', '-S', '-emit-llvm', 'Control.cpp', '-o', '-'], case).stdout
    run(case.name + '-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(case.name + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                      '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    control = run(case.name + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'{case.name}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    return declarations(control_ir)


def compare(tag, clang, ir):
    """Every C++ symbol NSC calls is one Clang's control declares, with the same physical types (an sret
    result is a leading ptr parameter and a void result)."""
    nsc = declarations(ir)
    called = set(re.findall(r'(?:call|invoke) [^@]*@"(\?[^"]*T13@@[^"]*)"\(', ir))
    for symbol in called:
        if symbol not in clang:
            raise SystemExit(f'{tag}: NSC llama a {symbol}, que el control de Clang no declara; evidencia: {root}')
        if clang[symbol] != nsc.get(symbol):
            raise SystemExit(f'{tag}: {symbol} tiene {nsc.get(symbol)} en NSC y {clang[symbol]} en Clang; '
                             f'evidencia: {root}')
    if len(called) != DIRECT:
        raise SystemExit(f'{tag}: se esperaban {DIRECT} llamadas C++ en NSC, hay {len(called)}; evidencia: {root}')
    return len(called)


def build_and_run(tag, folder, opt, expected=EXPECTED):
    run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                         '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
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
    (app / 'App.nsproj').write_text(project('T13App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    direct = compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden ({direct} declaraciones iguales a las de Clang)',
          flush=True)
negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('''#pragma once
namespace T13 {
struct Point { int x; int y; };
template <class T> concept Small = sizeof(T) <= 4;
template <Small T> struct Tiny { T v; };
template <class T> struct Only { static_assert(sizeof(T) == 4, "four bytes"); T v; };
template <class T> struct Box { T v; };
template <int N> int scaled(int x) noexcept { return x * N; }
}
''', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
CASES = [
    ('concept', 'T13.Tiny<double> x; x.v = 1.0; return 0;', "'T13::Tiny'", 'constraints not satisfied',
     'T13.Tiny<int> x; x.v = 4; return x.v - 4;'),
    ('static-assert', 'T13.Only<double> x; x.v = 1.0; return 0;', "'T13::Only'", 'static assertion failed',
     'T13.Only<int> x; x.v = 4; return x.v - 4;'),
    ('argumentos-de-mas', 'T13.Box<int, int> x; x.v = 1; return 0;', "'T13::Box'", 'too many template arguments',
     'T13.Box<int> x; x.v = 4; return x.v - 4;'),
    ('no-tipo-invalido', 'return T13.scaled<T13.Point>(1);', "'T13::scaled'", 'accepts the template arguments',
     'return T13.scaled<2>(3) - 6;'),
]
for name, body, template, clang_text, counterpart in CASES:
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T13Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if ('Clang cannot instantiate the C++ template ' + template) not in output or clang_text not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado con el diagnostico de Clang', flush=True)
    folder = negative / (name + '-contraparte')
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + counterpart + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T13Pos', plain, 'Main.ns'), encoding='utf-8')
    build_and_run(name + '-contraparte', folder, 0, expected=0)
    print(f'contraparte {name}: pasa', flush=True)
print('OK: especializaciones de plantillas C++ escritas en NativeSharp, frente a Clang; evidencia:', root)
