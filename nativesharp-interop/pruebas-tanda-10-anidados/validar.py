"""T10-005: records y enums C++ anidados en records C++.

Un record publico anidado en un record C++ importado es un tipo anidado NativeSharp de ese record,
a cualquier profundidad publica, como una clase anidada en codigo NativeSharp; un enum publico de un
record tambien, con sus enumeradores nombrados por el tipo. Un metodo estatico C++ toma y devuelve un
enum importado por valor. Compara un control C++ compilado por Clang 23.1.0 con el consumidor
NativeSharp, O0-O3, en biblioteca estatica y con --dll en DLL, directo y desde plantilla
precompilada con y sin encabezado, y exige los tipos fisicos de Clang en el IR de NSC. Los controles
negativos comprueban que un record anidado privado y un enum protegido se rechazan con el diagnostico de
acceso, como en Clang (T19-005), y la contraparte, que un enumerador de un enum sin `class` se nombra por
el record (`Outer.Red`), como en C++.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-anidados-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T10_EXPORT)
#define API __declspec(dllexport)
#elif defined(T10_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T10 {
struct Outer {
    struct Point { int x; int y; };
    struct Box { struct Cell { short v; }; };
    enum Color { Red = 1, Green = 2, Blue = 4 };
    enum class Mode : short { Slow = 3, Fast = 7 };
    struct Inner { API static int twice(int x); };
    API static int scale(Color c, Mode m);
    API static int at(Point* p);
    API static Mode flip(Mode m);
};
API int measure(Outer::Point p);
API int cell(Outer::Box::Cell c);
API int shade(Outer::Color c);
API Outer::Mode pick(int fast);
API int mode(Outer::Mode m);
}
struct Gl { enum K { Q = 5 }; };
API int useg(Gl::K k);
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace T10 {
int Outer::Inner::twice(int x) { return x * 2; }
int Outer::scale(Color c, Mode m) { return (int)c * (int)m; }
int Outer::at(Point* p) { return p->x + p->y; }
Outer::Mode Outer::flip(Mode m) { return m == Mode::Slow ? Mode::Fast : Mode::Slow; }
int measure(Outer::Point p) { return p.x * 10 + p.y; }
int cell(Outer::Box::Cell c) { return c.v + 1; }
int shade(Outer::Color c) { return (int)c * 2; }
Outer::Mode pick(int fast) { return fast ? Outer::Mode::Fast : Outer::Mode::Slow; }
int mode(Outer::Mode m) { return (int)m; }
}
int useg(Gl::K k) { return (int)k; }
'''
CONTROL = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
int main() {
    T10::Outer::Point p{2, 3};
    T10::Outer::Box::Cell c{9};
    return T10::measure(p) + T10::cell(c) + T10::shade(T10::Outer::Blue) + T10::mode(T10::pick(1)) +
           T10::Outer::scale(T10::Outer::Green, T10::Outer::Mode::Slow) + T10::Outer::at(&p) +
           T10::Outer::Inner::twice(4) + T10::mode(T10::Outer::flip(T10::Outer::Mode::Fast)) + useg(Gl::Q);
}
'''
NS_BODY = ('T10.Outer.Point p; p.x = 2; p.y = 3; T10.Outer.Box.Cell c; c.v = (short)9; '
           'return T10.measure(p) + T10.cell(c) + T10.shade(T10.Outer.Color.Blue) + T10.mode(T10.pick(1)) + '
           'T10.Outer.scale(T10.Outer.Color.Green, T10.Outer.Mode.Slow) + T10.Outer.at(&p) + '
           'T10.Outer.Inner.twice(4) + T10.mode(T10.Outer.flip(T10.Outer.Mode.Fast)) + useg(Gl.K.Q);')
EXPECTED = 23 + 10 + 8 + 7 + 6 + 5 + 8 + 3 + 5
DECLARATIONS = 10


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
    """Physical parameter and return types of every T10 or Gl declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*(?:T10@@|Gl@@)[^"(]*)"?\((.*)\)', line)
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
    (app / 'App.nsproj').write_text(project('T10App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++, IR de Clang y NativeSharp coinciden ({len(clang)} declaraciones)', flush=True)

    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text('namespace T10N { public class Bridge { public static int Run<T>(T unused) { '
                                          + NS_BODY + ' } } }\n', encoding='utf-8')
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
        compare(f'precompilado-O{opt}-{visibility}', clang, ir)
        if args.dll:
            shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
        build_and_run(f'precompilado-O{opt}-{visibility}', consumer, opt)
    print(f'precompilado-O{opt}: plantilla precompilada con y sin encabezado coincide con Clang', flush=True)

negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T10 { struct Outer { enum Color { Red = 1 }; static int keep(); }; '
                                'class Hidden { struct Secret { static int zero(); }; protected: enum Mode { On = 1 }; '
                                'public: static int open(); }; }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T10 { int Outer::keep() { return 0; } '
                                  'int Hidden::Secret::zero() { return 0; } int Hidden::open() { return Secret::zero() + On; } }\n',
                                  encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
for name, body, message in (
        ('record-privado', 'return T10.Hidden.open() + T10.Hidden.Secret.zero();',
         "nested type 'T10.Hidden+Secret' is private"),
        ('enum-protegido', 'return T10.Hidden.open() + (int)T10.Hidden.Mode.On;',
         "nested type 'T10.Hidden+Mode' is protected")):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T10Neg', plain, 'Main.ns'), encoding='utf-8')
    output = run('negativo-' + name, [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                      '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                                      '-o', 'app.exe'], folder, expected=1).stdout
    if message not in output:
        raise SystemExit(f'negativo {name}: falta el diagnostico esperado; evidencia: {root}')
    print(f'negativo {name}: rechazado', flush=True)
for name, body in (('enum-publico', 'return T10.Outer.keep() + (int)T10.Outer.Color.Red - 1;'),
                   ('enumerador-en-record', 'return T10.Outer.keep() + (int)T10.Outer.Red - 1;'),
                   ('record-publico', 'return T10.Hidden.open() - 1;')):
    folder = negative / name
    folder.mkdir()
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(project('T10Pos', plain, 'Main.ns'), encoding='utf-8')
    build_and_run('contraparte-' + name, folder, 0, expected=0)
    print(f'contraparte {name}: compila y devuelve 0', flush=True)
print('OK: records y enums C++ anidados en records, frente a Clang; evidencia:', root)
