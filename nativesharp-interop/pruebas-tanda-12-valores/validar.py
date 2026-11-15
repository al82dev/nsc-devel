"""T12-001 y T12-003: records C++ trivialmente copiables con bases, como valores NativeSharp.

Q deriva de P y R de Q, sin constructores ni destructor declarados: son trivialmente copiables pero no
de disposicion estandar. S declara un constructor. El consumidor NativeSharp declara locales sin
constructor, escribe y lee campos de la base y del derivado, copia, toma la direccion de un local y la
convierte a puntero a la base, y pasa y recibe estos records por valor. El ABI de Microsoft devuelve en
memoria un record con base o con constructor declarado aunque mida 8 bytes o menos: NSC lo toma de la
clasificacion de Clang. Compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp,
O0-O3, y exige los tipos fisicos de Clang en cada declaracion que NSC llama.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t12-valores-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T12_EXPORT)
#define API __declspec(dllexport)
#elif defined(T12_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T12 {
struct P { int x; };
struct Q : P { int y; };
struct R : Q { short z; };
struct S { int a; S() = default; S(int v) noexcept : a(v) {} };
API Q makeQ(int x, int y) noexcept;
API int sumQ(Q q) noexcept;
API R makeR(int x) noexcept;
API int sumR(R r) noexcept;
API int readP(const P* p) noexcept;
API S makeS(int v) noexcept;
API int readS(S s) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T12 {
Q makeQ(int x, int y) noexcept { Q q; q.x = x; q.y = y; return q; }
int sumQ(Q q) noexcept { return q.x + q.y; }
R makeR(int x) noexcept { R r; r.x = x; r.y = x + 1; r.z = (short)(x + 2); return r; }
int sumR(R r) noexcept { return r.x + r.y + r.z; }
int readP(const P* p) noexcept { return p->x; }
S makeS(int v) noexcept { return S(v); }
int readS(S s) noexcept { return s.a; }
}
'''
CONTROL = '''#include "api.h"
int main() {
    using namespace T12;
    int r = 0;
    Q q; q.x = 2; q.y = 3;
    r += sumQ(q);
    r += readP(&q);
    Q m = makeQ(10, 20);
    r += m.x + m.y;
    Q c = m;
    c.y = 1;
    r += c.x + c.y + m.y;
    R rr = makeR(4);
    r += rr.x + rr.y + rr.z + sumR(rr);
    S s = makeS(7);
    r += s.a + readS(s);
    return r;
}
'''
NS_BODY = ('int r = 0; '
           'T12.Q q; q.x = 2; q.y = 3; '
           'r += T12.sumQ(q); '
           'r += T12.readP(&q); '
           'T12.Q m = T12.makeQ(10, 20); '
           'r += m.x + m.y; '
           'T12.Q c = m; '
           'c.y = 1; '
           'r += c.x + c.y + m.y; '
           'T12.R rr = T12.makeR(4); '
           'r += rr.x + rr.y + rr.z + T12.sumR(rr); '
           'T12.S s = T12.makeS(7); '
           'r += s.a + T12.readS(s); '
           'return r;')
EXPECTED = 112
DIRECT = 7


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
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T12@@[^"(]*)"?\((.*)\)', line)
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
    run(case.name + '-api', ['clang++.exe', *flags, *(['-DT12_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                             '-o', 'api.obj'], case)
    if args.dll:
        run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                 '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T12_IMPORT 1\n' + HEADER, encoding='utf-8')
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
    called = set(re.findall(r'(?:call|invoke) [^@]*@"(\?[^"]*T12@@[^"]*)"\(', ir))
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
    (app / 'App.nsproj').write_text(project('T12App', reference('../'), 'Main.ns'), encoding='utf-8')
    ir = run(f'directo-O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                    '--cpp-clang-root=' + args.clang_root], app).stdout
    direct = compare(f'directo-O{opt}', clang, ir)
    if args.dll:
        shutil.copyfile(case / 'api.dll', app / 'api.dll')
    build_and_run(f'directo-O{opt}', app, opt)
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden ({direct} declaraciones iguales a las de Clang)',
          flush=True)
print('OK: records C++ trivialmente copiables con bases como valores, frente a Clang; evidencia:', root)
