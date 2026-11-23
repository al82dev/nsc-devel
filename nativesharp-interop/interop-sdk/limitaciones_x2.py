"""Limitaciones X2: clases, vida de objetos y operadores C++ (plan de limitaciones, tareas X2-01..X2-11).

Contadores de constructor, copia, movimiento y destructor en C++ verifican cada secuencia:
X2-02: construccion local y destruccion exactamente una vez al salir del bloque y en un retorno temprano.
X2-04: copia explicita `new T(in o)` (constructor de copia) y tipo move-only `new T(ref m)` (constructor de movimiento):
el origen movido tambien se destruye. Con constructor de copia presente, el de movimiento queda fuera (T11-016:
NativeSharp no sobrecarga ref frente a in).
X2-05: un record C++ no trivial por valor se pasa por un adaptador generado por Clang: `const T&` (`in o`, una copia
que destruye el llamado) o `T&&` si solo se mueve (`ref m`, un movimiento). Devolverlo por valor construye el resultado
en el local (`var o = lib.make(42)`: un constructor, ninguna copia ni movimiento, un destructor) y un resultado descartado
se construye y se destruye; las excepciones y los tipos move-only los verifica pruebas-x2-retorno-valor/validar.py
(con CRT, frente a Clang). Siguen rechazados: devolverlo desde una funcion NativeSharp, usarlo como temporal sin local
(`lib.make(1).v`) y asignarlo a un local ya construido.
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
int take_val(Obj o);
int take_mo(MoveOnly m);
Obj make(int v);
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
int take_val(Obj o) { return o.v; }
int take_mo(MoveOnly m) { return m.v; }
Obj make(int v) { return Obj(v); }
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
    lib.reset();
    { var o = new lib.Obj(42); if (lib.take_val(in o) != 42 || o.v != 42) return 10; }
    if (k.ctor != 1 || k.copy != 1 || k.dtor != 2) return 11;
    lib.reset();
    { var m = new lib.MoveOnly(42); if (lib.take_mo(ref m) != 42 || m.v != -1) return 12; }
    if (k.ctor != 1 || k.move != 1 || k.dtor != 2) return 13;
    lib.reset();
    { var o = lib.make(42); if (o.v != 42) return 14; }
    if (k.ctor != 1 || k.copy != 0 || k.move != 0 || k.dtor != 1) return 15;
    lib.reset();
    lib.make(7);
    if (k.ctor != 1 || k.copy + k.move != 0 || k.dtor != 1) return 16;
    lib.It it; it.p = lib.data(); *it = 30; ++it; *it += 9; ++it; int s = *it; --it; --it;
    return *it + s - 30 + 39;
'''
c = Caso('x2-vida-operadores', H, S, **CPP)
# A precompiled template cannot yet carry a non-trivial C++ record (catalog, R2): direct and DLL modes.
positivo(c, BODY, modes=['directo', 'dll'])
# An early return destroys the local exactly once (the prelude function lives in the program: direct mode).
positivo(Caso('x2-retorno-temprano', H, S, **CPP),
         'lib.reset(); int r = Early(); ref lib.Counters k = ref lib.counters(); return k.ctor == 1 && k.dtor == 1 ? r : 1;',
         preludio=PRE, modes=['directo'])
rechazo(c, 'var o = new lib.Obj(1); return o("x");', "no C++ 'operator()'")
rechazo(c, 'var m = new lib.MoveOnly(1); var m2 = new lib.MoveOnly(in m); return 0;', 'constructor')
# X2-05: the forms that still need a non-local destination keep the former rejection's coverage.
# X2-05: las formas que aun necesitan un destino no local conservan la cobertura del rechazo anterior.
rechazo(c, 'var o = Make(); return o.v;', 'requires direct local or discarded construction',
        preludio='lib.Obj Make() { return lib.make(42); }\n')
# X2-05: this temporary used as a receiver was a rejection ('cannot copy or transfer a C++ value'); it is now
# constructed in a hidden owner and destroyed at the end of the full expression (counters), as a C++ temporary.
# X2-05: este temporal receptor era un rechazo; ahora se construye en un dueno oculto y se destruye al final de la
# expresion completa.
BODY_TEMP = r'''
    lib.reset();
    ref lib.Counters k = ref lib.counters();
    int a = lib.make(40).v;
    if (k.ctor != 1 || k.copy + k.move != 0 || k.dtor != 1) return 1;
    int b = lib.make(1).v + lib.make(1).v;
    if (k.ctor != 3 || k.copy + k.move != 0 || k.dtor != 3) return 2;
    return a + b;
'''
positivo(Caso('x2-temporal-receptor', H, S, **CPP), BODY_TEMP, modes=['directo', 'dll'])
rechazo(c, 'var o = lib.make(1); o = lib.make(42); return o.v;', 'cannot copy or transfer a C++ value')
fin()
