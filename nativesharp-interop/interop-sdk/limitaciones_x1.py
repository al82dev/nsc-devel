"""Limitaciones X1: identidad, sobrecargas y referencias C++ (plan de limitaciones, tareas X1-01..X1-06).

X1-01: un alias (`using`/`typedef`) nombra el mismo tipo; una declaracion adelantada es un tipo opaco por puntero.
X1-02: familias de sobrecargas por aridad y tipo; con dos conversiones posibles se conserva el orden NativeSharp
(T10-009, `536baf9e4`): la constante `g(5)` elige `g(unsigned)` donde C++ seria ambigua (restriccion consciente).
X1-03: constructor `explicit`, acceso privado y `= delete`: una sobrecarga eliminada (funcion libre o metodo)
participa en la seleccion y la llamada que la elige se rechaza, en vez de pasar a otra sobrecarga o al enlazador.
X1-04: `T&` (ref), `const T&` (in, o un valor sin `in` que se materializa para la llamada si T no es un record C++),
`T&&` (valor materializado), retorno por referencia y referencia a un campo.
X1-05: ref-qualifiers `&` y `const &` en el receptor; `&&` sobre un temporal (rechazado sobre un objeto con nombre).
X1-06: los mismos resultados en importacion directa, biblioteca precompilada sin encabezado y DLL.
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
namespace lib {
int f(int a);
int f(int a, int b);
int f(double d);
int g(long long v);
int g(unsigned v);
void h(int) = delete;
int h(double d);
struct Box { int v; explicit Box(int x) : v(x) {} int get() const { return v; } };
using BoxAlias = Box;
typedef Box BoxT;
int unbox(const Box& b);
int unbox_alias(const BoxAlias& b);
struct Fwd;
Fwd* make_fwd();
int fwd_value(Fwd* p);
struct Acc { int total = 0; void add(int v) & { total += v; } int peek() const & { return total; } int take() && { return total; } };
int& slot();
int bump(int& r);
int read(const int& r);
double readd(const double& r);
int sink(int&& r);
struct Pair { int a; int b; };
int& field_a(Pair& p);
class Hidden { int secret = 1; int priv() { return 0; } public: int open = 41; int pub() { return open + secret - 1; } };
struct S { int m(int) = delete; int m(double d); int n(int a); };
}
'''
S = r'''
namespace lib {
int f(int a) { return a; }
int f(int a, int b) { return a + b; }
int f(double d) { return (int)(d * 2); }
int g(long long v) { return 1; }
int g(unsigned v) { return 2; }
int h(double d) { return (int)d; }
int unbox(const Box& b) { return b.v; }
int unbox_alias(const BoxAlias& b) { return b.v; }
struct Fwd { int v; };
static Fwd one = {42};
Fwd* make_fwd() { return &one; }
int fwd_value(Fwd* p) { return p->v; }
static int s = 40;
int& slot() { return s; }
int bump(int& r) { return ++r; }
int read(const int& r) { return r; }
double readd(const double& r) { return r; }
int sink(int&& r) { return r + 1; }
int& field_a(Pair& p) { return p.a; }
int S::m(double d) { return (int)d; }
int S::n(int a) { return a; }
}
'''
CPP = dict(language='C++', standard='c++20')

BODY = r'''
    if (lib.f(20) + lib.f(10, 10) + lib.f(1.0) != 42) return 1;
    if (lib.g(5L) != 1 || lib.g(5u) != 2 || lib.g(5) != 2) return 2;
    if (lib.h(42.0) != 42) return 3;
    var b = new lib.Box(42);
    var a2 = new lib.BoxAlias(42);
    if (lib.unbox(in b) != 42 || lib.unbox_alias(in a2) != 42 || b.get() != 42) return 4;
    if (lib.fwd_value(lib.make_fwd()) != 42) return 5;
    lib.Acc acc = new lib.Acc(); acc.add(40); acc.add(2);
    if (acc.peek() != 42) return 6;
    ref int r = ref lib.slot(); r = 40;
    if (lib.bump(ref r) != 41 || lib.slot() != 41) return 7;
    int v = 40;
    if (lib.read(in v) + lib.read(2) != 42 || lib.read(v) != 40 || lib.readd(1.5f) != 1.5) return 8;
    if (lib.sink(41) != 42) return 9;
    lib.Pair p; p.a = 1; p.b = 2;
    lib.field_a(ref p) = 40;
    if (p.a + p.b != 42) return 10;
    var x = new lib.Hidden();
    if (x.pub() != 41) return 11;
    var s = new lib.S();
    if (s.m(42.0) != 42 || s.n(42) != 42) return 12;
    return 42;
'''
c = Caso('x1-cpp', H, S, **CPP)
positivo(c, BODY, modes=['directo', 'dll'])
rechazo(c, 'return lib.h(4);', "call to deleted C++ function 'h(builtin:int)'")
rechazo(c, 'var s = new lib.S(); return s.m(4);', "call to deleted C++ method 'm(builtin:int)'")
rechazo(c, 'var x = new lib.Hidden(); return x.secret;', "member 'secret' is not accessible")
rechazo(c, 'var x = new lib.Hidden(); return x.priv();', 'priv')
rechazo(c, 'var b = new lib.Box(42); return lib.unbox(b);', 'lib.unbox')
rechazo(c, 'int v = 41; return lib.sink(v);', 'lib.sink')
rechazo(c, 'lib.Acc acc = new lib.Acc(); return acc.take();', 'take')
# X1-05: an `&&` method runs on a temporary, as C++ allows it only on an rvalue; on a named object it is rejected (above).
# Un metodo `&&` se ejecuta sobre un temporal; sobre un objeto con nombre se rechaza (arriba).
positivo(Caso('x1-receptor-rvalue', H, S, **CPP), 'return (new lib.Acc()).take() + 42;', modes=['directo', 'dll'])

# X1-06: the same calls through a precompiled library consumed without the header (records constructed by C++
#     helpers, as the object lifetime of `new` in a template is X2's).
H2 = r'''
namespace lib {
int f(int a);
int f(int a, int b);
int f(double d);
int g(long long v);
int g(unsigned v);
void h(int) = delete;
int h(double d);
int& slot();
int bump(int& r);
int read(const int& r);
int sink(int&& r);
}
'''
S2 = r'''
namespace lib {
int f(int a) { return a; }
int f(int a, int b) { return a + b; }
int f(double d) { return (int)(d * 2); }
int g(long long v) { return 1; }
int g(unsigned v) { return 2; }
int h(double d) { return (int)d; }
static int s = 40;
int& slot() { return s; }
int bump(int& r) { return ++r; }
int read(const int& r) { return r; }
int sink(int&& r) { return r + 1; }
}
'''
BODY2 = r'''
    if (lib.f(20) + lib.f(10, 10) + lib.f(1.0) != 42) return 1;
    if (lib.g(5) != 2 || lib.h(42.0) != 42) return 2;
    ref int r = ref lib.slot(); r = 40;
    if (lib.bump(ref r) != 41) return 3;
    int v = 40;
    if (lib.read(v) + lib.read(2) != 42 || lib.sink(41) != 42) return 4;
    return 42;
'''
c2 = Caso('x1-cpp-catalogo', H2, S2, **CPP)
positivo(c2, BODY2)
rechazo(c2, 'return lib.h(4);', "call to deleted C++ function 'h(builtin:int)'")
fin()
