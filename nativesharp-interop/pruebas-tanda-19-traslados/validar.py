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
- `excepcion-adaptador` y `excepcion-especializacion` (con el CRT): una funcion C++ con `try`/`catch` llama a un
  callback NativeSharp que llama a un metodo virtual por un adaptador de Clang, o al metodo estatico inline de la
  especializacion `Box<int>`; la excepcion C++ atraviesa el adaptador o la especializacion y los frames de
  NativeSharp. Con --dll la biblioteca enlaza su propio runtime, como en la tanda 15. Solo directo: los metodos de
  instancia y los usos de plantillas C++ en una plantilla precompilada son T19-031 y T19-016.
- `tls-bajo-demanda`: globales de namespace (escalar y record) y un campo estatico `thread_local` con inicializacion
  dinamica (sin `constinit`), leidos y escritos desde NativeSharp (estatica, con el CRT), tambien desde una
  plantilla precompilada con y sin encabezado. Como Clang, cada acceso de NSC comprueba `__tls_guard` y llama a
  `__dyn_tls_on_demand_init` del CRT; los casos `constinit` no llevan la guarda. Con --dll, como `tls-campo`.
- `tls-global`: globales de namespace `constinit thread_local` (escalar y record) leidas y escritas desde NativeSharp
  (estatica, con el CRT), tambien desde una plantilla precompilada con y sin encabezado; con --dll, como `tls-campo`.
- `tls-campo`: campo estatico `constinit thread_local` leido y escrito desde NativeSharp (estatica, con el CRT),
  tambien desde una plantilla precompilada con y sin encabezado. Con
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
- `vectorcall-puntero`: punteros a funcion C++ `__vectorcall` con records por valor: un agregado homogeneo de tres
  `float`, un record de 8 bytes y un record `{double, double}` con constructor (resultado por puntero oculto),
  llamados desde NativeSharp y como callbacks NativeSharp que llama la biblioteca; las llamadas por puntero y los
  callbacks tienen la forma de Clang.
- `vector-suelto`: vectores de Clang de 128 bits (`float` x4, `int` x4, `long long` x2) por valor como parametro y
  como resultado de funciones `cdecl`, y `float` x4 con `__vectorcall`, con sus portadores de
  NativeSharp.Core.Numerics (Float32x4, Int32x4, Int64x2); las declaraciones tienen la forma de las de Clang.
- `generico-nativo` (T19-025): structs genericos NativeSharp (`Box<int>`, `Box<long>`) y un metodo de una clase
  generica instanciada con un record C++ (`Shelf<T.Rec>.Pick`) junto a la referencia C++; tambien desde una
  plantilla precompilada que publica los genericos, con y sin encabezado. La observacion de records C++ copia los
  arboles ya reescritos por la fase generica. Un struct NativeSharp con un campo C++ sigue en T19-015.
- `vector-solo-cdecl`: un encabezado con vectores solo en funciones `cdecl` (sin otra convencion que pida el
  generador de codigo de Clang).
- `vector-suelto-double`: `double` x2 (`__m128d`, portador Float64x2) con `cdecl` y `__vectorcall`, y vectores
  enteros de 128 bits con `__vectorcall`.
- `vector-suelto-avx` (nivel de CPU `x86-64-v3`, proyecto `CpuLevel` y Clang `-march`): vectores de 256 bits
  (`float` x8, `double` x4, `int` x8, `long long` x4) con `cdecl` y `float` x8 y `double` x4 con `__vectorcall`;
  tambien comprueba los rechazos sin el nivel de CPU (directo y desde la plantilla precompilada sin encabezado).
- `vectorcall-avx` (`x86-64-v3`): agregados `__vectorcall` de vectores de 256 bits.
- `vectorcall-avx512` (`x86-64-v4`): agregados y vectores de 512 bits; solo compila, enlaza y compara la forma de
  las declaraciones con Clang, porque la CPU de este equipo no tiene AVX-512.
- `cdecl-constructor`: un record de 8 bytes con un constructor declarado (T12-001): como parametro viaja en un
  entero y como resultado por un puntero oculto, tambien desde una plantilla precompilada, que antes lo devolvia en
  un registro.
- `vectorcall-constructor`: un record `{double, double}` con un constructor declarado (el predeterminado sigue
  trivial): como parametro viaja en registros XMM y como resultado por un puntero oculto, porque el ABI de Microsoft
  no lo considera un agregado.

- `nombres-using` (T19-005): `using T::x;` de una variable, una funcion y un tipo en el namespace global, `using
  namespace T;` dentro de otro namespace (tipos y funciones), una global referencia (`int& alias`) y un miembro static
  referencia, leidos y escritos desde NativeSharp a traves del referente, como Clang; tambien desde una plantilla
  precompilada con y sin encabezado.
- `alias-plantilla` (T19-005): plantillas alias de una plantilla de clase (en el mismo namespace y en otro) y de un
  escalar (`Same<int>`), usadas como tipo y para llamar a un metodo estatico. Solo directo (T19-016).
- `enums-y-anidados` (T19-005): enumeradores de un enum sin `class` por el record (`R.Green`), enums sin nombre de
  namespace y de record, un campo de un enum sin nombre, `typedef enum { } Kind`, una global de un enum de
  namespace; tambien desde una plantilla precompilada con y sin encabezado; y los rechazos con el
  diagnostico de acceso de un record anidado privado, un enum anidado protegido y un enumerador privado por el record,
  como Clang los rechaza.

- `plantilla-por-using` (T19-005): una plantilla de clase y una de funcion nombradas por `using T::Box;`, por
  `using namespace T;` dentro de otro namespace y por un alias de namespace. Solo directo (T19-016).
- `sobrecargas-metodos` (T19-006): metodos C++ estaticos `g(int)`/`g(long)` y de instancia `f(int)`/`f(double)` y
  `e(int)`/`e(long)` elegidos como Clang (con un `float`, la promocion a `double`); `m(int, long)` frente a
  `m(long, int)` con dos int se rechaza por ambigua, como en Clang.
- `constructores` (T19-006): constructores C++ `V(int, int = 2)`, `V(long)` y `V(double, int = base)` elegidos y
  completados como Clang; una llamada sin constructor que la acepte lista sus alternativas. Solo directo: los objetos
  C++ con constructores en una plantilla precompilada siguen en T19-016.
- `predeterminados` (T19-006): argumentos predeterminados de enum, de record, de una global y de una llamada en
  funciones, y predeterminados constantes y no constantes en metodos estaticos y de instancia. Clang los evalua en
  cada llamada por un adaptador por cada numero de argumentos escritos; tambien desde una plantilla precompilada
  con y sin encabezado.

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
# EN: The standard Core is consumed as the library built by the external SDK (compiler first, then libraries),
#     one package per optimization level, under NSC_STDLIB_PACKAGE_ROOT (default: build-clang/packages, where
#     NSC Builder installs them).
# ES: El Core estandar se consume como la biblioteca construida por el SDK externo (primero el compilador, despues
#     las bibliotecas), un paquete por nivel de optimizacion, bajo NSC_STDLIB_PACKAGE_ROOT (por defecto:
#     build-clang/packages, donde los instala NSC Builder).
STDLIB_PACKAGE_ROOT = Path(os.environ.get('NSC_STDLIB_PACKAGE_ROOT',
                                          Path(__file__).resolve().parents[2] / 'build-clang' / 'packages'))


# EN: Marker of the Core reference in the case tables and the level being compiled.
# ES: Marca de la referencia a Core en las tablas de casos y nivel que se compila.
CORE_TOKEN = '@NATIVESHARP_CORE@'
CORE_LEVEL = [0]


def core_reference(opt):
    found = sorted(STDLIB_PACKAGE_ROOT.glob(f'NativeSharp.Core/*/windows-x64/O{opt}/NativeSharpCore.lib'))
    if not found:
        raise SystemExit(f'NativeSharp.Core O{opt} package not found under {STDLIB_PACKAGE_ROOT}; build the external '
                         'libraries first or set NSC_STDLIB_PACKAGE_ROOT')
    return '<LibraryReference Include="' + found[-1].as_posix() + '" />'

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


def cxx(case):
    """Clang with the case's CPU level (`-march`), as the NativeSharp project's CpuLevel."""
    # Clang con el nivel de CPU del caso (`-march`), como el CpuLevel del proyecto NativeSharp.
    return CXX + (['-march=' + case['cpu']] if case.get('cpu') else [])
API = '''#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
CASES = {
    'plantilla-por-using': {
        'header': '''namespace T {
template <class X> struct Box { X v; static X twice(X a) noexcept { return a + a; } };
template <class X> X thrice(X a) noexcept { return a * 3; }
}
using T::Box;
using T::thrice;
namespace U { using namespace T; }
namespace A = T;
API int touch() noexcept;
''',
        'source': 'int touch() noexcept { return 1; }\n',
        'cpp': 'return Box<int>::twice(4) + U::Box<int>::twice(10) + A::Box<int>::twice(1) + thrice<int>(3) + '
               'U::thrice<int>(5) + touch();',
        'ns': 'return Box<int>.twice(4) + U.Box<int>.twice(10) + A.Box<int>.twice(1) + thrice<int>(3) + '
              'U.thrice<int>(5) + touch();',
        'crt': False,
        'pre': False,
    },
    'sobrecargas-metodos': {
        'header': '''namespace T {
struct S {
    int k;
    API static int g(int v) noexcept;
    API static int g(long v) noexcept;
    API int f(int v) const noexcept;
    API int f(double v) const noexcept;
    API int e(int v) const noexcept;
    API int e(long v) const noexcept;
    API static int m(int a, long b) noexcept;
    API static int m(long a, int b) noexcept;
};
}
''',
        'source': '''namespace T {
int S::g(int v) noexcept { return v + 1; }
int S::g(long v) noexcept { return (int)v + 100; }
int S::f(int v) const noexcept { return v + k; }
int S::f(double v) const noexcept { return (int)(v * 10) + k; }
int S::e(int v) const noexcept { return v + k; }
int S::e(long v) const noexcept { return (int)v + 50; }
int S::m(int a, long b) noexcept { return 1; }
int S::m(long a, int b) noexcept { return 2; }
}
''',
        'cpp': 'T::S s; s.k = 1; return s.e(2) * 10000 + T::S::g(5) * 1000 + s.f(3) * 100 + s.f(2.5) + s.f(1.5f);',
        'ns': 'T.S s; s.k = 1; return s.e(2) * 10000 + T.S.g(5) * 1000 + s.f(3) * 100 + s.f(2.5) + s.f(1.5f);',
        'crt': False,
        'pre': True,
        'rechazos': [
            ('return T.S.m(5, 5);', "ambiguous"),
            ('return T.S.m(5, 5);', 'candidate: static int T::S::m(int, long)'),
            ('return T.S.g();', 'candidate: static int T::S::g(long) takes 1 argument(s)'),
            ('T.S s; s.k = 1; return s.f();', 'candidate: int T::S::f(double) const takes 1 argument(s)'),
            ('T.S s; s.k = 1; return s.f(s);', 'candidate: int T::S::f(double) const: argument 1'),
        ],
    },
    'predeterminados': {
        'header': '''namespace T {
enum class Color { Red = 1, Green = 2 };
struct P { int x; int y; };
API extern int base;
API int counter() noexcept;
API int pick(Color c = Color::Green) noexcept;
API int sum(P p = P{1, 2}) noexcept;
API int add(int v = base, int w = counter()) noexcept;
struct S {
    int k;
    API int m(int v = 3) const noexcept;
    API int n(int v = base) const noexcept;
    API static int s(int v = 5, int w = 6) noexcept;
    API static int t(Color c = Color::Red) noexcept;
};
}
''',
        'source': '''namespace T {
int base = 4;
static int calls = 0;
int counter() noexcept { return ++calls; }
int pick(Color c) noexcept { return (int)c * 10; }
int sum(P p) noexcept { return p.x * 10 + p.y; }
int add(int v, int w) noexcept { return v * 10 + w; }
int S::m(int v) const noexcept { return v + k; }
int S::n(int v) const noexcept { return v * 2 + k; }
int S::s(int v, int w) noexcept { return v * w; }
int S::t(Color c) noexcept { return (int)c + 40; }
}
''',
        'cpp': 'T::S x; x.k = 1; int a = T::add(); T::base = 7; '
               'return T::pick() + T::pick(T::Color::Red) + T::sum() * 2 + a * 100 + T::add() + T::add(3) + '
               'x.m() * 1000 + x.n() + T::S::s() + T::S::s(2) + T::S::t();',
        'ns': 'T.S x; x.k = 1; int a = T.add(); T.base = 7; '
              'return T.pick() + T.pick(T.Color.Red) + T.sum() * 2 + a * 100 + T.add() + T.add(3) + '
              'x.m() * 1000 + x.n() + T.S.s() + T.S.s(2) + T.S.t();',
        'crt': False,
        'pre': True,
    },
    'constructores': {
        'header': '''namespace T {
API extern int base;
struct V {
    int v;
    API V(int a, int b = 2) noexcept;
    API V(long a) noexcept;
    API V(double a, int b = base) noexcept;
};
}
''',
        'source': '''namespace T {
int base = 7;
V::V(int a, int b) noexcept : v(a * 10 + b) {}
V::V(long a) noexcept : v((int)a + 500) {}
V::V(double a, int b) noexcept : v((int)(a * 100) + b) {}
}
''',
        'cpp': 'T::V x(4); T::V y(4, 3); T::V z(2.5); T::base = 1; T::V w(1.5); '
               'return x.v + y.v * 100 + z.v * 10000 + w.v * 1000000;',
        'ns': 'T.V x = new T.V(4); T.V y = new T.V(4, 3); T.V z = new T.V(2.5); T.base = 1; T.V w = new T.V(1.5); '
              'return x.v + y.v * 100 + z.v * 10000 + w.v * 1000000;',
        'crt': False,
        # T19-016: a precompiled template does not carry C++ objects with constructors yet.
        # T19-016: una plantilla precompilada aun no lleva objetos C++ con constructores.
        'pre': False,
        'rechazos': [
            ('T.V q = new T.V(1, 2, 3); return q.v;', 'candidate: T::V::V(int, int)'),
        ],
    },
    'nombres-using': {
        'header': '''namespace T {
API extern int counter;
API int twice(int v) noexcept;
struct Rec { int a; int b; };
API Rec make(int v) noexcept;
API extern int& alias;
struct S { API static int& r; API static int read() noexcept; };
}
using T::counter;
using T::twice;
using T::Rec;
namespace W { using namespace T; }
''',
        'source': '''namespace T {
int counter = 40;
int twice(int v) noexcept { return v * 2; }
Rec make(int v) noexcept { return Rec{v, v + 1}; }
int& alias = counter;
static int store = 3;
int& S::r = store;
int S::read() noexcept { return store; }
}
''',
        'cpp': 'counter = counter + 1; Rec r = T::make(5); W::Rec q = W::make(1); T::alias = T::alias + 2; '
               'T::S::r = T::S::r + 4; return twice(counter) * 100 + r.a * 10 + q.b + T::alias + T::S::read();',
        'ns': 'counter = counter + 1; Rec r = T.make(5); W.Rec q = W.make(1); T.alias = T.alias + 2; '
              'T.S.r = T.S.r + 4; return twice(counter) * 100 + r.a * 10 + q.b + T.alias + T.S.read();',
        'crt': False,
        'pre': True,
    },
    'alias-plantilla': {
        'header': '''namespace T {
template <class X> struct Box { X v; static X twice(X a) noexcept { return a + a; } };
template <class X> using Alias = Box<X>;
template <class X> using Same = X;
API int seen(Box<int> box) noexcept;
}
namespace U { template <class X> using Box2 = T::Box<X>; }
''',
        'source': 'namespace T { int seen(Box<int> box) noexcept { return box.v * 3; } }\n',
        'cpp': 'T::Same<int> v = 20; T::Alias<int> b; b.v = 7; '
               'return T::Alias<int>::twice(v) + (int)U::Box2<long long>::twice(1) + T::seen(b);',
        'ns': 'T.Same<int> v = 20; T.Alias<int> b; b.v = 7; '
              'return T.Alias<int>.twice(v) + (int)U.Box2<long>.twice(1) + T.seen(b);',
        'crt': False,
        'pre': False,
    },
    'enums-y-anidados': {
        'header': '''namespace T {
struct R { enum Color { Red = 3, Green = 5 }; enum { K = 7 }; int c; };
enum { Alpha = 11 };
enum class Level { Low = 1, High = 9 };
API extern Level current;
API int raise() noexcept;
API int pick(R::Color c) noexcept;
class Outer { struct Hidden { int x; }; enum Hid { X = 4 }; public: API static int use() noexcept; };
class Base { protected: enum class Mode { A = 1, B = 2 }; public: API static int b() noexcept; };
struct S { enum { A = 1, B = 2 } mode; int v; };
API S make() noexcept;
typedef enum { P = 3, Q = 8 } Kind;
API int weight(Kind k) noexcept;
}
''',
        'source': '''namespace T {
Level current = Level::Low;
int raise() noexcept { current = Level::High; return (int)current; }
int pick(R::Color c) noexcept { return (int)c * 2; }
int Outer::use() noexcept { Hidden h{5}; return h.x + X; }
int Base::b() noexcept { return (int)Mode::B; }
S make() noexcept { S s; s.mode = S::B; s.v = 5; return s; }
int weight(Kind k) noexcept { return (int)k + 1; }
}
''',
        'cpp': 'int a = (int)T::current; int b = T::raise(); T::S s = T::make(); '
               'return T::pick(T::R::Green) * 1000 + T::R::K * 100 + T::Alpha + a * 10 + b + (int)T::current + '
               'T::Outer::use() + T::Base::b() + (int)s.mode * 10000 + s.v + T::S::A + T::weight(T::Q) + (int)T::P;',
        'ns': 'int a = (int)T.current; int b = T.raise(); T.S s = T.make(); '
              'return T.pick(T.R.Green) * 1000 + T.R.K * 100 + T.Alpha + a * 10 + b + (int)T.current + '
              'T.Outer.use() + T.Base.b() + (int)s.mode * 10000 + s.v + T.S.A + T.weight(T.Q) + (int)T.P;',
        'crt': False,
        'pre': True,
        'rechazos': [
            ('T.Outer.Hidden h; h.x = 1; return h.x;', "nested type 'T.Outer+Hidden' is private"),
            ('return (int)T.Base.Mode.B;', "nested type 'T.Base+Mode' is protected"),
            ('return (int)T.Outer.X;', "member 'X' is not accessible"),
        ],
    },
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
        'dll': 'tls-rechazado',
        'pre': True,
        'tls_guard': False,
    },
    'tls-global': {
        'header': '''namespace T {
extern constinit thread_local int depth;
struct P { int x; int y; };
extern constinit thread_local P spot;
API int deepen() noexcept;
}
''',
        'source': '''namespace T {
constinit thread_local int depth = 3;
constinit thread_local P spot = {4, 6};
int deepen() noexcept { depth = depth * 2 + spot.x; return depth; }
}
''',
        'cpp': 'int a = T::depth; T::depth = 10; T::spot.x = 7; int b = T::deepen(); return a * 1000 + b * 10 + T::spot.y;',
        'ns': 'int a = T.depth; T.depth = 10; T.spot.x = 7; int b = T.deepen(); return a * 1000 + b * 10 + T.spot.y;',
        'crt': True,
        'dll': 'tls-rechazado',
        'dll_tls': ('extern constinit thread_local int depth', 'T::depth'),
        'pre': True,
        'tls_guard': False,
    },
    'tls-bajo-demanda': {
        'header': '''namespace T {
extern thread_local int depth;
struct P { int x; int y; };
extern thread_local P spot;
struct S { static thread_local int counter; };
API int seed() noexcept;
API int deepen() noexcept;
}
''',
        'source': '''namespace T {
int seed() noexcept { return 7; }
thread_local int depth = seed();
thread_local P spot = {seed(), 6};
thread_local int S::counter = seed() * 2;
int deepen() noexcept { depth = depth * 2 + S::counter; return depth; }
}
''',
        'cpp': 'int a = T::depth; T::S::counter = 3; T::spot.x = T::spot.x + 1; int b = T::deepen(); '
               'return a * 1000 + b * 10 + T::spot.x;',
        'ns': 'int a = T.depth; T.S.counter = 3; T.spot.x = T.spot.x + 1; int b = T.deepen(); '
              'return a * 1000 + b * 10 + T.spot.x;',
        'crt': True,
        'dll': 'tls-rechazado',
        'dll_tls': ('extern thread_local int depth', 'T::depth'),
        'pre': True,
        'tls_guard': True,
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
    'vectorcall-puntero': {
        'header': '''namespace T {
struct H3 { float a; float b; float c; };
struct P2 { int x; int y; };
using Fh = H3 (__vectorcall *)(H3, float, P2) noexcept;
API Fh pick(int which) noexcept;
API float __vectorcall callh(Fh f, float k) noexcept;
struct C2 { double x; double y; C2() = default; C2(double a, double b) noexcept : x(a), y(b) {} };
using Fc = C2 (__vectorcall *)(C2, double) noexcept;
API Fc pickc() noexcept;
API double __vectorcall callc(Fc f) noexcept;
}
''',
        'source': '''namespace T {
static H3 __vectorcall scale(H3 h, float k, P2 p) noexcept { return H3{h.a * k + p.x, h.b * k, h.c * k + p.y}; }
static H3 __vectorcall shift(H3 h, float k, P2 p) noexcept { return H3{h.a + k, h.b + p.x, h.c + p.y}; }
Fh pick(int which) noexcept { return which ? shift : scale; }
float __vectorcall callh(Fh f, float k) noexcept
{
    H3 r = f(H3{1.0f, 2.0f, 3.0f}, k, P2{4, 5});
    return r.a + r.b * 10.0f + r.c * 100.0f;
}
static C2 __vectorcall twice(C2 c, double k) noexcept { return C2(c.x * k, c.y + k); }
Fc pickc() noexcept { return twice; }
double __vectorcall callc(Fc f) noexcept { C2 r = f(C2(1.5, 2.5), 2.0); return r.x * 100.0 + r.y * 10.0; }
}
''',
        'cpp_prelude': 'static T::H3 __vectorcall mine(T::H3 h, float k, T::P2 p) noexcept '
                       '{ return T::H3{h.a - k, h.b * 2.0f + p.y, h.c + p.x}; }\n'
                       'static T::C2 __vectorcall minec(T::C2 v, double k) noexcept { return T::C2(v.x + k, v.y * k); }\n',
        'cpp': 'T::Fh f = T::pick(0); T::H3 r = f(T::H3{1.5f, 2.0f, 2.5f}, 2.0f, T::P2{3, 4}); '
               'return (int)(r.a * 10.0f + r.b * 100.0f + r.c * 1000.0f) + (int)T::callh(T::pick(1), 3.0f) * 100000 + '
               '(int)T::callh(mine, 1.0f) * 1000000 + (int)T::callc(T::pickc()) * 7 + (int)T::callc(minec) * 3 + '
               '[] { T::Fc g = T::pickc(); T::C2 c = g(T::C2(1.5, 2.5), 2.0); return (int)(c.x * 10.0 + c.y); }();',
        'ns_prelude': 'T.H3 Mine(T.H3 h, float k, T.P2 p) { T.H3 r = h; r.a = h.a - k; r.b = h.b * 2.0f + p.y; '
                      'r.c = h.c + p.x; return r; }\n'
                      'T.C2 MineC(T.C2 v, double k) { T.C2 r = new T.C2(v.x + k, v.y * k); return r; }\n'
                      'T.Fc MakeMine() { T.Fc mc = (T.C2 v, double k) => MineC(v, k); return mc; }\n'
                      'int Twice() { T.Fc g = T.pickc(); T.C2 a = new T.C2(1.5, 2.5); T.C2 c = g(a, 2.0); return (int)(c.x * 10.0 + c.y); }\n',
        'ns': 'T.Fh f = T.pick(0); T.H3 h = new T.H3(); h.a = 1.5f; h.b = 2.0f; h.c = 2.5f; T.P2 p = new T.P2(); '
              'p.x = 3; p.y = 4; T.H3 r = f(h, 2.0f, p); T.Fh m = (T.H3 v, float k, T.P2 q) => Mine(v, k, q); '
              'return (int)(r.a * 10.0f + r.b * 100.0f + r.c * 1000.0f) + (int)T.callh(T.pick(1), 3.0f) * 100000 + '
              '(int)T.callh(m, 1.0f) * 1000000 + (int)T.callc(T.pickc()) * 7 + (int)T.callc(MakeMine()) * 3 + Twice();',
        'ptr_ir': True,
        'crt': False,
    },
    'cdecl-puntero': {
        'header': '''namespace T {
struct K8 { int x; int y; K8() = default; K8(int a, int b) noexcept : x(a), y(b) {} };
struct Q4 { int a; int b; int c; int d; };
using Fk = K8 (*)(K8, Q4, int) noexcept;
API Fk pickk() noexcept;
API int callk(Fk f) noexcept;
}
''',
        'source': '''namespace T {
static K8 mixk(K8 k, Q4 q, int s) noexcept { return K8(k.x * s + q.a, k.y + q.d); }
Fk pickk() noexcept { return mixk; }
int callk(Fk f) noexcept { K8 r = f(K8(2, 3), Q4{1, 2, 3, 4}, 5); return r.x * 100 + r.y; }
}
''',
        'cpp_prelude': 'static T::K8 minek(T::K8 k, T::Q4 q, int s) noexcept { return T::K8(k.x + s, k.y * q.b); }\n',
        'cpp': 'T::Fk f = T::pickk(); T::Q4 q{7, 8, 9, 10}; T::K8 r = f(T::K8(1, 2), q, 3); '
               'return r.x * 1000 + r.y * 10 + T::callk(T::pickk()) * 100000 + T::callk(minek) * 3;',
        'ns_prelude': 'T.K8 MineK(T.K8 k, T.Q4 q, int s) { T.K8 r = new T.K8(k.x + s, k.y * q.b); return r; }\n'
                      'T.Fk MakeK() { T.Fk m = (T.K8 k, T.Q4 q, int s) => MineK(k, q, s); return m; }\n',
        'ns': 'T.Fk f = T.pickk(); T.Q4 q = new T.Q4(); q.a = 7; q.b = 8; q.c = 9; q.d = 10; '
              'T.K8 k = new T.K8(1, 2); T.K8 r = f(k, q, 3); '
              'return r.x * 1000 + r.y * 10 + T.callk(T.pickk()) * 100000 + T.callk(MakeK()) * 3;',
        'crt': False,
        'ptr_ir': True,
        'ptr_cc': '',
    },
    'vector-suelto': {
        'header': '''namespace T {
typedef float f4 __attribute__((vector_size(16)));
typedef int i4 __attribute__((vector_size(16)));
typedef long long l2 __attribute__((vector_size(16)));
API f4 scale(f4 v, float k) noexcept;
API float fsum(f4 v) noexcept;
API i4 iadd(i4 a, int k, i4 b) noexcept;
API int isum(i4 v) noexcept;
API l2 lmake(long long a, long long b) noexcept;
API long long lsum(l2 v, int k) noexcept;
API f4 __vectorcall vscale(f4 v, double k) noexcept;
}
''',
        'source': '''namespace T {
f4 scale(f4 v, float k) noexcept { return v * k + f4{1.0f, 0.0f, 0.0f, 0.5f}; }
float fsum(f4 v) noexcept { return v[0] + v[1] * 10.0f + v[2] * 100.0f + v[3] * 1000.0f; }
i4 iadd(i4 a, int k, i4 b) noexcept { return a * k + b; }
int isum(i4 v) noexcept { return v[0] + v[1] * 10 + v[2] * 100 + v[3] * 1000; }
l2 lmake(long long a, long long b) noexcept { return l2{a * 3, b + 7}; }
long long lsum(l2 v, int k) noexcept { return v[0] * k + v[1]; }
f4 __vectorcall vscale(f4 v, double k) noexcept { return v * (float)k; }
}
''',
        'cpp': 'T::f4 v = T::scale(T::f4{1.0f, 2.0f, 3.0f, 4.0f}, 2.0f); T::i4 a = T::i4{1, 2, 3, 4}; '
               'T::i4 b = T::iadd(a, 3, T::i4{5, 6, 7, 8}); T::l2 l = T::lmake(4, 5); '
               'return (int)T::fsum(v) + T::isum(b) * 3 + (int)T::lsum(l, 2) * 7 + (int)T::fsum(T::vscale(v, 0.5)) * 11 + '
               '(int)v[3] + b[2] + (int)l[1];',
        'ns_prelude': 'using NativeSharp.Core.Numerics;\n',
        'ns_refs': CORE_TOKEN,
        'ns': 'Float32x4 a = new Float32x4(1.0f, 2.0f, 3.0f, 4.0f); Float32x4 v = T.scale(a, 2.0f); '
              'Int32x4 i; i.X = 1; i.Y = 2; i.Z = 3; i.W = 4; Int32x4 j; j.X = 5; j.Y = 6; j.Z = 7; j.W = 8; '
              'Int32x4 sum4 = T.iadd(i, 3, j); Int64x2 pair = T.lmake(4, 5); '
              'return (int)T.fsum(v) + T.isum(sum4) * 3 + (int)T.lsum(pair, 2) * 7 + (int)T.fsum(T.vscale(v, 0.5)) * 11 + '
              '(int)v.W + sum4.Z + (int)pair.Y;',
        'crt': False,
        'ir': True,
        'vector_ir': True,
    },
    'generico-nativo': {
        'header': '''namespace T {
struct Rec { int a; int b; };
API Rec make(int v) noexcept;
API int read(Rec r) noexcept;
}
''',
        'source': '''namespace T {
Rec make(int v) noexcept { return Rec{v, v * 2}; }
int read(Rec r) noexcept { return r.a * 10 + r.b; }
}
''',
        'cpp': 'T::Rec r = T::make(3); T::Rec s = T::make(1); return T::read(r) * 100 + 4 * 10 + 5 + T::read(s) * 1000;',
        'ns_prelude': 'public struct Box<U> { public U value; public int tag; }\n'
                      'public class Shelf<U> { public static U Pick(U a, U b, bool first) { return first ? a : b; } }\n',
        'pre_prelude': True,
        'ns': 'T.Rec r = T.make(3); Box<int> b; b.value = T.read(r); b.tag = 4; Box<long> n; n.value = 5L; n.tag = 0; '
              'T.Rec s = Shelf<T.Rec>.Pick(T.make(1), r, true); '
              'return b.value * 100 + b.tag * 10 + (int)n.value + T.read(s) * 1000;',
        'crt': False,
        'pre': True,
    },
    'vector-solo-cdecl': {
        'header': '''namespace T {
typedef float f4 __attribute__((vector_size(16)));
API f4 twice(f4 v) noexcept;
API float total(f4 v) noexcept;
}
''',
        'source': '''namespace T {
f4 twice(f4 v) noexcept { return v + v; }
float total(f4 v) noexcept { return v[0] + v[1] * 10.0f + v[2] * 100.0f + v[3] * 1000.0f; }
}
''',
        'cpp': 'return (int)T::total(T::twice(T::f4{1.0f, 2.0f, 3.0f, 4.0f}));',
        'ns_prelude': 'using NativeSharp.Core.Numerics;\n',
        'ns_refs': CORE_TOKEN,
        'ns': 'return (int)T.total(T.twice(new Float32x4(1.0f, 2.0f, 3.0f, 4.0f)));',
        'crt': False,
        'ir': True,
    },
    'vector-suelto-double': {
        'header': '''namespace T {
typedef double d2 __attribute__((vector_size(16)));
typedef int i4 __attribute__((vector_size(16)));
typedef long long l2 __attribute__((vector_size(16)));
API d2 dmake(double a, double b) noexcept;
API double dsum(d2 v) noexcept;
API d2 __vectorcall vdscale(d2 v, double k) noexcept;
API i4 __vectorcall viadd(i4 a, i4 b) noexcept;
API long long __vectorcall vlsum(l2 v, int k) noexcept;
}
''',
        'source': '''namespace T {
d2 dmake(double a, double b) noexcept { return d2{a * 3.0, b + 0.5}; }
double dsum(d2 v) noexcept { return v[0] + v[1] * 100.0; }
d2 __vectorcall vdscale(d2 v, double k) noexcept { return v * k; }
i4 __vectorcall viadd(i4 a, i4 b) noexcept { return a + b * 3; }
long long __vectorcall vlsum(l2 v, int k) noexcept { return v[0] * k + v[1]; }
}
''',
        'cpp': 'T::d2 d = T::dmake(2.0, 4.0); T::d2 s = T::vdscale(d, 2.0); T::i4 i = T::viadd(T::i4{1, 2, 3, 4}, '
               'T::i4{5, 6, 7, 8}); return (int)T::dsum(d) + (int)T::dsum(s) * 3 + i[0] + i[1] * 10 + i[2] * 100 + '
               'i[3] * 1000 + (int)T::vlsum(T::l2{7, 9}, 5) * 7 + (int)s[1];',
        'ns_prelude': 'using NativeSharp.Core.Numerics;\n',
        'ns_refs': CORE_TOKEN,
        'ns': 'Float64x2 d = T.dmake(2.0, 4.0); Float64x2 s = T.vdscale(d, 2.0); Int32x4 a; a.X = 1; a.Y = 2; '
              'a.Z = 3; a.W = 4; Int32x4 b; b.X = 5; b.Y = 6; b.Z = 7; b.W = 8; Int32x4 i = T.viadd(a, b); '
              'Int64x2 l; l.X = 7L; l.Y = 9L; return (int)T.dsum(d) + (int)T.dsum(s) * 3 + i.X + i.Y * 10 + '
              'i.Z * 100 + i.W * 1000 + (int)T.vlsum(l, 5) * 7 + (int)s.Y;',
        'crt': False,
        'ir': True,
    },
    'vector-suelto-avx': {
        'header': '''namespace T {
typedef float f8 __attribute__((vector_size(32)));
typedef double d4 __attribute__((vector_size(32)));
typedef int i8 __attribute__((vector_size(32)));
typedef long long l4 __attribute__((vector_size(32)));
API f8 fscale8(f8 v, float k) noexcept;
API float fsum8(f8 v) noexcept;
API d4 dmake4(double a) noexcept;
API double dsum4(d4 v) noexcept;
API i8 iadd8(i8 a, i8 b) noexcept;
API int isum8(i8 v) noexcept;
API l4 lmake4(long long a) noexcept;
API long long lsum4(l4 v) noexcept;
API f8 __vectorcall vscale8(f8 v, double k) noexcept;
API d4 __vectorcall vdmix4(d4 a, d4 b) noexcept;
}
''',
        'source': '''namespace T {
f8 fscale8(f8 v, float k) noexcept { return v * k; }
float fsum8(f8 v) noexcept { float s = 0.0f; for (int i = 0; i < 8; ++i) s += v[i] * (float)(i + 1); return s; }
d4 dmake4(double a) noexcept { return d4{a, a + 1.0, a * 2.0, a - 3.0}; }
double dsum4(d4 v) noexcept { return v[0] + v[1] * 10.0 + v[2] * 100.0 + v[3] * 1000.0; }
i8 iadd8(i8 a, i8 b) noexcept { return a + b * 2; }
int isum8(i8 v) noexcept { int s = 0; for (int i = 0; i < 8; ++i) s += v[i] * (i + 1); return s; }
l4 lmake4(long long a) noexcept { return l4{a, a * 2, a * 3, a * 4}; }
long long lsum4(l4 v) noexcept { return v[0] + v[1] + v[2] + v[3] * 10; }
f8 __vectorcall vscale8(f8 v, double k) noexcept { return v * (float)k; }
d4 __vectorcall vdmix4(d4 a, d4 b) noexcept { return a + b * 0.5; }
}
''',
        'cpp': 'T::f8 a = T::f8{1, 2, 3, 4, 5, 6, 7, 8}; T::f8 v = T::fscale8(a, 2.0f); T::d4 d = T::dmake4(2.0); '
               'T::i8 i = T::i8{1, 2, 3, 4, 5, 6, 7, 8}; T::i8 j = T::iadd8(i, i); T::l4 l = T::lmake4(5); '
               'return (int)T::fsum8(v) + (int)T::dsum4(T::vdmix4(d, d)) + T::isum8(j) * 3 + (int)T::lsum4(l) * 7 + '
               '(int)T::fsum8(T::vscale8(v, 0.5)) * 11 + (int)v[7] + j[6] + (int)l[3] + (int)d[2];',
        'ns_prelude': 'using NativeSharp.Core.Numerics;\n',
        'ns_refs': CORE_TOKEN,
        'ns': 'Float32x8 a; a.X = 1.0f; a.Y = 2.0f; a.Z = 3.0f; a.W = 4.0f; a.A = 5.0f; a.B = 6.0f; a.C = 7.0f; '
              'a.D = 8.0f; Float32x8 v = T.fscale8(a, 2.0f); Float64x4 d = T.dmake4(2.0); Int32x8 i; i.X = 1; '
              'i.Y = 2; i.Z = 3; i.W = 4; i.A = 5; i.B = 6; i.C = 7; i.D = 8; Int32x8 j = T.iadd8(i, i); '
              'Int64x4 l = T.lmake4(5L); return (int)T.fsum8(v) + (int)T.dsum4(T.vdmix4(d, d)) + T.isum8(j) * 3 + '
              '(int)T.lsum4(l) * 7 + (int)T.fsum8(T.vscale8(v, 0.5)) * 11 + (int)v.D + j.C + (int)l.W + (int)d.Z;',
        'crt': False,
        'ir': True,
        'cpu': 'x86-64-v3',
        'cpu_rechazo': 'x86-64-v3',
    },
    'vectorcall-avx': {
        'header': '''namespace T {
typedef float f8 __attribute__((vector_size(32)));
typedef double d4 __attribute__((vector_size(32)));
struct W8 { f8 a; f8 b; };
struct D4 { d4 v[3]; };
API W8 __vectorcall w8make(float s) noexcept;
API float __vectorcall w8sum(W8 w, float k) noexcept;
API D4 __vectorcall d4make(double s) noexcept;
API double __vectorcall d4sum(D4 d, W8 w) noexcept;
API int __vectorcall wmix(double k, W8 a, W8 b, W8 c) noexcept;
}
''',
        'source': '''namespace T {
W8 __vectorcall w8make(float s) noexcept
{
    W8 r;
    for (int i = 0; i < 8; ++i) { r.a[i] = s + (float)i; r.b[i] = s * (float)(i + 1); }
    return r;
}
float __vectorcall w8sum(W8 w, float k) noexcept { return (w.a[0] + w.a[7]) * k + w.b[5]; }
D4 __vectorcall d4make(double s) noexcept
{
    D4 r;
    for (int j = 0; j < 3; ++j) for (int i = 0; i < 4; ++i) r.v[j][i] = s * (double)(j + 1) + (double)i;
    return r;
}
double __vectorcall d4sum(D4 d, W8 w) noexcept { return d.v[0][1] + d.v[2][3] * 10.0 + (double)w.b[2]; }
int __vectorcall wmix(double k, W8 a, W8 b, W8 c) noexcept
{
    return (int)(a.a[1] + b.b[2] * 10.0f + c.a[6] * 100.0f) + (int)k;
}
}
''',
        'cpp': 'T::W8 w = T::w8make(1.5f); return (int)(T::w8sum(w, 2.0f) * 10.0f) + '
               '(int)w.b[7] * 1000 + (int)T::d4sum(T::d4make(2.0), w) * 10000 + '
               'T::wmix(4.0, w, T::w8make(2.0f), w) * 7;',
        'ns': 'T.W8 w = T.w8make(1.5f); return (int)(T.w8sum(w, 2.0f) * 10.0f) + '
              '(int)w.b[7] * 1000 + (int)T.d4sum(T.d4make(2.0), w) * 10000 + '
              'T.wmix(4.0, w, T.w8make(2.0f), w) * 7;',
        'crt': False,
        'ir': True,
        'cpu': 'x86-64-v3',
        'cpu_rechazo': 'x86-64-v3',
    },
    'vectorcall-avx512': {
        'header': '''namespace T {
typedef float f8 __attribute__((vector_size(32)));
typedef double d4 __attribute__((vector_size(32)));
typedef int i16 __attribute__((vector_size(64)));
struct W8 { f8 a; f8 b; };
struct D4 { d4 v[3]; };
struct Z1 { i16 z; };
API W8 __vectorcall w8make(float s) noexcept;
API float __vectorcall w8sum(W8 w, float k) noexcept;
API D4 __vectorcall d4make(double s) noexcept;
API double __vectorcall d4sum(D4 d, W8 w) noexcept;
API Z1 __vectorcall zmake(int s) noexcept;
API int __vectorcall zsum(Z1 z, int k) noexcept;
API int __vectorcall wmix(double k, W8 a, W8 b, W8 c) noexcept;
}
''',
        'source': '''namespace T {
W8 __vectorcall w8make(float s) noexcept
{
    W8 r;
    for (int i = 0; i < 8; ++i) { r.a[i] = s + (float)i; r.b[i] = s * (float)(i + 1); }
    return r;
}
float __vectorcall w8sum(W8 w, float k) noexcept { return (w.a[0] + w.a[7]) * k + w.b[5]; }
D4 __vectorcall d4make(double s) noexcept
{
    D4 r;
    for (int j = 0; j < 3; ++j) for (int i = 0; i < 4; ++i) r.v[j][i] = s * (double)(j + 1) + (double)i;
    return r;
}
double __vectorcall d4sum(D4 d, W8 w) noexcept { return d.v[0][1] + d.v[2][3] * 10.0 + (double)w.b[2]; }
Z1 __vectorcall zmake(int s) noexcept
{
    Z1 r;
    for (int i = 0; i < 16; ++i) r.z[i] = s + i * 3;
    return r;
}
int __vectorcall zsum(Z1 z, int k) noexcept { return z.z[0] + z.z[15] * k; }
int __vectorcall wmix(double k, W8 a, W8 b, W8 c) noexcept
{
    return (int)(a.a[1] + b.b[2] * 10.0f + c.a[6] * 100.0f) + (int)k;
}
}
''',
        'cpp': 'T::W8 w = T::w8make(1.5f); T::Z1 z = T::zmake(2); return (int)(T::w8sum(w, 2.0f) * 10.0f) + '
               '(int)w.b[7] * 1000 + (int)T::d4sum(T::d4make(2.0), w) * 10000 + T::zsum(z, 3) * 3 + '
               'T::wmix(4.0, w, T::w8make(2.0f), w) * 7 + z.z[9];',
        'ns': 'T.W8 w = T.w8make(1.5f); T.Z1 z = T.zmake(2); return (int)(T.w8sum(w, 2.0f) * 10.0f) + '
              '(int)w.b[7] * 1000 + (int)T.d4sum(T.d4make(2.0), w) * 10000 + T.zsum(z, 3) * 3 + '
              'T.wmix(4.0, w, T.w8make(2.0f), w) * 7 + z.z[9];',
        'crt': False,
        'ir': True,
        'cpu': 'x86-64-v4',
        'pre': False,
        'solo_ir': 'la CPU de este equipo (Comet Lake) no tiene AVX-512',
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
    'excepcion-adaptador': {
        'header': '''namespace T {
using Cb = int(*)(int);
struct Base { int code; API Base() noexcept; API virtual int fire(int x); };
struct Derived : Base { API Derived() noexcept; API int fire(int x) override; };
API Base* makeBase() noexcept;
API Base* makeDerived() noexcept;
API int guarded(Cb f);
}
''',
        'source': '''namespace T {
Base::Base() noexcept : code(1) {}
int Base::fire(int x) { return x + code; }
Derived::Derived() noexcept { code = 30; }
int Derived::fire(int x) { throw code + x; }
static Base base;
static Derived derived;
Base* makeBase() noexcept { return &base; }
Base* makeDerived() noexcept { return &derived; }
int guarded(Cb f) { try { return f(1); } catch (int e) { return e; } }
}
''',
        'cpp_prelude': 'static int fire(int v) { T::Base* b = T::makeDerived(); return b->fire(v); }\n'
                       'static int plain(int v) { T::Base* b = T::makeBase(); return b->fire(v); }\n',
        'cpp': 'return T::guarded(fire) * 100 + T::guarded(plain);',
        'ns_prelude': 'int Fire(int v) { T.Base* b = T.makeDerived(); return b->fire(v); }\n'
                      'int Plain(int v) { T.Base* b = T.makeBase(); return b->fire(v); }\n',
        'ns': 'T.Cb f = (int v) => Fire(v); T.Cb g = (int v) => Plain(v); return T.guarded(f) * 100 + T.guarded(g);',
        'crt': True,
        'adapter': True,
    },
    'excepcion-especializacion': {
        'header': '''namespace T {
using Cb = int(*)(int);
API int guarded(Cb f);
template <class V> struct Box { V v; static V check(V x) { if (x > 2) throw x * 10; return x; } };
}
''',
        'source': '''namespace T {
int guarded(Cb f) { try { return f(1); } catch (int e) { return e; } }
}
''',
        'cpp': 'return T::guarded([](int v) { return T::Box<int>::check(v + 5); }) * 100 + '
               'T::guarded([](int v) { return T::Box<int>::check(v); });',
        'ns': 'T.Cb f = (int v) => T.Box<int>.check(v + 5); T.Cb g = (int v) => T.Box<int>.check(v); '
              'return T.guarded(f) * 100 + T.guarded(g);',
        'crt': True,
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


def project(folder, body, dll, references=None, library=False, name='T19Traslados', source=None, cpu=None):
    if references is not None:
        references = references.replace(CORE_TOKEN, core_reference(CORE_LEVEL[0]))
    output = ('<OutputType>Library</OutputType>' if library else '') + ('<CpuLevel>' + cpu + '</CpuLevel>' if cpu else '')
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
        result = 'agg' if words and words[-1].startswith(('{', '%', '}')) or '{' in head else \
            'vec' + head[head.index('<'):head.index('>') + 1].replace(' ', '') if '<' in head else (words[-1] if words else '')
        shapes = []
        for parameter in [p.strip() for p in re.split(r',\s*(?![^{]*})', parameters)] if parameters.strip() else []:
            kind = 'sret' if 'sret' in parameter else 'inreg' if ' inreg' in parameter else \
                'ptr' if parameter.startswith('ptr') else 'agg' if parameter.startswith(('{', '%')) else \
                'vec' + parameter[:parameter.index('>') + 1].replace(' ', '') if parameter.startswith('<') else parameter.split()[0]
            shapes.append(kind)
        found[symbol] = ('x86_vectorcallcc' in head, result, tuple(shapes))
    return found


def pointer_shapes(ir, convention):
    """Shapes of the calls through a pointer and of the callbacks (`mine*` in C++, `__*_boundary` in NativeSharp)
    of one convention: `x86_vectorcallcc`, or '' for the default C convention."""
    # Formas de las llamadas por puntero y de los callbacks de una convencion.
    found = set()
    prefix = convention + ' ' if convention else ''
    for line in ir.splitlines():
        line = line.strip()
        match = re.match(r'(?:%\S+ = )?(?:tail )?call ' + prefix + r'(.*?) %[\w.]+\((.*)\)', line)
        if match and not convention and match.group(1).startswith('x86_'):
            continue
        if not match:
            match = re.match(r'define (?:internal |dso_local |private |weak_odr )*' + prefix +
                             r'(.*?) @"?([^"(]*)"?\((.*)\)', line)
            if not match or not ('mine' in match.group(2) or match.group(2).endswith('_boundary')) or \
                    (not convention and match.group(1).startswith('x86_')):
                continue
            head, parameters = match.group(1), match.group(3)
        else:
            head, parameters = match.groups()
        words = [w for w in head.split() if w not in ('dso_local', 'noundef', 'nonnull', 'zeroext', 'signext',
                                                      'inreg') and not w.startswith(('dereferenceable', 'align'))]
        result = 'agg' if '{' in head or '%' in head else (words[-1] if words else '')
        shapes = []
        for parameter in [p.strip() for p in re.split(r',\s*(?![^{]*})', parameters)] if parameters.strip() else []:
            kind = 'sret' if 'sret' in parameter else 'inreg' if ' inreg' in parameter else \
                'ptr' if parameter.startswith('ptr') else 'agg' if parameter.startswith(('{', '%')) else parameter.split()[0]
            shapes.append(kind)
        found.add((result, tuple(shapes)))
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


def crt_build_args(case):
    """With the CRT, NativeSharp starts through the static MSVC runtime and links with lld-link."""
    # Con el CRT, NativeSharp arranca por el runtime estatico de MSVC y enlaza con lld-link.
    if not case['crt']:
        return [], {}
    extra_args = ['--crt-startup']
    for item in CRT:
        extra_args += ['--link-lib', str(item)]
    return extra_args, {'NSC_LINKER': 'lld-link.exe'}


def precompiled(name, case, folder, opt, control):
    CORE_LEVEL[0] = opt
    """The same body from a precompiled NativeSharp generic, with and without the C++ header for the consumer."""
    # El mismo cuerpo desde una plantilla NativeSharp precompilada, con y sin el encabezado C++ en el consumidor.
    producer = folder / 'producer'
    producer.mkdir()
    project(producer, '', args.dll, reference(args.dll, '../') + case.get('ns_refs', ''), True, 'T19TrasladosProducer',
            cpu=case.get('cpu'), source=
            (case.get('ns_prelude', '') if case.get('pre_prelude') else
             ''.join(line + '\n' for line in case.get('ns_prelude', '').splitlines() if line.startswith('using '))) +
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
        project(consumer, 'return T19P.Bridge.Run<int>(0);', args.dll, references + case.get('ns_refs', ''),
                cpu=case.get('cpu'))
        try:
            result = nsc_build(f'{name}-O{opt}-{visibility}', consumer, opt, *crt_build_args(case))
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
    declaration, read = case.get('dll_tls', ('static constinit thread_local', 'T::S::counter'))
    exported = case['header'].replace(declaration, 'API ' + declaration)
    header = '#define T19_IMPORT 1\n' + API + exported
    (folder / 'api.h').write_text(header, encoding='utf-8')
    (folder / 'control.cpp').write_text('#include "api.h"\nint main() { return ' + read + '; }\n', encoding='utf-8')
    clang = run(name + '-dll-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], folder, expected=1).stdout
    if 'thread' not in clang or 'dllimport' not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(name + ': Clang no rechaza el thread_local dllimport; evidencia: ' + str(root))
    print(name + ': Clang rechaza un thread_local dllimport (ABI de Microsoft); no aplica en DLL', flush=True)


def check(name, case):
    if args.dll and case.get('dll') == 'tls-rechazado':
        tls_dll_rejected(name, case)
        return
    library = root / (name + '-lib')
    library.mkdir()
    (library / 'api.h').write_text(API + case['header'], encoding='utf-8')
    # The DLL is linked without the CRT: it defines the floating-point marker itself, as the control does.
    # La DLL se enlaza sin el CRT: define ella misma el marcador de punto flotante, como el control.
    dll_marker = 'extern "C" int _fltused = 0;\n' if args.dll and not case['crt'] else ''
    (library / 'api.cpp').write_text('#include "api.h"\n' + dll_marker + case['source'], encoding='utf-8')
    run(name + '-api', [*cxx(case), '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'],
        library)
    if args.dll and case['crt']:
        # The DLL links its own static C runtime and starts it from its DLL entry, as in tanda 15.
        # La DLL enlaza su propio runtime estatico de C y lo arranca desde su entrada de DLL, como en la tanda 15.
        run(name + '-api-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                                *[str(p) for p in CRT]], library)
        header = '#define T19_IMPORT 1\n' + API + case['header']
    elif args.dll:
        run(name + '-api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                '/implib:api.lib', 'api.obj'], library)
        header = '#define T19_IMPORT 1\n' + API + case['header']
    else:
        run(name + '-api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
        header = API + case['header']
    for opt in range(4):
        CORE_LEVEL[0] = opt
        folder = root / f'{name}-O{opt}'
        folder.mkdir()
        (folder / 'api.h').write_text(header, encoding='utf-8')
        for item in ('api.lib', *(('api.dll',) if args.dll else ())):
            (folder / item).write_bytes((library / item).read_bytes())
        # Without the CRT the control defines the floating-point marker itself; with it, libcmt.lib does.
        # Sin el CRT el control define el marcador de punto flotante; con el CRT lo define libcmt.lib.
        marker = '' if case['crt'] else 'extern "C" __declspec(selectany) int _fltused = 0;\n'
        (folder / 'control.cpp').write_text('#include "api.h"\n' + marker + case.get('cpp_prelude', '') +
                                            'int main() { ' + case['cpp'] + ' }\n', encoding='utf-8')
        if case['crt']:
            run(f'{name}-O{opt}-control', [*cxx(case), '-O' + str(opt), '-c', 'control.cpp', '-o', 'control.obj'], folder)
            run(f'{name}-O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe',
                                                'control.obj', 'api.lib', *[str(p) for p in CRT]], folder)
        else:
            run(f'{name}-O{opt}-control', [*cxx(case), '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld',
                                           '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'], folder)
        control = None if case.get('solo_ir') else \
            run(f'{name}-O{opt}-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode
        source = case['ns_prelude'] + 'int Main()\n{\n    ' + case['ns'] + '\n}\n' if 'ns_prelude' in case else None
        project(folder, case['ns'], args.dll, reference(args.dll) + case.get('ns_refs', ''), source=source,
                cpu=case.get('cpu'))
        if case.get('solo_ir'):
            # EN: the CPU of this machine cannot run the program: it compiles and links, and every declaration has
            #     Clang's shape. / ES: la CPU de este equipo no puede ejecutar el programa: compila y enlaza, y cada
            #     declaracion tiene la forma de Clang.
            run(f'{name}-O{opt}-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' +
                args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder)
            clang_ir = run(f'{name}-O{opt}-control-ir', [*cxx(case), '-O' + str(opt), '-S', '-emit-llvm',
                                                          'control.cpp', '-o', '-'], folder).stdout
            nsc_ir = run(f'{name}-O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                   '--cpp-clang-root=' + args.clang_root], folder).stdout
            print(f'{name} O{opt}: compila y enlaza; {compare(f"{name}-O{opt}", clang_ir, nsc_ir)} declaraciones con '
                  f'la forma de Clang; no se ejecuta: {case["solo_ir"]}', flush=True)
            continue
        extra_args, extra = crt_build_args(case)
        result = nsc_build(f'{name}-O{opt}', folder, opt, extra_args, extra)
        if result != control:
            raise SystemExit(f'{name} O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
        detail = ''
        if case.get('adapter'):
            # The virtual call goes through a Clang adapter, and the exception crosses it.
            # La llamada virtual pasa por un adaptador de Clang, y la excepcion lo atraviesa.
            nsc_ir = run(f'{name}-O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                   '--cpp-clang-root=' + args.clang_root], folder).stdout
            adapters = set(re.findall(r'(?:call|invoke) [^@]*@(__nsc_cpp_call_[0-9a-f]{16})\(', nsc_ir))
            if not adapters:
                raise SystemExit(f'{name} O{opt}: la llamada virtual no pasa por un adaptador de Clang; evidencia: {root}')
            detail = f'; {len(adapters)} adaptadores de Clang'
        if case.get('ptr_ir'):
            # Each call through a vectorcall pointer and each NativeSharp callback has a shape Clang uses for the
            # same pointer type.
            # Cada llamada por un puntero vectorcall y cada callback NativeSharp tiene una forma que Clang usa
            # para el mismo tipo de puntero.
            clang_ir = run(f'{name}-O{opt}-control-ir', [*cxx(case), '-O0', '-S', '-emit-llvm', 'control.cpp', '-o', '-'],
                           folder).stdout
            nsc_ir = run(f'{name}-O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                   '--cpp-clang-root=' + args.clang_root], folder).stdout
            convention = case.get('ptr_cc', 'x86_vectorcallcc')
            clang_shapes, nsc_shapes = pointer_shapes(clang_ir, convention), pointer_shapes(nsc_ir, convention)
            if not nsc_shapes or not nsc_shapes <= clang_shapes or not clang_shapes <= nsc_shapes:
                raise SystemExit(f'{name} O{opt}: formas vectorcall por puntero: NSC {sorted(nsc_shapes)}, '
                                 f'Clang {sorted(clang_shapes)}; evidencia: {root}')
            detail += f'; {len(nsc_shapes)} formas de llamada por puntero como Clang'
        if 'tls_guard' in case:
            # Clang guards a thread_local without constant initialization with the CRT's on-demand TLS
            # initialization; NSC must emit the same guard, and none for a constinit one.
            # Clang protege un thread_local sin inicializacion constante con la inicializacion TLS bajo demanda del
            # CRT; NSC debe emitir la misma guarda, y ninguna para uno constinit.
            clang_ir = run(f'{name}-O{opt}-control-ir', [*cxx(case), '-O' + str(opt), '-S', '-emit-llvm', 'control.cpp',
                                                          '-o', '-'], folder).stdout
            nsc_ir = run(f'{name}-O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                   '--cpp-clang-root=' + args.clang_root], folder).stdout
            clang_guard = '@__dyn_tls_on_demand_init()' in clang_ir and '@__tls_guard' in clang_ir
            nsc_guard = '@__dyn_tls_on_demand_init()' in nsc_ir and '@__tls_guard' in nsc_ir
            if clang_guard != case['tls_guard'] or nsc_guard != case['tls_guard']:
                raise SystemExit(f'{name} O{opt}: guarda TLS bajo demanda: Clang {clang_guard}, NSC {nsc_guard}, '
                                 f'se esperaba {case["tls_guard"]}; evidencia: {root}')
            detail += '; guarda TLS bajo demanda como Clang' if nsc_guard else '; sin guarda TLS, como Clang'
        if case.get('ir'):
            clang_ir = run(f'{name}-O{opt}-control-ir', [*cxx(case), '-O' + str(opt), '-S', '-emit-llvm', 'control.cpp',
                                                          '-o', '-'], folder).stdout
            nsc_ir = run(f'{name}-O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj',
                                                   '--cpp-clang-root=' + args.clang_root], folder).stdout
            detail = f'; {compare(f"{name}-O{opt}", clang_ir, nsc_ir)} declaraciones con la forma de Clang'
        if case.get('pre', bool(case.get('ir'))):
            precompiled(name, case, folder, opt, control)
            detail += '; plantilla precompilada con y sin encabezado'
        print(f'{name} O{opt}: NativeSharp y Clang devuelven {result}{detail}', flush=True)
    if case.get('cpu_rechazo'):
        cpu_rejections(name, case, root / f'{name}-O0')
    for index, (body, expected) in enumerate(case.get('rechazos', ())):
        rejected(name, index, body, expected, root / f'{name}-O0')


def rejected(name, index, body, expected, folder):
    """A use that C++ rejects (here, for access) is rejected by NativeSharp with its own diagnostic."""
    # Un uso que C++ rechaza (aqui, por acceso) lo rechaza NativeSharp con su propio diagnostico.
    direct = folder / f'rechazo-{index}'
    direct.mkdir()
    for item in ('api.h', 'api.lib', *(('api.dll',) if args.dll else ())):
        (direct / item).write_bytes((folder / item).read_bytes())
    project(direct, body, args.dll, reference(args.dll))
    result = run(f'{name}-rechazo-{index}-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                 '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o', 'app.exe'], direct,
                 expected=1).stdout
    if expected not in result or 'internal error' in result:
        print(result[-2000:], flush=True)
        raise SystemExit(f'{name}: «{body}» debia rechazarse con «{expected}»; evidencia: {root}')
    print(f'{name}: «{body}» se rechaza («{expected}»)', flush=True)


def cpu_rejections(name, case, folder):
    CORE_LEVEL[0] = 0
    """Without the CPU level, the direct import and a precompiled plan of a higher level are rejected."""
    # Sin el nivel de CPU se rechazan la importacion directa y un plan precompilado de nivel mayor.
    expected = case['cpu_rechazo']
    direct = folder / 'sin-nivel'
    direct.mkdir()
    for item in ('api.h', 'api.lib', *(('api.dll',) if args.dll else ())):
        (direct / item).write_bytes((folder / item).read_bytes())
    source = case['ns_prelude'] + 'int Main()\n{\n    ' + case['ns'] + '\n}\n' if 'ns_prelude' in case else None
    project(direct, case['ns'], args.dll, reference(args.dll) + case.get('ns_refs', ''), source=source)
    result = run(f'{name}-sin-nivel-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' +
                 args.clang_root, '--no-artifact-cache', '-o', 'app.exe'], direct, expected=1).stdout
    if expected not in result or 'internal error' in result:
        print(result[-2000:], flush=True)
        raise SystemExit(f'{name}: sin nivel de CPU se esperaba un rechazo con «{expected}»; evidencia: {root}')
    consumer = folder / 'sin-nivel-precompilado'
    consumer.mkdir()
    if args.dll:
        (consumer / 'api.dll').write_bytes((folder / 'api.dll').read_bytes())
    (folder / 'api.h').rename(folder / 'api.h.oculto')
    try:
        project(consumer, 'return T19P.Bridge.Run<int>(0);', args.dll,
                '<LibraryReference Include="../producer/Producer.lib" />' + case.get('ns_refs', ''))
        result = run(f'{name}-sin-nivel-precompilado-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                     '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o', 'app.exe'], consumer,
                     expected=1).stdout
    finally:
        (folder / 'api.h.oculto').rename(folder / 'api.h')
    if expected not in result or 'internal error' in result:
        print(result[-2000:], flush=True)
        raise SystemExit(f'{name}: el consumidor precompilado sin nivel de CPU debia rechazarse con «{expected}»; '
                         f'evidencia: {root}')
    print(f'{name}: sin el nivel de CPU se rechaza, directo y desde la plantilla precompilada sin encabezado '
          f'(«{expected}»)', flush=True)


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for name, case in CASES.items():
    if args.caso and args.caso != name:
        continue
    check(name, case)
print('OK: traslados de la tanda 19 frente a Clang; evidencia:', root)
