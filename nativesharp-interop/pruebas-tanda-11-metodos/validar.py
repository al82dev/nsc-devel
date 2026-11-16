"""T11-005 y T11-010: metodos de instancia C++ con enums, records, referencias y nombres reservados.

Un metodo de instancia C++ toma y devuelve enums y records C++ triviales por valor, toma `T&` como
parametro `ref` y `const T&` como `in`, y devuelve `T&` como retorno `ref` ligado al receptor. Los
metodos `get` y `set`, palabras reservadas de NativeSharp, se llaman tras `.` y `->`. Compara un
control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, en biblioteca estatica
y con --dll en DLL, y exige los tipos fisicos de Clang en el IR de NSC (el MS ABI devuelve los records
de un metodo en memoria). Los metodos de instancia aun no se transportan en una plantilla
precompilada. Los controles negativos comprueban que un retorno `const T&` y un parametro `T&&` no se
proyectan.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t11-metodos-', dir='A:/temp'))
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
struct Q { int a; int b; int c; };
struct C {
    int v;
    API int get() const;
    API void set(int x);
    API int scale(E e) const;
    API E kind() const;
    API P scaled(P p) const;
    API Q grow(Q q) const;
    API int read(const P& p) const;
    API void bump(int& x) const;
    API int& slot();
};
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T11 {
int C::get() const { return v; }
void C::set(int x) { v = x; }
int C::scale(E e) const { return v * (int)e; }
E C::kind() const { return E::Y; }
P C::scaled(P p) const { return {p.x * v, p.y * v}; }
Q C::grow(Q q) const { return {q.a + v, q.b + v, q.c + v}; }
int C::read(const P& p) const { return v + p.x + p.y; }
void C::bump(int& x) const { x += v; }
int& C::slot() { return v; }
}
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    using namespace T11;
    C c; c.v = 0;
    c.set(3);
    int r = c.get();
    r += c.scale(E::Y);
    if (c.kind() == E::Y) r += 1;
    P p{1, 2};
    P s = c.scaled(p);
    r += s.x + s.y;
    Q q{1, 2, 3};
    Q g = c.grow(q);
    r += g.a + g.b + g.c;
    r += c.read(p);
    int x = 1;
    c.bump(x);
    r += x;
    c.slot() = 5;
    C* h = &c;
    r += h->get();
    return r;
}
'''
NS_BODY = ('T11.C c; c.v = 0; c.set(3); int r = c.get(); r += c.scale(T11.E.Y); if (c.kind() == T11.E.Y) r += 1; '
           'T11.P p; p.x = 1; p.y = 2; T11.P s = c.scaled(p); r += s.x + s.y; '
           'T11.Q q; q.a = 1; q.b = 2; q.c = 3; T11.Q g = c.grow(q); r += g.a + g.b + g.c; '
           'r += c.read(in p); int x = 1; c.bump(ref x); r += x; c.slot() = 5; T11.C* h = &c; r += h->get(); return r;')
EXPECTED = 3 + 6 + 1 + 9 + 15 + 6 + 4 + 5
DECLARATIONS = 9


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
(negative / 'api.h').write_text('#pragma once\nnamespace T11 { struct C { int v; const int& view() const; int take(int&& x) const; '
                                'int plain() const; }; }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T11 { const int& C::view() const { return v; } '
                                  'int C::take(int&& x) const { return x; } int C::plain() const { return v; } }\n',
                                  encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
for name, body in (('parametro-rvalue', 'T11.C c; c.v = 1; return c.take(1);'),):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T11Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if 'no compatible member' not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'negativo {name}: falta el diagnostico; evidencia: {root}')
    print(f'negativo {name}: rechazado', flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T11.C c; c.v = 4; return c.plain() - 4; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T11Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: el metodo por valor del mismo record se aplica', flush=True)
# T19-007: a `const int&` result of a const method is read, as Clang reads it; writing through it is rejected by both.
# T19-007: un resultado `const int&` de un metodo const se lee, como en Clang; escribir a traves de el lo rechazan
# los dos.
folder = negative / 'retorno-const-ref'
folder.mkdir()
(folder / 'control.cpp').write_text('#include "../api.h"\nint main() { T11::C c; c.v = 5; return c.view(); }\n',
                                    encoding='utf-8')
run('retorno-const-ref-control', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', 'control.cpp',
                                  '../api.lib', '-fuse-ld=lld', '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'],
    folder)
control = run('retorno-const-ref-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode
(folder / 'Main.ns').write_text('int Main() { T11.C c; c.v = 5; return c.view(); }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T11Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('retorno-const-ref', folder, 0, expected=control)
print(f'retorno-const-ref: c.view() devuelve {control}, igual que Clang', flush=True)
(folder / 'escribe.cpp').write_text('#include "../api.h"\nint main() { T11::C c; c.view() = 2; return 0; }\n',
                                    encoding='utf-8')
clang = run('retorno-const-ref-escribe-control', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                                  '-c', 'escribe.cpp', '-o', 'escribe.obj'], folder, expected=1).stdout
if 'cannot assign to return value' not in clang:
    raise SystemExit(f'retorno-const-ref: Clang no rechaza la escritura; evidencia: {root}')
(folder / 'Main.ns').write_text('int Main() { T11.C c; c.view() = 2; return 0; }\n', encoding='utf-8')
output = run('negativo-retorno-const-ref-escribe', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                                    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                                    '-O0', '-o', 'app.exe'], folder, expected=1).stdout
if 'error:' not in output or 'assign' not in output:
    raise SystemExit(f'negativo retorno-const-ref-escribe: falta el diagnostico; evidencia: {root}')
print('negativo retorno-const-ref-escribe: rechazado, igual que Clang', flush=True)
print('OK: metodos de instancia C++ con enums, records y referencias, frente a Clang; evidencia:', root)
