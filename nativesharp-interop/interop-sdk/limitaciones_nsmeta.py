"""Limitaciones NSMeta: consumidores precompilados sin encabezado (R1-05, columna L de X2/X3/X4, C5-07).

Una plantilla publicada en una biblioteca NativeSharp (modo lib) se instancia en un consumidor que no tiene el
encabezado C/C++; en modo dll la biblioteca se consume por su biblioteca de importacion. Antes, el productor rechazaba
cualquier record C++ que no fuera un valor de bytes ("C++ record has no eligible value observation"), una
especializacion de plantilla de clase no se analizaba en la superficie publicada ("expected ';'") y una lambda hacia un
puntero `sysv_abi` con records exigia que la plantilla nombrara el typedef.

Representacion (contrato vigente de NSMeta, ampliado sin version paralela): la entrada `MacroSource` del catalogo C,
que ya llevaba la fuente de la referencia preprocesada por Clang para las macros, se marca como sesion de fuente
(`__nsc_source_session`) y nombra los records que describe (`type:<identidad>`), las plantillas de clase usadas
(`template:<nombre>`) y las opciones de generacion de codigo de la referencia (`option:rtti=0`,
`option:exceptions=0`). El consumidor abre sobre esa fuente una sesion Clang completa, propia del proyecto
precompilado, e importa de ella los records, sus miembros de vida, bases y miembros virtuales, las especializaciones y
los typedefs, como lo hace un consumidor con encabezado. Los records de esa referencia no entran al catalogo de
valores, para que el consumidor no vea el mismo record por dos vias. Un consumidor anterior que no conoce la marca
la lee como un nombre de macro mas.

Casos (O0-O3, lib y dll):
- nsm-obj: record C++ con constructor, constructor de copia y destructor no triviales (construccion y destruccion).
- nsm-mix: un record trivial y uno no trivial de la misma referencia.
- nsm-poly: record polimorfico (destructor y metodo virtuales, derivado con `override`, despacho por la base).
- nsm-spec: especializacion no trivial `Box<int>` de una plantilla de clase C++.
- nsm-spec-trivial: especializacion trivial `TBox<int>`.
- nsm-sysv-typedef: typedef `sysv_abi` devuelto por una funcion inline.
- nsm-sysv-lambda: lambda hacia un puntero `sysv_abi` con un record, sin nombrar el typedef en la plantilla.
"""
from comun import Caso, positivo, fin

H = r'''
namespace lib {
extern int g_d;
struct Obj { int v; Obj(int x) : v(x) {} Obj(const Obj& o) : v(o.v) {} ~Obj() { ++g_d; } int get() const { return v; } };
inline int dtors() { return g_d; }
struct Triv { int a; int b; int sum() const { return a + b; } };
struct Base { virtual ~Base() {} virtual int f() const { return 1; } int twice() const { return 2 * f(); } };
struct Der : Base { int k; Der(int x) : k(x) {} int f() const override { return k; } };
template <class T> struct Box { T v; Box(T x) : v(x) {} ~Box() { ++g_d; } T get() const { return v; } };
template <class T> struct TBox { T v; T get() const { return v; } };
typedef int (__attribute__((sysv_abi)) *SysvFn)(int, int);
inline int __attribute__((sysv_abi)) addSysv(int a, int b) { return a + b; }
inline SysvFn sysvAdder() { return &addSysv; }
}
'''
# The C++ test library links without the C++ runtime: a virtual destructor names the sized operator delete.
# La biblioteca de prueba enlaza sin el runtime C++: un destructor virtual nombra el operator delete con tamano.
S = ('namespace lib { int g_d; }\nvoid operator delete(void*, decltype(sizeof(0))) noexcept {}\n'
     'void operator delete(void*) noexcept {}\n')
C = dict(language='C++', standard='c++20', extra_ref=' Exceptions="false" Rtti="false"')
MODES = ['lib', 'dll']
OPTS = ['0', '1', '2', '3']
positivo(Caso('nsm-obj', H, S, **C),
         'int r = 0; { lib.Obj o = new lib.Obj(41); r = o.get(); } return r + lib.dtors();', modes=MODES, opts=OPTS)
positivo(Caso('nsm-mix', H, S, **C),
         'lib.Triv t; t.a = 40; t.b = 1; int r = 0; { lib.Obj o = new lib.Obj(t.sum()); r = o.get(); } '
         'return r + lib.dtors();', modes=MODES, opts=OPTS)
positivo(Caso('nsm-poly', H, S, **C),
         'lib.Der d = new lib.Der(21); return d.twice() == 42 && d.f() == 21 ? 42 : 1;', modes=MODES, opts=OPTS)
positivo(Caso('nsm-spec', H, S, **C),
         'int r = 0; { lib.Box<int> b = new lib.Box<int>(41); r = b.get(); } return r + lib.dtors();',
         modes=MODES, opts=OPTS)
positivo(Caso('nsm-spec-trivial', H, S, **C), 'lib.TBox<int> b; b.v = 42; return b.get();', modes=MODES, opts=OPTS)
positivo(Caso('nsm-sysv-typedef', H, S, **C), 'lib.SysvFn f = lib.sysvAdder(); return f(40, 2);',
         modes=MODES, opts=OPTS)

H7 = r'''
typedef struct P { int x, y; } P;
typedef int (__attribute__((sysv_abi)) *scb)(P p, int k);
int call_scb(scb f);
'''
S7 = 'int call_scb(scb f) { P p = {30, 10}; return f(p, 2); }\n'
positivo(Caso('nsm-sysv-lambda', H7, S7), 'return call_scb((p, k) => p.x + p.y + k);', modes=MODES, opts=OPTS)
fin()
