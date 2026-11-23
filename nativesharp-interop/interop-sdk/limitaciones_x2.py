"""Limitaciones X2: clases, vida de objetos y operadores C++ (plan de limitaciones, tareas X2-01..X2-11).

Contadores de constructor, copia, movimiento y destructor en C++ verifican cada secuencia:
X2-02: construccion local y destruccion exactamente una vez al salir del bloque y en un retorno temprano.
X2-04: copia explicita `new T(in o)` (constructor de copia) y tipo move-only `new T(ref m)` (constructor de movimiento):
el origen movido tambien se destruye. Con constructor de copia presente, el de movimiento queda fuera (T11-016:
NativeSharp no sobrecarga ref frente a in).
X2-08: `+=` con resultado `T&`, `++`/`--` prefijos, `*o` (resultado `T&` asignable), `o(args)` y `==`, cada uno sobre el
objeto en su lugar, sin copiarlo.
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
namespace lib {
struct Counters { int ctor, copy, move, dtor; };
Counters& counters();
void reset();
struct Obj {
  int v;
  Obj(int x);
  Obj(const Obj& o);
  Obj(Obj&& o);
  ~Obj();
  Obj& operator+=(int d);
  int operator()(int k) const;
  int operator*() const;
  Obj& operator++();
  Obj& operator--();
  bool operator==(const Obj& o) const;
};
struct MoveOnly { int v; MoveOnly(int x); MoveOnly(const MoveOnly&) = delete; MoveOnly(MoveOnly&& o); ~MoveOnly(); };
struct It { int* p; int& operator*() const { return *p; } It& operator++() { ++p; return *this; } It& operator--() { --p; return *this; } };
int* data();
}
'''
S = r'''
namespace lib {
static Counters c;
Counters& counters() { return c; }
void reset() { c = Counters{}; }
Obj::Obj(int x) : v(x) { ++c.ctor; }
Obj::Obj(const Obj& o) : v(o.v) { ++c.copy; }
Obj::Obj(Obj&& o) : v(o.v) { o.v = -1; ++c.move; }
Obj::~Obj() { ++c.dtor; }
Obj& Obj::operator+=(int d) { v += d; return *this; }
int Obj::operator()(int k) const { return v + k; }
int Obj::operator*() const { return v; }
Obj& Obj::operator++() { ++v; return *this; }
Obj& Obj::operator--() { --v; return *this; }
bool Obj::operator==(const Obj& o) const { return v == o.v; }
MoveOnly::MoveOnly(int x) : v(x) { ++c.ctor; }
MoveOnly::MoveOnly(MoveOnly&& o) : v(o.v) { o.v = -1; ++c.move; }
MoveOnly::~MoveOnly() { ++c.dtor; }
static int d[4] = {1, 2, 3, 4};
int* data() { return d; }
}
'''
CPP = dict(language='C++', standard='c++20')
PRE = 'int Early() { var o = new lib.Obj(42); if (o.v == 42) return 42; return 0; }\n'
BODY = r'''
    lib.reset();
    { var o = new lib.Obj(40); o += 2; if (*o != 42) return 1; }
    ref lib.Counters k = ref lib.counters();
    if (k.ctor != 1 || k.dtor != 1) return 2;
    lib.reset();
    { var o = new lib.Obj(21); var o2 = new lib.Obj(in o); if (o2.v + o.v != 42) return 4; }
    if (k.ctor != 1 || k.copy != 1 || k.dtor != 2) return 5;
    lib.reset();
    { var m = new lib.MoveOnly(42); var m2 = new lib.MoveOnly(ref m); if (m2.v != 42 || m.v != -1) return 6; }
    if (k.ctor != 1 || k.move != 1 || k.dtor != 2) return 7;
    lib.reset();
    { var o = new lib.Obj(40); ++o; ++o; --o; ++o; var p = new lib.Obj(42); if (!(o == p) || o(0) != 42 || o(-2) != 40) return 8; }
    if (k.copy != 0 || k.move != 0 || k.dtor != 2) return 9;
    lib.It it; it.p = lib.data(); *it = 30; ++it; *it += 9; ++it; int s = *it; --it; --it;
    return *it + s - 30 + 39;
'''
c = Caso('x2-vida-operadores', H, S, **CPP)
positivo(c, BODY, modes=['directo', 'dll'])
# An early return destroys the local exactly once (the prelude function lives in the program: direct mode).
positivo(Caso('x2-retorno-temprano', H, S, **CPP),
         'lib.reset(); int r = Early(); ref lib.Counters k = ref lib.counters(); return k.ctor == 1 && k.dtor == 1 ? r : 1;',
         preludio=PRE, modes=['directo'])
rechazo(c, 'var o = new lib.Obj(1); return o("x");', "no C++ 'operator()'")
rechazo(c, 'var m = new lib.MoveOnly(1); var m2 = new lib.MoveOnly(in m); return 0;', 'constructor')
fin()
