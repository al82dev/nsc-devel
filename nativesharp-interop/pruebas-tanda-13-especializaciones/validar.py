"""T13-003, T13-004, T13-010, T13-011, T13-012, T13-015, T13-018 y T13-023: especializaciones de plantillas
de clase nombradas en el encabezado.

El encabezado nombra con alias especializaciones de una plantilla primaria (Box<int>, Box<double>), de una
especializacion explicita (Box<char>) y de una parcial (Box<int*>), de una plantilla con parametro no tipo
(Arr<4>), de una plantilla con constructor y destructor (Counter<int>) y de una instanciada de forma
explicita en la biblioteca (Acc<long long>, con extern template), y usa Box<int> como campo de un record.
Clang instancia cada especializacion en la sesion; NSC la importa como record y Clang emite los miembros que
el programa llama. Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3,
y exige los tipos fisicos de Clang en cada declaracion C++ que NSC llama.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t13-especializaciones-', dir='A:/temp'))
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
    void set(T x) noexcept { v = x; }
    T twice() const noexcept;
};
template <class T> T Box<T>::twice() const noexcept { return v + v; }
template <> struct Box<char> { int special; int get() const noexcept { return special + 1000; } };
template <class T> struct Box<T*> { T* p; T deref() const noexcept { return *p; } };
template <int N> struct Arr {
    int a[N];
    int size() const noexcept { return N; }
    int sum() const noexcept { int s = 0; for (int i = 0; i < N; ++i) s += a[i]; return s; }
};
template <class T> struct Counter {
    T n;
    int* log;
    Counter(T start, int* sink) noexcept : n(start), log(sink) { *log += 1; }
    ~Counter() { *log += 10; }
    T next() noexcept { return ++n; }
};
template <class T> struct Acc { T total; T add(T x) noexcept; };
extern template struct API Acc<long long>;
using IntBox = Box<int>;
using DoubleBox = Box<double>;
using CharBox = Box<char>;
using IntPtrBox = Box<int*>;
using Arr4 = Arr<4>;
using IntCounter = Counter<int>;
using LAcc = Acc<long long>;
struct Holder { Box<int> inner; int k; };
API IntBox makeBox(int x) noexcept;
API int readBox(const IntBox* b) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T13 {
template <class T> T Acc<T>::add(T x) noexcept { total += x; return total; }
template struct API Acc<long long>;
IntBox makeBox(int x) noexcept { IntBox b; b.v = x; return b; }
int readBox(const IntBox* b) noexcept { return b->get(); }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    using namespace T13;
    int r = 0;
    IntBox b = makeBox(3);
    r += b.v + b.get() + b.twice();
    b.set(10);
    r += readBox(&b);
    DoubleBox d; d.v = 2.5; r += (int)(d.twice() * 2.0);
    CharBox cb; cb.special = 5; r += cb.get();
    int x = 7; IntPtrBox pb; pb.p = &x; r += pb.deref();
    Arr4 a; a.a[0] = 1; a.a[1] = 2; a.a[2] = 3; a.a[3] = 4; r += a.size() + a.sum();
    LAcc acc; acc.total = 1; r += (int)acc.add(2);
    int log = 0;
    { IntCounter c(5, &log); r += c.next(); }
    r += log;
    Holder h; h.inner.v = 4; h.k = 1; r += h.inner.get() + h.k;
    return r;
}
'''
NS_BODY = ('int r = 0; '
           'T13.IntBox b = T13.makeBox(3); '
           'r += b.v + b.get() + b.twice(); '
           'b.set(10); '
           'r += T13.readBox(&b); '
           'T13.DoubleBox d; d.v = 2.5; r += (int)(d.twice() * 2.0); '
           'T13.CharBox cb; cb.special = 5; r += cb.get(); '
           'int x = 7; T13.IntPtrBox pb; pb.p = &x; r += pb.deref(); '
           'T13.Arr4 a; a.a[0] = 1; a.a[1] = 2; a.a[2] = 3; a.a[3] = 4; r += a.size() + a.sum(); '
           'T13.LAcc acc; acc.total = 1; r += (int)acc.add(2); '
           'int log = 0; '
           'if (true) { T13.IntCounter c = new T13.IntCounter(5, &log); r += c.next(); } '
           'r += log; '
           'T13.Holder h; h.inner.v = 4; h.k = 1; r += h.inner.get() + h.k; '
           'return r;')
EXPECTED = 1083
# Direct calls: makeBox, readBox, the members of the specializations, the Counter constructor and destructor.
DIRECT = 14


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
    """Physical parameter and return types of every T12 declaration, keyed by mangled name."""
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
template <class T> struct Only { static_assert(sizeof(T) == 4, "four bytes"); T v; };
using Good = Only<int>;
using Bad = Only<double>;
}
''', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
folder = negative / 'instanciacion-rechazada'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T13.Bad x; x.v = 1.0; return 0; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T13Neg', plain, 'Main.ns'), encoding='utf-8')
output = run('negativo-instanciacion-rechazada', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                                  '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                                  '-o', 'app.exe'], folder, expected=1).stdout
if "C/C++ record alias cannot yet be resolved safely: 'T13.Bad'" not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo instanciacion-rechazada: falta el diagnostico esperado; evidencia: {root}')
print('negativo instanciacion-rechazada: rechazado', flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T13.Good x; x.v = 4; return x.v - 4; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T13Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: pasa', flush=True)
print('OK: especializaciones de plantillas de clase nombradas en el encabezado, frente a Clang; evidencia:', root)
