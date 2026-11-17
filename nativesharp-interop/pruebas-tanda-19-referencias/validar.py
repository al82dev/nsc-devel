"""T19-007: referencias C++ desde NativeSharp, frente a Clang 23.1.0, O0-O3, en estatica y DLL.

- Retornos `const T&` de funciones y metodos (`A::pick`, `A::limit`, `A::V::get`) como referencias de solo lectura,
  junto a `int& V::slot`. Escribir a traves de un resultado `const T&` se rechaza, igual que Clang.
- Caso `referencias-completas`: `T&` y `const T&` en metodos estaticos (parametros y retornos); parametros `T&&` en
  funciones, metodos estaticos y de instancia y constructores (un valor se materializa; `ref x` hace de
  `std::move(x)`; con `f(int&)` y `f(int&&)`, `ref x` elige `T&`); retornos `T&&` como referencias de solo lectura;
  operandos `T&` y `T&&` y retornos por referencia en operadores libres, temporales enlazados a operandos
  `const T&` y un operador miembro no const sobre un lugar y sobre un temporal. Se repite desde una plantilla
  NativeSharp precompilada, con y sin el encabezado C++ en el consumidor (los constructores, solo directo: T19-016).
  Los usos que Clang rechaza (un lvalue a `T&&`, un temporal a `T&`, escribir en un xvalue) se rechazan tambien.
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-referencias-', dir='A:/temp'))
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
HEADER = '''namespace A {
API const int& pick(const int& a, const int& b) noexcept;
API const int& limit() noexcept;
struct API V {
    int x[4];
    const int& get(int i) const noexcept;
    int& slot(int i) noexcept;
};
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace A {
static const int four = 4;
const int& pick(const int& a, const int& b) noexcept { return a < b ? b : a; }
const int& limit() noexcept { return four; }
const int& V::get(int i) const noexcept { return x[i]; }
int& V::slot(int i) noexcept { return x[i]; }
}
'''
CPP_BODY = ('int a = 3; int b = 9; int r = A::pick(a, b); A::V v; v.x[1] = 5; r = r + v.get(1) * 10; '
            'v.slot(2) = 7; r = r + v.x[2] * 100; r = r + A::limit() * 1000; return r;')
NS_BODY = ('int a = 3; int b = 9; int r = A.pick(in a, in b); A.V v; v.x[1] = 5; r = r + v.get(1) * 10; '
           'v.slot(2) = 7; r = r + v.x[2] * 100; r = r + A.limit() * 1000; return r;')


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


def project(case, body):
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'Main.ns').write_text('int Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19Referencias</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')


def nsc(case, opt, expected=0):
    return run(f'{case.name}-nsc-O{opt}', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                           '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                           '-O' + str(opt), '-o', 'app.exe'], case, expected=expected).stdout


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
library = root / 'lib'
library.mkdir()
(library / 'api.h').write_text(API + HEADER, encoding='utf-8')
(library / 'api.cpp').write_text(SOURCE, encoding='utf-8')
run('api', [*CXX, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], library)
if args.dll:
    run('api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll', '/implib:api.lib',
                    'api.obj'], library)
    header = '#define T19_IMPORT 1\n' + API + HEADER
else:
    run('api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
    header = API + HEADER


def prepare(name):
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text(header, encoding='utf-8')
    for item in ('api.lib', *(('api.dll',) if args.dll else ())):
        (case / item).write_bytes((library / item).read_bytes())
    return case


for opt in range(4):
    case = prepare(f'O{opt}')
    (case / 'control.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n'
                                      'int main() { ' + CPP_BODY + ' }\n', encoding='utf-8')
    run(f'O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    project(case, NS_BODY)
    nsc(case, opt)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}', flush=True)
# Writing through a `const T&` result is rejected, by Clang too.
# Escribir a traves de un resultado `const T&` se rechaza, tambien en Clang.
for label, cpp, ns in (
        ('funcion', 'int a = 3; int b = 9; A::pick(a, b) = 1; return a;',
         'int a = 3; int b = 9; A.pick(in a, in b) = 1; return a;'),
        ('metodo', 'A::V v{}; v.get(1) = 2; return 0;', 'A.V v; v.get(1) = 2; return 0;')):
    case = prepare('rechazo-' + label)
    (case / 'control.cpp').write_text('#include "api.h"\nint main() { ' + cpp + ' }\n', encoding='utf-8')
    clang = run(case.name + '-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], case, expected=1).stdout
    if 'cannot assign to return value' not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(f'{case.name}: Clang no rechaza la escritura; evidencia: {root}')
    project(case, ns)
    output = nsc(case, 0, expected=1)
    if 'error:' not in output or 'assign' not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{case.name}: falta el rechazo de la escritura; evidencia: {root}')
    print(f'{case.name}: escribir a traves de const T& se rechaza, igual que Clang', flush=True)

# EN: T19-007. References in static methods, `T&&` parameters and reference operands of free operators.
# ES: T19-007. Referencias en metodos estaticos, parametros `T&&` y operandos por referencia de operadores libres.
FULL_HEADER = '''namespace B {
struct API S {
    int k;
    static int& cell(int i) noexcept;
    static const int& larger(const int& a, const int& b) noexcept;
    static void bump(int& x) noexcept;
    int eat(int&& v) noexcept;
    static int twice(int&& v) noexcept;
    static int&& sfwd(int& x) noexcept;
    int&& own() noexcept;
};
API int take(int&& v) noexcept;
API int&& fwd(int& x) noexcept;
API int pick(int& v) noexcept;
API int pick(int&& v) noexcept;
struct P { int x; };
API bool operator==(P& a, P& b) noexcept;
API P& operator<<(P& a, int v) noexcept;
API P operator+(const P& a, const P& b) noexcept;
API P operator-(P&& a, P&& b) noexcept;
API P make(int v) noexcept;
struct M { int n; API int operator-(int v) noexcept; };
API M makeM(int n) noexcept;
struct API W {
    int v;
    W(int&& a) noexcept;
    W(int a, int&& b) noexcept;
};
}
'''
FULL_SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace B {
static int cells[3] = {1, 2, 3};
int& S::cell(int i) noexcept { return cells[i]; }
const int& S::larger(const int& a, const int& b) noexcept { return a < b ? b : a; }
void S::bump(int& x) noexcept { x += 5; }
int S::eat(int&& v) noexcept { v += k; return v; }
int S::twice(int&& v) noexcept { return v * 2; }
int&& S::sfwd(int& x) noexcept { return static_cast<int&&>(x); }
int&& S::own() noexcept { return static_cast<int&&>(k); }
int take(int&& v) noexcept { v += 1; return v * 2; }
int&& fwd(int& x) noexcept { return static_cast<int&&>(x); }
int pick(int& v) noexcept { return 1; }
int pick(int&& v) noexcept { return 2; }
bool operator==(P& a, P& b) noexcept { return a.x == b.x; }
P& operator<<(P& a, int v) noexcept { a.x = a.x * 10 + v; return a; }
P operator+(const P& a, const P& b) noexcept { return P{a.x + b.x}; }
P operator-(P&& a, P&& b) noexcept { return P{a.x - b.x}; }
P make(int v) noexcept { return P{v}; }
int M::operator-(int v) noexcept { n = n - v; return n * 2; }
M makeM(int n) noexcept { return M{n}; }
W::W(int&& a) noexcept : v(a * 7) {}
W::W(int a, int&& b) noexcept : v(a * 10 + b) {}
}
'''
# The same statements in both languages; `mix` folds each result in. / Las mismas sentencias en ambos lenguajes.
FULL_STEPS = [
    ('B::S::cell(1) = 40; mix(B::S::cell(1));', 'B.S.cell(1) = 40; mix(B.S.cell(1));'),
    ('int a = 4; int b = 9; mix(B::S::larger(a, b));', 'int a = 4; int b = 9; mix(B.S.larger(in a, in b));'),
    ('B::S::bump(a); mix(a);', 'B.S.bump(ref a); mix(a);'),
    ('B::S s; s.k = 2; mix(s.eat(5)); mix(B::S::twice(4));', 'B.S s; s.k = 2; mix(s.eat(5)); mix(B.S.twice(4));'),
    ('mix(B::take(5)); short h = 3; mix(B::take(h));', 'mix(B.take(5)); short h = 3; mix(B.take(h));'),
    ('mix(B::take(static_cast<int&&>(a))); mix(a);', 'mix(B.take(ref a)); mix(a);'),
    ('mix(B::pick(a)); mix(B::pick(7));', 'mix(B.pick(ref a)); mix(B.pick(7));'),
    ('mix(B::fwd(a)); mix(B::S::sfwd(a) + 1); mix(s.own() + 3);',
     'mix(B.fwd(ref a)); mix(B.S.sfwd(ref a) + 1); mix(s.own() + 3);'),
    ('B::M m{9}; mix(m - 4); mix(m.n); mix(B::makeM(7) - 2);',
     'B.M m; m.n = 9; mix(m - 4); mix(m.n); mix(B.makeM(7) - 2);'),
    ('B::P p{3}; B::P q{3}; B::P u{4}; mix(p == q ? 1 : 0); mix(p == u ? 1 : 0);',
     'B.P p; p.x = 3; B.P q; q.x = 3; B.P u; u.x = 4; mix(p == q ? 1 : 0); mix(p == u ? 1 : 0);'),
    ('B::P c{1}; mix(((c << 2) << 3).x); mix(c.x);', 'B.P c; c.x = 1; mix(((c << 2) << 3).x); mix(c.x);'),
    ('mix((B::make(1) + B::make(2) + B::make(3)).x); mix((B::make(5) - B::make(2)).x);',
     'mix((B.make(1) + B.make(2) + B.make(3)).x); mix((B.make(5) - B.make(2)).x);'),
]
CTOR_STEPS = [('B::W w1(6); B::W w2(2, 3); mix(w1.v); mix(w2.v);',
               'B.W w1 = new B.W(6); B.W w2 = new B.W(2, 3); mix(w1.v); mix(w2.v);')]


def full_bodies(steps):
    cpp = 'int r = 0; auto mix = [&](int v) { r = (r * 31 + v) % 1000003; }; '
    ns = ''
    for c, n in steps:
        cpp += c + ' '
        ns += n + ' '
    return cpp + 'return r;', ns


full = root / 'completo'
full.mkdir()
(full / 'api.h').write_text(API + FULL_HEADER, encoding='utf-8')
(full / 'api.cpp').write_text(FULL_SOURCE, encoding='utf-8')
run('completo-api', [*CXX, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], full)
if args.dll:
    run('completo-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                         '/implib:api.lib', 'api.obj'], full)
    full_header = '#define T19_IMPORT 1\n' + API + FULL_HEADER
else:
    run('completo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], full)
    full_header = API + FULL_HEADER
(full / 'api.h').write_text(full_header, encoding='utf-8')
NS_MIX = ('namespace T19R { public class Mixer { public static int r; '
          'public static void mix(int v) { r = (r * 31 + v) % 1000003; } } }\n')


def full_case(name, body, prelude=''):
    case = full / name
    case.mkdir()
    for item in ('api.h', 'api.lib', *(('api.dll',) if args.dll else ())):
        (case / item).write_bytes((full / item).read_bytes())
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'Main.ns').write_text(NS_MIX + prelude + 'int Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19Referencias</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')
    return case


def ns_body(steps):
    return 'T19R.Mixer.r = 0; ' + full_bodies(steps)[1].replace('mix(', 'T19R.Mixer.mix(') + 'return T19R.Mixer.r;'


for opt in range(4):
    cpp, _ = full_bodies(FULL_STEPS + CTOR_STEPS)
    case = full_case(f'O{opt}', ns_body(FULL_STEPS + CTOR_STEPS))
    (case / 'control.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n'
                                      'int main() { ' + cpp + ' }\n', encoding='utf-8')
    run(f'completo-O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                                     '-Wl,/entry:main', '-o', 'control.exe'], case)
    control = run(f'completo-O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    nsc(case, opt)
    result = run(f'completo-O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'completo O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    # The same body, without the constructors (T19-016), from a precompiled NativeSharp template.
    # El mismo cuerpo, sin los constructores (T19-016), desde una plantilla NativeSharp precompilada.
    cpp, _ = full_bodies(FULL_STEPS)
    pre = full / f'pre-O{opt}'
    pre.mkdir()
    if args.dll:
        (pre / 'api.dll').write_bytes((full / 'api.dll').read_bytes())
    (pre / 'control.cpp').write_text('#include "../api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n'
                                     'int main() { ' + cpp + ' }\n', encoding='utf-8')
    run(f'pre-O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', '../api.lib', '-fuse-ld=lld', '-nostdlib',
                                '-Wl,/entry:main', '-o', 'control.exe'], pre)
    pre_control = run(f'pre-O{opt}-control-run', [str(pre / 'control.exe')], pre, expected=None).returncode
    producer = pre / 'producer'
    producer.mkdir()
    runtime = ' Runtime="../../api.dll"' if args.dll else ''
    (producer / 'Producer.ns').write_text(NS_MIX + 'namespace T19P { public class Bridge { public static int Run<U>(U unused) '
                                          '{ ' + ns_body(FULL_STEPS) + ' } } }\n', encoding='utf-8')
    (producer / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19RefProducer</Name><OutputType>Library</OutputType></PropertyGroup>'
        '<ItemGroup><Compile Include="Producer.ns" /><CppLibraryReference Header="../../api.h" Language="C++" '
        'Standard="c++20" Library="../../api.lib"' + runtime + ' /></ItemGroup></Project>', encoding='utf-8')
    run(f'pre-O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                 '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
    for visibility in ('con-encabezado', 'sin-encabezado'):
        consumer = pre / visibility
        consumer.mkdir()
        references = '<LibraryReference Include="../producer/Producer.lib" />'
        if visibility == 'con-encabezado':
            references += ('<CppLibraryReference Header="../../api.h" Language="C++" Standard="c++20" '
                           'Library="../../api.lib"' + runtime + ' />')
        else:
            (full / 'api.h').rename(full / 'api.h.oculto')
        if args.dll:
            (consumer / 'api.dll').write_bytes((full / 'api.dll').read_bytes())
        (consumer / 'Main.ns').write_text('int Main() { return T19P.Bridge.Run<int>(0); }\n', encoding='utf-8')
        (consumer / 'App.nsproj').write_text(
            '<Project><PropertyGroup><Name>T19RefConsumer</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" />' + references + '</ItemGroup></Project>', encoding='utf-8')
        try:
            run(f'pre-O{opt}-{visibility}', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                             '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                             '-O' + str(opt), '-o', 'app.exe'], consumer)
        finally:
            if visibility == 'sin-encabezado':
                (full / 'api.h.oculto').rename(full / 'api.h')
        value = run(f'pre-O{opt}-{visibility}-run', [str(consumer / 'app.exe')], consumer, expected=None).returncode
        if value != pre_control:
            raise SystemExit(f'pre O{opt} {visibility}: NativeSharp devolvio {value}, Clang {pre_control}; '
                             f'evidencia: {root}')
    print(f'referencias-completas O{opt}: NativeSharp y Clang devuelven {result}; plantilla precompilada con y sin '
          f'encabezado, {pre_control}', flush=True)
# Uses that Clang rejects are rejected by NativeSharp too. / Los usos que Clang rechaza tambien se rechazan.
for label, cpp, ns in (
        ('lvalue-a-rvalue', 'int a = 5; return B::take(a);', 'int a = 5; return B.take(a);'),
        ('lvalue-a-operador-rvalue', 'B::P p = B::make(5); return (p - B::make(2)).x;',
         'B.P p = B.make(5); return (p - B.make(2)).x;'),
        ('temporal-a-referencia', 'return B::make(1) == B::make(1) ? 1 : 0;',
         'return B.make(1) == B.make(1) ? 1 : 0;'),
        ('escritura-a-xvalue', 'int a = 4; B::fwd(a) = 3; return a;', 'int a = 4; B.fwd(ref a) = 3; return a;')):
    case = full_case('rechazo-' + label, ns)
    (case / 'control.cpp').write_text('#include "api.h"\nint main() { ' + cpp + ' }\n', encoding='utf-8')
    run(case.name + '-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], case, expected=1)
    output = nsc(case, 0, expected=1)
    if 'error:' not in output or 'internal error' in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{case.name}: falta el rechazo; evidencia: {root}')
    print(f'{case.name}: se rechaza, igual que Clang', flush=True)
print('OK: referencias C++ (const T&, T& y T&& en funciones, metodos, constructores y operadores), frente a Clang; '
      'evidencia:', root)
