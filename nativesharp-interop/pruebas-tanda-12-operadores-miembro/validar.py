"""T12-004: operadores miembro de records C++ usados como operadores NativeSharp.

V2 declara operadores miembro const (suma, igualdad, negacion y producto por un escalar), Acc es un record
con constructor y destructor cuyo operador toma el objeto por referencia const, y D, que deriva de B,
declara un operador que devuelve D por valor. El consumidor NativeSharp los usa como operadores; NSC llama
a un adaptador que genera Clang, una funcion libre cuyo cuerpo llama al miembro. Compara un control C++
compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, y exige los tipos fisicos de Clang en
cada declaracion C++ que NSC llama directamente. T19-007: N declara un operador miembro no const, que se
aplica a un lugar asignable y a un temporal, como en C++. Control negativo: ese operador sobre un objeto
`in` (const), que Clang tambien rechaza. X2: un operador heredado de una base (antes control negativo, no se
importaba) ahora se aplica al derivado, como en C++.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t12-operadores-', dir='A:/temp'))
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
struct V2 {
    int x, y;
    API V2 operator+(const V2& o) const noexcept;
    API bool operator==(const V2& o) const noexcept;
    API V2 operator-() const noexcept;
    API int operator*(int k) const noexcept;
};
struct Acc { int total; API Acc(int t) noexcept; API ~Acc(); API int operator+(int v) const noexcept; };
struct B { int b; API int operator%(int m) const noexcept; };
struct D : B { int d; API D operator+(const D& o) const noexcept; };
struct N { int n; API int operator-(int v) noexcept; };
API V2 makeV(int x, int y) noexcept;
API N makeN(int n) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T12 {
V2 V2::operator+(const V2& o) const noexcept { V2 r; r.x = x + o.x; r.y = y + o.y; return r; }
bool V2::operator==(const V2& o) const noexcept { return x == o.x && y == o.y; }
V2 V2::operator-() const noexcept { V2 r; r.x = -x; r.y = -y; return r; }
int V2::operator*(int k) const noexcept { return x * k + y; }
Acc::Acc(int t) noexcept : total(t) {}
Acc::~Acc() {}
int Acc::operator+(int v) const noexcept { return total + v; }
int B::operator%(int m) const noexcept { return b % m; }
D D::operator+(const D& o) const noexcept { D r; r.b = b + o.b; r.d = d + o.d; return r; }
V2 makeV(int x, int y) noexcept { V2 v; v.x = x; v.y = y; return v; }
int N::operator-(int v) noexcept { n = n - v; return n * 2; }
N makeN(int n) noexcept { N r; r.n = n; return r; }
}
'''
CONTROL = '''#include "api.h"
int main() {
    using namespace T12;
    int r = 0;
    V2 a = makeV(1, 2);
    V2 b = makeV(10, 20);
    V2 c = a + b;
    r += c.x + c.y;
    if (a == a) { r += 1; }
    if (!(a == b)) { r += 2; }
    V2 n = -a;
    r += n.x + n.y;
    r += a * 5;
    Acc acc(100);
    r += acc + 5;
    D d1; d1.b = 1; d1.d = 2;
    D d2 = d1 + d1;
    r += d2.b + d2.d;
    N m; m.n = 9;
    r += m - 4;
    r += m.n;
    r += makeN(7) - 2;
    return r;
}
'''
NS_BODY = ('int r = 0; '
           'T12.V2 a = T12.makeV(1, 2); '
           'T12.V2 b = T12.makeV(10, 20); '
           'T12.V2 c = a + b; '
           'r += c.x + c.y; '
           'if (a == a) { r += 1; } '
           'if (!(a == b)) { r += 2; } '
           'T12.V2 n = -a; '
           'r += n.x + n.y; '
           'r += a * 5; '
           'T12.Acc acc = new T12.Acc(100); '
           'r += acc + 5; '
           'T12.D d1; d1.b = 1; d1.d = 2; '
           'T12.D d2 = d1 + d1; '
           'r += d2.b + d2.d; '
           'T12.N m; m.n = 9; '
           'r += m - 4; '
           'r += m.n; '
           'r += T12.makeN(7) - 2; '
           'return r;')
EXPECTED = 176
# Direct C++ calls: makeV, makeN and the Acc constructor and destructor. Operators go through 7 Clang adapters.
DIRECT = 4
ADAPTERS = 7


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
    adapters = set(re.findall(r'(?:call|invoke) [^@]*@(__nsc_cpp_call_[0-9a-f]{16})\(', ir))
    if len(adapters) != ADAPTERS:
        raise SystemExit(f'{tag}: se esperaban {ADAPTERS} adaptadores de Clang, hay {len(adapters)}; evidencia: {root}')
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
    print(f'directo-O{opt}: control C++ y NativeSharp coinciden ({direct} declaraciones iguales a las de Clang, '
          f'{ADAPTERS} adaptadores)', flush=True)


negative = root / 'negativos'
negative.mkdir()
(negative / 'api.h').write_text('''#pragma once
namespace T12 {
struct N { int n; int operator-(int v) noexcept; };
struct B { int b; int operator%(int m) const noexcept; };
struct D : B { int d; };
}
''', encoding='utf-8')
(negative / 'api.cpp').write_text('''#include "api.h"
namespace T12 {
int N::operator-(int v) noexcept { return n - v; }
int B::operator%(int m) const noexcept { return b % m; }
}
''', encoding='utf-8')
run('negativos-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('negativos-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
plain = '<CppLibraryReference Header="../api.h" Language="C++" Standard="c++20" Library="../api.lib" />'
# EN: X2. An operator a base declares is the derived record's too (C++ member lookup); this was a negative
#     control while inherited operators were not imported. / Era un control negativo mientras los operadores
#     heredados no se importaban; ahora D usa el operator% de B, como en C++ (5 % 3 - 2 == 0).
folder = negative / 'operador-heredado'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T12.D x; x.b = 5; x.d = 1; return x % 3 - 2; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T12Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('operador-heredado', folder, 0, expected=0)
print('operador-heredado: pasa (operador de la base aplicado al derivado)', flush=True)
# EN: T19-007. A non-const member operator does not apply to a const object, by Clang either.
# ES: T19-007. Un operador miembro no const no se aplica a un objeto const, tampoco en Clang.
folder = negative / 'operador-no-const-sobre-in'
folder.mkdir()
(folder / 'control.cpp').write_text('#include "../api.h"\nint f(const T12::N& x) { return x - 1; }\n'
                                    'int main() { T12::N y{5}; return f(y); }\n', encoding='utf-8')
clang = run('operador-no-const-sobre-in-control', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
            '-c', 'control.cpp', '-o', 'control.obj'], folder, expected=1).stdout
if 'error:' not in clang:
    print(clang[-2000:], flush=True)
    raise SystemExit(f'operador-no-const-sobre-in: Clang no rechaza el objeto const; evidencia: {root}')
(folder / 'Main.ns').write_text('int F(in T12.N x) { return x - 1; }\n'
                                'int Main() { T12.N y; y.n = 5; return F(in y); }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T12Neg', plain, 'Main.ns'), encoding='utf-8')
output = run('negativo-operador-no-const-sobre-in', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
             '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', 'app.exe'], folder,
             expected=1).stdout
if "overload of 'op_Subtraction'" not in output and 'binary operator' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo operador-no-const-sobre-in: falta el diagnostico de operador; evidencia: {root}')
print('negativo operador-no-const-sobre-in: rechazado, igual que Clang', flush=True)
folder = negative / 'contraparte'
folder.mkdir()
(folder / 'Main.ns').write_text('int Main() { T12.B x; x.b = 5; return x % 3 - 2; }\n', encoding='utf-8')
(folder / 'App.nsproj').write_text(project('T12Pos', plain, 'Main.ns'), encoding='utf-8')
build_and_run('contraparte', folder, 0, expected=0)
print('contraparte: pasa', flush=True)
print('OK: operadores miembro C++ como operadores NativeSharp, frente a Clang; evidencia:', root)
