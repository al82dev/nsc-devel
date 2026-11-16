"""T19-036: fixtures de capacidades trasladadas entre tandas sin evidencia propia.

Cada caso compara un programa NativeSharp con un control C++ de Clang 23.1.0, O0-O3, con la biblioteca como
estatica y, con --dll, como DLL. Los casos que todavia no funcionan estan en `casos_pendientes.py` y pasan a
este archivo cuando NativeSharp los consume como Clang (registro t19-traslados-verificacion.md):

- `clase-precompilada`: records declarados con `class` (miembros publicos), por valor en funciones de la biblioteca y
  con un metodo estatico inline, tambien desde una plantilla precompilada con y sin encabezado.
- `globales-precompilada` y `campos-estaticos-precompilada`: globales C++ de namespace (escalar y record) y campos
  estaticos (escalar, record y `const`) leidos y escritos desde NativeSharp, tambien desde una plantilla
  precompilada con y sin encabezado.
- `anidado-plantilla` y `anidado-plantilla-var`: record anidado en una especializacion de plantilla de clase
  (`T.Box<int>.Item`), con tipo explicito y con `var`, devuelto por un metodo estatico de la especializacion y
  pasado por valor a una funcion. Solo directo: una plantilla NativeSharp precompilada todavia no admite usos de
  plantillas C++ en su cuerpo (T19-016).
- `tls-campo`: campo estatico `constinit thread_local` leido y escrito desde NativeSharp (estatica, con el CRT). Con
  --dll el caso comprueba que Clang tambien rechaza un `thread_local` `dllimport`: el ABI de Microsoft no lo
  permite.
- `vectorcall-hva`: un record C++ `{double, double}` (agregado homogeneo) por valor como parametro y como resultado
  de funciones `__vectorcall`; viaja en registros XMM, como en Clang.
- `vectorcall-mixto`: siete argumentos `__vectorcall`: un double, un agregado de tres float que cabe en los
  registros que quedan, tres agregados de cuatro double que ya no caben y van por direccion (uno despues del sexto
  argumento), un record de 8 bytes en un entero y uno de 12 bytes por direccion; y los resultados de cada forma.
- `vectorcall-vectores`: agregados de vectores de Clang de 128 bits (`float` x4, `double` x2 e `int` x4, este en
  un arreglo de dos) por valor como parametro y como resultado; cada vector ocupa un registro XMM, y el agregado que
  ya no cabe va por direccion, como en Clang.
- `cdecl-constructor`: un record de 8 bytes con un constructor declarado (T12-001): como parametro viaja en un
  entero y como resultado por un puntero oculto, tambien desde una plantilla precompilada, que antes lo devolvia en
  un registro.
- `vectorcall-constructor`: un record `{double, double}` con un constructor declarado (el predeterminado sigue
  trivial): como parametro viaja en registros XMM y como resultado por un puntero oculto, porque el ABI de Microsoft
  no lo considera un agregado.

Los casos `vectorcall-*` y `cdecl-constructor` comparan tambien la forma de cada declaracion (convencion, `inreg`, `sret`, tipos) del IR
de NSC con la del IR de Clang, y repiten el programa desde una plantilla NativeSharp precompilada, con y sin el
encabezado C++ visible para el consumidor.

Uso: validar.py --nsc <nsc.exe> --clang-root <raiz de Clang> [--dll] [--caso <nombre>]
"""
import argparse
import os
import re
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
parser.add_argument('--caso')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-traslados-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
API = '''#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
CASES = {
    'tls-campo': {
        'header': '''namespace T {
struct S { static constinit thread_local int counter; };
API int bump() noexcept;
}
''',
        'source': '''namespace T {
constinit thread_local int S::counter = 5;
int bump() noexcept { S::counter = S::counter + 1; return S::counter; }
}
''',
        'cpp': 'int a = T::S::counter; T::S::counter = 40; int b = T::bump(); return a * 100 + b;',
        'ns': 'int a = T.S.counter; T.S.counter = 40; int b = T.bump(); return a * 100 + b;',
        'crt': True,
    },
    'clase-precompilada': {
        'header': '''namespace T {
class Pt {
public:
    int x;
    int y;
};
class Counter {
public:
    static int scale(int v) noexcept { return v * 10; }
    int count;
    int step;
};
API int area(Pt p) noexcept;
API Pt mk(int a) noexcept;
}
''',
        'source': '''namespace T {
int area(Pt p) noexcept { return p.x * p.y; }
Pt mk(int a) noexcept { Pt r; r.x = a; r.y = a * 2; return r; }
}
''',
        'cpp': 'T::Pt p = T::mk(3); p.y = p.y + 1; T::Counter c; c.count = 4; c.step = 2; '
               'return T::area(p) + p.x + T::Counter::scale(c.count * 10 + c.step) * 100;',
        'ns': 'T.Pt p = T.mk(3); p.y = p.y + 1; T.Counter c; c.count = 4; c.step = 2; '
              'return T.area(p) + p.x + T.Counter.scale(c.count * 10 + c.step) * 100;',
        'crt': False,
        'ir': True,
    },
    'globales-precompilada': {
        'header': '''namespace T {
struct P { int x; int y; };
API extern int counter;
API extern P origin;
API int bump() noexcept;
}
''',
        'source': '''namespace T {
int counter = 3;
P origin = {1, 2};
int bump() noexcept { counter = counter + 1; return counter * 10 + origin.y; }
}
''',
        'cpp': 'T::counter = 7; T::origin.y = T::origin.y + 1; int b = T::bump(); '
               'return b * 1000 + T::origin.y * 100 + T::origin.x * 10 + T::counter;',
        'ns': 'T.counter = 7; T.origin.y = T.origin.y + 1; int b = T.bump(); '
              'return b * 1000 + T.origin.y * 100 + T.origin.x * 10 + T.counter;',
        'crt': False,
        'ir': True,
    },
    'campos-estaticos-precompilada': {
        'header': '''namespace T {
struct P { int x; int y; };
struct S {
    API static int total;
    API static P corner;
    API static const int limit;
};
API int bump() noexcept;
}
''',
        'source': '''namespace T {
int S::total = 10;
P S::corner = {5, 6};
const int S::limit = 9;
int bump() noexcept { S::total = S::total + S::corner.y; return S::total; }
}
''',
        'cpp': 'T::S::total = 20; T::S::corner.x = T::S::corner.x + 2; int b = T::bump(); '
               'return b * 1000 + T::S::corner.x * 100 + T::S::limit * 10 + T::S::corner.y;',
        'ns': 'T.S.total = 20; T.S.corner.x = T.S.corner.x + 2; int b = T.bump(); '
              'return b * 1000 + T.S.corner.x * 100 + T.S.limit * 10 + T.S.corner.y;',
        'crt': False,
        'ir': True,
    },
    'anidado-plantilla': {
        'header': '''namespace T {
template <class V> struct Box {
    struct Item { V v; int tag; };
    static Item make(V v) noexcept { Item r; r.v = v; r.tag = 3; return r; }
};
API int seen(Box<int>::Item item) noexcept;
}
''',
        'source': 'namespace T { int seen(Box<int>::Item item) noexcept { return item.v * 100 + item.tag; } }\n',
        'cpp': 'T::Box<int>::Item it = T::Box<int>::make(4); it.tag = it.tag + 1; return T::seen(it) + it.v * 10;',
        'ns': 'T.Box<int>.Item it = T.Box<int>.make(4); it.tag = it.tag + 1; return T.seen(it) + it.v * 10;',
        'crt': False,
        'ir': True,
        'pre': False,
    },
    'anidado-plantilla-var': {
        'header': '''namespace T {
template <class V> struct Box {
    struct Item { V v; int tag; };
    static Item make(V v) noexcept { Item r; r.v = v; r.tag = 3; return r; }
};
API int seen(Box<int>::Item item) noexcept;
}
''',
        'source': 'namespace T { int seen(Box<int>::Item item) noexcept { return item.v * 100 + item.tag; } }\n',
        'cpp': 'auto it = T::Box<int>::make(4); it.tag = it.tag + 1; return T::seen(it) + it.v * 10;',
        'ns': 'var it = T.Box<int>.make(4); it.tag = it.tag + 1; return T.seen(it) + it.v * 10;',
        'crt': False,
        'ir': True,
        'pre': False,
    },
    'vectorcall-hva': {
        'header': '''namespace T {
struct V2 { double x; double y; };
API double __vectorcall sum(V2 v, double k) noexcept;
API V2 __vectorcall make(double a) noexcept;
}
''',
        'source': '''namespace T {
double __vectorcall sum(V2 v, double k) noexcept { return v.x * k + v.y; }
V2 __vectorcall make(double a) noexcept { V2 r; r.x = a; r.y = a * 2.0; return r; }
}
''',
        'cpp': 'T::V2 v = T::make(1.5); return (int)(T::sum(v, 10.0) * 10.0) + (int)v.y;',
        'ns': 'T.V2 v = T.make(1.5); return (int)(T.sum(v, 10.0) * 10.0) + (int)v.y;',
        'crt': False,
        'ir': True,
    },
    'vectorcall-mixto': {
        'header': '''namespace T {
struct V4 { double a; double b; double c; double d; };
struct F3 { float x; float y; float z; };
struct P { int a; int b; };
struct Q { int a; int b; int c; };
API V4 __vectorcall make4(double s) noexcept;
API F3 __vectorcall make3(float s) noexcept;
API P __vectorcall pair(int a) noexcept;
API Q __vectorcall triple(int a) noexcept;
API double __vectorcall mix(double k, F3 f, V4 a, P p, Q q, V4 b, V4 c) noexcept;
}
''',
        'source': '''namespace T {
V4 __vectorcall make4(double s) noexcept { V4 r; r.a = s; r.b = s * 2.0; r.c = s * 3.0; r.d = s * 4.0; return r; }
F3 __vectorcall make3(float s) noexcept { F3 r; r.x = s; r.y = s * 2.0f; r.z = s * 4.0f; return r; }
P __vectorcall pair(int a) noexcept { P r; r.a = a; r.b = a + 1; return r; }
Q __vectorcall triple(int a) noexcept { Q r; r.a = a; r.b = a * 2; r.c = a * 3; return r; }
double __vectorcall mix(double k, F3 f, V4 a, P p, Q q, V4 b, V4 c) noexcept
{
    return k * 100.0 + (f.x + f.y + f.z) * 10.0 + a.a + a.d + b.b + c.c + p.a * p.b + q.a + q.c;
}
}
''',
        'cpp': 'T::V4 a = T::make4(1.0); T::F3 f = T::make3(0.5f); T::P p = T::pair(3); T::Q q = T::triple(2); '
               'return (int)T::mix(2.0, f, a, p, q, T::make4(2.0), T::make4(3.0)) + p.b * 1000 + q.c * 10000;',
        'ns': 'T.V4 a = T.make4(1.0); T.F3 f = T.make3(0.5f); T.P p = T.pair(3); T.Q q = T.triple(2); '
              'return (int)T.mix(2.0, f, a, p, q, T.make4(2.0), T.make4(3.0)) + p.b * 1000 + q.c * 10000;',
        'crt': False,
        'ir': True,
    },
    'vectorcall-vectores': {
        'header': '''namespace T {
typedef float f4 __attribute__((vector_size(16)));
typedef double d2 __attribute__((vector_size(16)));
struct W2 { f4 a; f4 b; };
typedef int i4 __attribute__((vector_size(16)));
struct D3 { d2 a; d2 b; d2 c; };
struct I2 { i4 v[2]; };
API W2 __vectorcall wmake(float s) noexcept;
API I2 __vectorcall imake(int s) noexcept;
API int __vectorcall isum(I2 a, W2 b, W2 c, double k) noexcept;
API float __vectorcall wsum(W2 w, float k) noexcept;
API D3 __vectorcall dmake(double s) noexcept;
API double __vectorcall dsum(D3 d) noexcept;
}
''',
        'source': '''namespace T {
W2 __vectorcall wmake(float s) noexcept
{
    W2 r;
    r.a = f4{s, s * 2.0f, s * 3.0f, s * 4.0f};
    r.b = f4{s + 1.0f, s + 2.0f, s + 3.0f, s + 4.0f};
    return r;
}
float __vectorcall wsum(W2 w, float k) noexcept { return (w.a[0] + w.a[3]) * k + w.b[1]; }
D3 __vectorcall dmake(double s) noexcept
{
    D3 r; r.a = d2{s, s * 2.0}; r.b = d2{s * 3.0, s * 4.0}; r.c = d2{s * 5.0, s * 6.0}; return r;
}
double __vectorcall dsum(D3 d) noexcept { return d.a[0] + d.b[1] + d.c[0]; }
I2 __vectorcall imake(int s) noexcept
{
    I2 r; r.v[0] = i4{s, s + 1, s + 2, s + 3}; r.v[1] = i4{s * 2, s * 3, s * 4, s * 5}; return r;
}
int __vectorcall isum(I2 a, W2 b, W2 c, double k) noexcept
{
    return a.v[0][1] + a.v[1][3] * 10 + (int)(b.a[2] + c.b[0]) + (int)k;
}
}
''',
        'cpp': 'T::W2 w = T::wmake(1.5f); T::I2 i = T::imake(3); return (int)(T::wsum(w, 2.0f) * 10.0f) + '
               '(int)w.b[3] + (int)T::dsum(T::dmake(2.0)) + T::isum(i, w, T::wmake(2.0f), 7.0) * 1000 + '
               'i.v[1][2] * 100000;',
        'ns': 'T.W2 w = T.wmake(1.5f); T.I2 i = T.imake(3); return (int)(T.wsum(w, 2.0f) * 10.0f) + '
              '(int)w.b[3] + (int)T.dsum(T.dmake(2.0)) + T.isum(i, w, T.wmake(2.0f), 7.0) * 1000 + '
              'i.v[1][2] * 100000;',
        'crt': False,
        'ir': True,
    },
    'cdecl-constructor': {
        'header': '''namespace T {
struct C8 {
    int x; int y;
    C8() = default;
    C8(int a, int b) noexcept : x(a), y(b) {}
};
API C8 makec(int a) noexcept;
API int usec(C8 c, int k) noexcept;
}
''',
        'source': '''namespace T {
C8 makec(int a) noexcept { return C8(a, a * 3); }
int usec(C8 c, int k) noexcept { return c.x * k + c.y; }
}
''',
        'cpp': 'T::C8 c = T::makec(5); return T::usec(c, 4) * 10 + c.y;',
        'ns': 'T.C8 c = T.makec(5); return T.usec(c, 4) * 10 + c.y;',
        'crt': False,
        'ir': True,
    },
    'vectorcall-constructor': {
        'header': '''namespace T {
struct C2 {
    double x; double y;
    C2() = default;
    C2(double a, double b) noexcept : x(a), y(b) {}
};
API C2 __vectorcall makec(double a) noexcept;
API double __vectorcall usec(C2 c, double k) noexcept;
}
''',
        'source': '''namespace T {
C2 __vectorcall makec(double a) noexcept { return C2(a, a * 3.0); }
double __vectorcall usec(C2 c, double k) noexcept { return c.x * k + c.y; }
}
''',
        'cpp': 'T::C2 c = T::makec(2.5); return (int)(T::usec(c, 4.0) * 10.0) + (int)c.y;',
        'ns': 'T.C2 c = T.makec(2.5); return (int)(T.usec(c, 4.0) * 10.0) + (int)c.y;',
        'crt': False,
        'ir': True,
    },
}


def run(label, command, cwd, expected=0, extra_env=None):
    result = subprocess.run(command, cwd=cwd, env={**env, **(extra_env or {})}, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def reference(dll, prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="' + prefix +
            'api.lib"' + runtime + ' />')


def project(folder, body, dll, references=None, library=False, name='T19Traslados', source=None):
    output = '<OutputType>Library</OutputType>' if library else ''
    (folder / 'Main.ns').write_text(source or ('int Main()\n{\n    ' + body + '\n}\n'), encoding='utf-8')
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" />' + (reference(dll) if references is None else references) +
        '</ItemGroup></Project>', encoding='utf-8')


def declarations(ir):
    """Shape of each T declaration: convention, result and per-parameter kind (inreg, sret, ptr or scalar type)."""
    # Forma de cada declaracion de T: convencion, resultado y tipo de cada parametro.
    found = {}
    for line in ir.splitlines():
        match = re.match(r'declare\s+(.*?)\s+@"?(\?[^"(]*@T@@[^"(]*)"?\((.*)\)', line)
        if not match:
            continue
        head, symbol, parameters = match.groups()
        words = [w for w in head.split() if w not in ('dso_local', 'dllimport', 'noundef', 'nonnull', 'zeroext',
                                                      'signext', 'inreg') and not w.startswith(('dereferenceable', 'align'))]
        result = 'agg' if words and words[-1].startswith(('{', '%', '}')) or '{' in head else (words[-1] if words else '')
        shapes = []
        for parameter in [p.strip() for p in re.split(r',\s*(?![^{]*})', parameters)] if parameters.strip() else []:
            kind = 'sret' if 'sret' in parameter else 'inreg' if ' inreg' in parameter else \
                'ptr' if parameter.startswith('ptr') else 'agg' if parameter.startswith(('{', '%')) else parameter.split()[0]
            shapes.append(kind)
        found[symbol] = ('x86_vectorcallcc' in head, result, tuple(shapes))
    return found


def compare(tag, clang_ir, nsc_ir):
    clang, nsc = declarations(clang_ir), declarations(nsc_ir)
    if not clang:
        raise SystemExit(f'{tag}: el IR de Clang no declara funciones de T; evidencia: {root}')
    for symbol, shape in clang.items():
        if nsc.get(symbol) != shape:
            raise SystemExit(f'{tag}: {symbol} tiene {nsc.get(symbol)} en NSC y {shape} en Clang; evidencia: {root}')
    return len(clang)


def nsc_build(tag, folder, opt, extra_args=(), extra_env=None):
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt), *extra_args]
    run(tag + '-nsc', command + ['-o', 'app.exe'], folder, extra_env=extra_env)
    return run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode


def precompiled(name, case, folder, opt, control):
    """The same body from a precompiled NativeSharp generic, with and without the C++ header for the consumer."""
    # El mismo cuerpo desde una plantilla NativeSharp precompilada, con y sin el encabezado C++ en el consumidor.
    producer = folder / 'producer'
    producer.mkdir()
    project(producer, '', args.dll, reference(args.dll, '../'), True, 'T19TrasladosProducer',
            'namespace T19P { public class Bridge { public static int Run<U>(U unused) { ' + case['ns'] + ' } } }\n')
    run(f'{name}-O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'App.nsproj', '--cpp-clang-root=' +
        args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
    for visibility in ('con-encabezado', 'sin-encabezado'):
        consumer = folder / visibility
        consumer.mkdir()
        references = '<LibraryReference Include="../producer/Producer.lib" />'
        if visibility == 'con-encabezado':
            references += reference(args.dll, '../')
        else:
            (folder / 'api.h').rename(folder / 'api.h.oculto')
        if args.dll:
            (consumer / 'api.dll').write_bytes((folder / 'api.dll').read_bytes())
        project(consumer, 'return T19P.Bridge.Run<int>(0);', args.dll, references)
        try:
            result = nsc_build(f'{name}-O{opt}-{visibility}', consumer, opt)
        finally:
            if visibility == 'sin-encabezado':
                (folder / 'api.h.oculto').rename(folder / 'api.h')
        if result != control:
            raise SystemExit(f'{name} O{opt} {visibility}: NativeSharp devolvio {result}, Clang {control}; '
                             f'evidencia: {root}')


def tls_dll_rejected(name, case):
    # The Microsoft ABI does not allow dllimport thread-local data: Clang rejects the header too.
    # El ABI de Microsoft no permite datos thread_local dllimport: Clang tambien rechaza el encabezado.
    folder = root / (name + '-dll')
    folder.mkdir()
    exported = case['header'].replace('static constinit thread_local', 'API static constinit thread_local')
    header = '#define T19_IMPORT 1\n' + API + exported
    (folder / 'api.h').write_text(header, encoding='utf-8')
    (folder / 'control.cpp').write_text('#include "api.h"\nint main() { return T::S::counter; }\n', encoding='utf-8')
    clang = run(name + '-dll-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], folder, expected=1).stdout
    if 'thread' not in clang or 'dllimport' not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(name + ': Clang no rechaza el thread_local dllimport; evidencia: ' + str(root))
    print(name + ': Clang rechaza un thread_local dllimport (ABI de Microsoft); no aplica en DLL', flush=True)


def check(name, case):
    if args.dll and case['crt']:
        tls_dll_rejected(name, case)
        return
    library = root / (name + '-lib')
    library.mkdir()
    (library / 'api.h').write_text(API + case['header'], encoding='utf-8')
    # The DLL is linked without the CRT: it defines the floating-point marker itself, as the control does.
    # La DLL se enlaza sin el CRT: define ella misma el marcador de punto flotante, como el control.
    dll_marker = 'extern "C" int _fltused = 0;\n' if args.dll and not case['crt'] else ''
    (library / 'api.cpp').write_text('#include "api.h"\n' + dll_marker + case['source'], encoding='utf-8')
    run(name + '-api', [*CXX, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'],
        library)
    if args.dll:
        run(name + '-api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                '/implib:api.lib', 'api.obj'], library)
        header = '#define T19_IMPORT 1\n' + API + case['header']
    else:
        run(name + '-api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
        header = API + case['header']
    for opt in range(4):
        folder = root / f'{name}-O{opt}'
        folder.mkdir()
        (folder / 'api.h').write_text(header, encoding='utf-8')
        for item in ('api.lib', *(('api.dll',) if args.dll else ())):
            (folder / item).write_bytes((library / item).read_bytes())
        # Without the CRT the control defines the floating-point marker itself; with it, libcmt.lib does.
        # Sin el CRT el control define el marcador de punto flotante; con el CRT lo define libcmt.lib.
        marker = '' if case['crt'] else 'extern "C" __declspec(selectany) int _fltused = 0;\n'
        (folder / 'control.cpp').write_text('#include "api.h"\n' + marker + 'int main() { ' + case['cpp'] + ' }\n',
                                            encoding='utf-8')
        if case['crt']:
            run(f'{name}-O{opt}-control', [*CXX, '-O' + str(opt), '-c', 'control.cpp', '-o', 'control.obj'], folder)
            run(f'{name}-O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe',
                                                'control.obj', 'api.lib', *[str(p) for p in CRT]], folder)
        else:
            run(f'{name}-O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld',
                                           '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'], folder)
        control = run(f'{name}-O{opt}-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode
        project(folder, case['ns'], args.dll)
        extra_args, extra = [], {}
        if case['crt']:
            extra_args.append('--crt-startup')
            for item in CRT:
                extra_args += ['--link-lib', str(item)]
            extra = {'NSC_LINKER': 'lld-link.exe'}
        result = nsc_build(f'{name}-O{opt}', folder, opt, extra_args, extra)
        if result != control:
            raise SystemExit(f'{name} O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
        detail = ''
        if case.get('ir'):
            clang_ir = run(f'{name}-O{opt}-control-ir', [*CXX, '-O' + str(opt), '-S', '-emit-llvm', 'control.cpp',
                                                          '-o', '-'], folder).stdout
            nsc_ir = run(f'{name}-O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                   '--cpp-clang-root=' + args.clang_root], folder).stdout
            detail = f'; {compare(f"{name}-O{opt}", clang_ir, nsc_ir)} declaraciones con la forma de Clang'
            if case.get('pre', True):
                precompiled(name, case, folder, opt, control)
                detail += '; plantilla precompilada con y sin encabezado'
        print(f'{name} O{opt}: NativeSharp y Clang devuelven {result}{detail}', flush=True)


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for name, case in CASES.items():
    if args.caso and args.caso != name:
        continue
    check(name, case)
print('OK: traslados de T19-036 frente a Clang; evidencia:', root)
