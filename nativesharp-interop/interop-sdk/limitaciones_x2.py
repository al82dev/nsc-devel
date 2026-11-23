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
(con CRT, frente a Clang). Un prvalue (`lib.take_val(lib.make(42))`) se construye en el parametro que recibe y
destruye la funcion original (ABI Microsoft x64), sin copia; un objeto prestado con `in` es la copia construida ahi.
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
int take_two(Obj a, Obj b);
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
int take_two(Obj a, Obj b) { return a.v + b.v; }
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
# X2-05: a factory prvalue passed to a by-value parameter was a rejection ("no viable overload of namespace function
# 'lib.take_val'"): the by-value adapter would have copied it. It is now constructed in the temporary the original
# function receives and destroys (Microsoft x64), as C++ constructs the parameter in place: one constructor, no copy or
# move, one destructor (the callee's). A copy is written as a prvalue too (`new T(in o)`). With several such
# arguments each keeps its cleanup until the call (an exception while building a later one destroys it).
# X2-05: un prvalue de fabrica a un parametro por valor era un rechazo; ahora se construye en el temporal que recibe y
# destruye la funcion original, como C++ construye el parametro en su lugar.
BODY_PRVALUE = r'''
    lib.reset();
    ref lib.Counters k = ref lib.counters();
    if (lib.take_val(lib.make(40)) != 40) return 1;
    if (k.ctor != 1 || k.copy + k.move != 0 || k.dtor != 1) return 2;
    if (lib.take_val(new lib.Obj(1)) + lib.take_two(new lib.Obj(1), lib.make(1)) != 3) return 3;
    if (k.ctor != 4 || k.copy + k.move != 0 || k.dtor != 4) return 4;
    { var o = new lib.Obj(7); if (lib.take_val(new lib.Obj(in o)) != 7) return 5; }
    if (k.ctor != 5 || k.copy != 1 || k.move != 0 || k.dtor != 6) return 6;
    lib.reset();
    if (lib.take_mo(new lib.MoveOnly(42)) != 42 || k.ctor != 1 || k.move != 0 || k.dtor != 1) return 7;
    return 42;
'''
positivo(Caso('x2-prvalue-por-valor', H, S, **CPP), BODY_PRVALUE, modes=['directo', 'dll'])
# X2-05: mixing a prvalue with an object lent with `in` was a rejection ('is lent with'); the lent object is now the
# copy C++ constructs in its parameter (`ref m` the move of a move-only record), built in the temporary the callee
# destroys, so the by-value adapter is not needed: `take_two(in o, in p)`, which did not link without the CRT
# (`__CxxFrameHandler3` of the adapter), runs too.
# X2-05: mezclar un prvalue con un objeto prestado con `in` era un rechazo; el objeto prestado es ahora la copia que C++
# construye en su parametro, en el temporal que destruye el llamado.
BODY_LENT = r'''
    lib.reset();
    ref lib.Counters k = ref lib.counters();
    { var o = new lib.Obj(40); var p = new lib.Obj(2); if (lib.take_two(in o, in p) != 42) return 1;
      if (k.ctor != 2 || k.copy != 2 || k.move != 0 || k.dtor != 2) return 2; }
    if (k.dtor != 4) return 3;
    lib.reset();
    { var o = new lib.Obj(40); if (lib.take_two(in o, lib.make(2)) != 42) return 4;
      if (k.ctor != 2 || k.copy != 1 || k.move != 0 || k.dtor != 2) return 5; }
    lib.reset();
    { var m = new lib.MoveOnly(42); if (lib.take_mo(ref m) != 42 || m.v != -1) return 6;
      if (k.ctor != 1 || k.move != 1 || k.dtor != 1) return 7; }
    return k.dtor == 2 ? 42 : 8;
'''
positivo(Caso('x2-prestado-por-valor', H, S, **CPP), BODY_LENT, modes=['directo', 'dll'])
rechazo(c, 'var m = new lib.MoveOnly(1); var m2 = new lib.MoveOnly(in m); return 0;', 'constructor')
# X2-05: the forms that still need a non-local destination keep the former rejection's coverage.
# X2-05: las formas que aun necesitan un destino no local conservan la cobertura del rechazo anterior.
# X2-05: a NativeSharp function returning the record was a rejection ('requires direct local or discarded
# construction'); it now constructs the result in the caller's local (hidden pointer), also through another such
# function, with no copy or move. Returning a local of the function (NRVO) stays rejected.
# X2-05: una funcion NativeSharp que devuelve el record era un rechazo; ahora construye el resultado en el local del
# llamador, tambien a traves de otra funcion igual, sin copias. Devolver un local de la funcion sigue rechazado.
PRE_RET = ('lib.Obj Make(int k) { if (k < 0) return lib.make(-k); return lib.make(k); }\n'
           'lib.Obj Twice(int k) { return Make(k * 2); }\n')
BODY_RET = r'''
    lib.reset();
    ref lib.Counters k = ref lib.counters();
    {
        var o = Make(40);
        var p = Twice(-1);
        if (o.v + p.v != 42 || k.ctor != 2 || k.copy + k.move != 0 || k.dtor != 0) return 1;
    }
    if (k.dtor != 2) return 2;
    return 42;
'''
positivo(Caso('x2-retorno-ns', H, S, **CPP), BODY_RET, preludio=PRE_RET, modes=['directo'])
# X2-05: `return o;` of a C++ local was a rejection ('requires return move(value)'); it is now C++'s implicit move:
# the caller's result is move-constructed from o, which the function's scope destroys.
# X2-05: `return o;` era un rechazo; ahora es el movimiento implicito de C++.
BODY_NRVO = r'''
    lib.reset();
    ref lib.Counters k = ref lib.counters();
    {
        var o = Keep();
        if (o.v != 42 || k.ctor != 1 || k.move != 1 || k.copy != 0 || k.dtor != 1) return 1;
    }
    if (k.dtor != 2) return 2;
    return 42;
'''
positivo(Caso('x2-retorno-local', H, S, **CPP), BODY_NRVO,
         preludio='lib.Obj Keep() { var o = lib.make(42); return o; }\n', modes=['directo'])
# X2-05: `return move(o);` was a rejection ('cannot copy or transfer a C++ value'); it is the same move in C++, and
# the moved-from local is still destroyed. / Era un rechazo; es el mismo movimiento y el local movido se destruye.
positivo(Caso('x2-retorno-move', H, S, **CPP), BODY_NRVO,
         preludio='lib.Obj Keep() { var o = lib.make(42); return move(o); }\n', modes=['directo'])
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
    // Member operators on a temporary, as in C++. / Operadores miembro sobre un temporal, como en C++.
    int d = lib.make(1)(1) + *lib.make(0);
    if (d != 2 || k.ctor != 5 || k.copy + k.move != 0 || k.dtor != 5) return 3;
    return a + b;
'''
positivo(Caso('x2-temporal-receptor', H, S, **CPP), BODY_TEMP, modes=['directo', 'dll'])
# X2-05: `o = value` calls the operator= C++ selects. Obj declares a move constructor, so C++ deletes its implicit
# copy assignment: the reassignment stays rejected, now with that reason (it was 'cannot copy or transfer a C++ value').
# X2-05: `o = valor` llama al operator= que elige C++; el de Obj esta borrado, asi que sigue rechazado con ese motivo.
rechazo(c, 'var o = lib.make(1); o = lib.make(42); return o.v;', 'has no accessible assignment from an rvalue')
HA = r'''
namespace lib {
struct Counters2 { int ctor, copy, move, copyAssign, moveAssign, dtor; };
Counters2& counters2();
struct A {
  int v;
  A(int x);
  A(const A& o);
  A(A&& o);
  A& operator=(const A& o);
  A& operator=(A&& o);
  ~A();
};
A mk(int v);
}
'''
SA = r'''
namespace lib {
static Counters2 c2;
Counters2& counters2() { return c2; }
A::A(int x) : v(x) { ++c2.ctor; }
A::A(const A& o) : v(o.v) { ++c2.copy; }
A::A(A&& o) : v(o.v) { o.v = -1; ++c2.move; }
A& A::operator=(const A& o) { v = o.v; ++c2.copyAssign; return *this; }
A& A::operator=(A&& o) { v = o.v; o.v = -1; ++c2.moveAssign; return *this; }
A::~A() { ++c2.dtor; }
A mk(int v) { return A(v); }
}
'''
BODY_ASSIGN = r'''
    {
        var o = lib.mk(1);
        o = lib.mk(40);
        if (o.v != 40 || lib.counters2().ctor != 2 || lib.counters2().moveAssign != 1 || lib.counters2().dtor != 1)
            return 1;
        var p = lib.mk(2);
        o = p;
        if (o.v != 2 || p.v != 2 || lib.counters2().copyAssign != 1 || lib.counters2().dtor != 1) return 2;
    }
    if (lib.counters2().copy + lib.counters2().move != 0 || lib.counters2().dtor != 3) return 3;
    return 42;
'''
positivo(Caso('x2-asignacion', HA, SA, **CPP), BODY_ASSIGN, modes=['directo', 'dll'])
fin()
