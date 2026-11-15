"""T11-006, T11-009, T11-015 y T11-016: constructores C++ con enums, records, punteros y referencias.

Un constructor C++ toma escalares, enums, records triviales por valor, punteros a records, `T&` como
`ref` y `const T&` como `in`; el constructor de copia se invoca de forma explicita con
`new T(in a)` y, en un tipo sin constructor de copia, el de movimiento con `new T(ref a)`. Un
constructor eliminado no oculta a los demas. Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, en
biblioteca estatica y con --dll en DLL, y exige los tipos fisicos de Clang en el IR de NSC. Los
controles negativos comprueban que una copia con constructor eliminado, un constructor privado y un
destructor privado se rechazan.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t11-constructores-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T11_EXPORT)
#define API __declspec(dllexport)
#elif defined(T11_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T11 {
enum class E : short { X = 1, Y = 2 };
struct P { int x; int y; };
API int dtors();
struct C {
    int v;
    API C(int x) noexcept;
    API C(double x) noexcept;
    API C(E e) noexcept;
    API C(P p) noexcept;
    API C(const P* p) noexcept;
    API C(const C& other) noexcept;
    API C(C&& other) noexcept;
    API C(int& counter, int x) noexcept;
    API ~C();
};
struct M {
    int v;
    API M(int x) noexcept;
    API M(M&& other) noexcept;
    M(const M&) = delete;
    API ~M();
};
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T11 {
static int count = 0;
int dtors() { return count; }
C::C(int x) noexcept : v(x) {}
C::C(double x) noexcept : v((int)(x * 10)) {}
C::C(E e) noexcept : v((int)e * 10) {}
C::C(P p) noexcept : v(p.x + p.y) {}
C::C(const P* p) noexcept : v(p->x * p->y) {}
C::C(const C& other) noexcept : v(other.v + 100) {}
C::C(C&& other) noexcept : v(other.v + 1000) {}
C::C(int& counter, int x) noexcept : v(x) { counter += x; }
C::~C() { ++count; }
M::M(int x) noexcept : v(x) {}
M::M(M&& other) noexcept : v(other.v) { other.v = 0; }
M::~M() { ++count; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    using namespace T11;
    int r = 0;
    {
        C a(2); C b(1.5); C c(E::Y);
        P p{3, 4};
        C d(p); C e(&p); C f(a);
        int counter = 0;
        C g(counter, 5);
        r = a.v + b.v + c.v + d.v + e.v + f.v + g.v + counter;
        M m(5);
        M n(static_cast<M&&>(m));
        r += n.v * 10 + m.v;
    }
    return r + dtors();
}
'''
NS_BODY = ('int r = 0; if (true) { T11.C a = new T11.C(2); T11.C b = new T11.C(1.5); T11.C c = new T11.C(T11.E.Y); '
           'T11.P p; p.x = 3; p.y = 4; T11.C d = new T11.C(p); T11.C e = new T11.C(&p); T11.C f = new T11.C(in a); '
           'int counter = 0; T11.C g = new T11.C(ref counter, 5); '
           'r = a.v + b.v + c.v + d.v + e.v + f.v + g.v + counter; '
           'T11.M m = new T11.M(5); T11.M n = new T11.M(ref m); r += n.v * 10 + m.v; } return r + T11.dtors();')
EXPECTED = 2 + 15 + 20 + 7 + 12 + 102 + 5 + 5 + 50 + 9
DECLARATIONS = 12


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
    """Physical parameter and return types of every T11 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T11@@[^"(]*)"?\((.*)\)', line)
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
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT11_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T11_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    control_ir = run(case.name + '-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                 '-O0', '-S', '-emit-llvm', 'Control.cpp', '-o', '-'], case).stdout
    run(case.name + '-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(case.name + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                      '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
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
    (app / 'App.nsproj').write_text(project('T11App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++, IR de Clang y NativeSharp coinciden ({len(clang)} declaraciones)', flush=True)


negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T11 { struct U { int v; U(int x) noexcept; U(const U&) = delete; ~U(); }; '
                                'struct V { int v; V() noexcept; ~V(); private: V(int x) noexcept; }; '
                                'struct W { int v; W() noexcept; private: ~W(); }; }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T11 { U::U(int x) noexcept : v(x) {} U::~U() {} '
                                  'V::V() noexcept : v(1) {} V::V(int x) noexcept : v(x) {} V::~V() {} '
                                  'W::W() noexcept : v(1) {} W::~W() {} }\n', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
for name, body, message in (
        ('copia-eliminada', 'T11.U a = new T11.U(1); T11.U b = new T11.U(in a); return b.v;',
         'no viable constructor accepts the argument types'),
        ('constructor-privado', 'T11.V a = new T11.V(5); return a.v;',
         'matching constructor exists but is not accessible'),
        ('destructor-privado', 'T11.W a = new T11.W(); return a.v;', 'C++ local requires an accessible destructor')):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T11Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if message not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado', flush=True)
for name, body in (('contraparte-u', 'T11.U a = new T11.U(1); return a.v - 1;'),
                   ('contraparte-v', 'T11.V a = new T11.V(); return a.v - 1;')):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T11Pos', plain, 'Main.ns'), encoding='utf-8')
    build_and_run(name, folder, 0, expected=0)
    print(f'{name}: pasa', flush=True)
print('OK: constructores C++ con enums, records, punteros y referencias, frente a Clang; evidencia:', root)
