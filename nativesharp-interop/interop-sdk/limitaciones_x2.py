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
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(c, BODY, modes=['directo', 'dll', 'lib'])
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
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-prvalue-por-valor', H, S, **CPP), BODY_PRVALUE, modes=['directo', 'dll', 'lib'])
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
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-prestado-por-valor', H, S, **CPP), BODY_LENT, modes=['directo', 'dll', 'lib'])
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
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-temporal-receptor', H, S, **CPP), BODY_TEMP, modes=['directo', 'dll', 'lib'])
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
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-asignacion', HA, SA, **CPP), BODY_ASSIGN, modes=['directo', 'dll', 'lib'])
# X2-06: a NativeSharp struct with a field of a C++ record that is a transport value (trivially copyable and
# destructible) was rejected ("has no imported object layout yet"); its bytes are its value, as for a C record, so the
# struct holds it, copies it and passes the field by value. A non-trivial C++ field stays a conscious restriction.
# X2-06: un struct NativeSharp con un campo C++ transportable se rechazaba; ahora lo contiene y lo copia por bytes.
HT = 'namespace lib { struct P { int x; int y; int sum() const { return x + y; } }; int psum(P p); }\n'
ST = 'namespace lib { int psum(P p) { return p.x + p.y; } }\n'
positivo(Caso('x2-campo-transportable', HT, ST, **CPP),
         'Holder h; h.p.x = 20; h.p.y = 18; h.k = 2; Holder g = h; g.p.y += 2;\n'
         '    return h.p.y == 18 ? lib.psum(g.p) + g.k : 1;',
         preludio='struct Holder { public lib.P p; public int k; }\n', modes=['directo', 'dll'])
# X2-06: a NativeSharp class (a reference, never copied) with fields of a non-trivial C++ record: each is constructed
# with the object as a C++ member is, in place by its initializer (`new T(...)` or a factory) or by default, in
# declaration order; a later `h.b = value` is its operator=; the object's destruction runs each field's destructor.
# A struct with such a field stays a restriction (it would need C++ copies).
# X2-06: una clase NativeSharp con campos de un record C++ no trivial: se construyen con el objeto como miembros C++ y se
# destruyen con el.
HF = r'''
namespace lib {
struct C2 { int ctor, dtor, assign; };
C2& c();
struct Obj { int v; Obj(int x); Obj(); Obj(const Obj& o); Obj& operator=(const Obj& o); Obj& operator=(Obj&& o); ~Obj(); };
Obj make(int v);
struct NoDefault { int v; NoDefault(int x); ~NoDefault(); };
}
'''
SF = r'''
namespace lib {
static C2 k; C2& c() { return k; }
Obj::Obj(int x) : v(x) { ++k.ctor; } Obj::Obj() : v(7) { ++k.ctor; } Obj::Obj(const Obj& o) : v(o.v) { ++k.ctor; }
Obj& Obj::operator=(const Obj& o) { v = o.v; ++k.assign; return *this; }
Obj& Obj::operator=(Obj&& o) { v = o.v; ++k.assign; return *this; }
Obj::~Obj() { ++k.dtor; }
Obj make(int v) { return Obj(v); }
NoDefault::NoDefault(int x) : v(x) {}
NoDefault::~NoDefault() {}
}
'''
BODYF = r'''
    {
        var h = new Holder();
        if (h.a.v != 30 || h.b.v != 7 || h.m.v != 1 || lib.c().ctor != 3) return 1;
        h.b = lib.make(5);
        if (h.b.v != 5 || lib.c().assign != 1 || lib.c().dtor != 1) return 2;
    }
    return lib.c().dtor == 4 && lib.c().ctor == 4 ? 42 : 3;
'''
positivo(Caso('x2-clase-campo-cpp', HF, SF, **CPP), BODYF,
         preludio='class Holder { public lib.Obj a = new lib.Obj(30); public lib.Obj b; public lib.Obj m = lib.make(1); }\n',
         modes=['directo', 'dll'])
rechazo(Caso('x2-clase-campo-cpp', HF, SF, **CPP), 'var h = new Holder(); return 0;', 'has no accessible default constructor',
        preludio='class Holder { public lib.NoDefault n; }\n')
# X2-06: a struct (a value: copied, moved, zero-initialized, held in arrays and generics) cannot hold a non-trivial C++
# record, which C++ does not allow to relocate bytewise; the diagnostic says so and points to a class field or a local
# (it read 'has no imported object layout yet'). / Un struct no puede contener un record C++ no trivial; el diagnostico
# lo explica y sugiere un campo de clase o un local.
rechazo(Caso('x2-struct-campo-cpp', HF, SF, **CPP), 'Pair p; return p.k;', 'is not trivially copyable and destructible',
        preludio='struct Pair { public lib.Obj a; public int k; }\n')
rechazo(Caso('x2-arreglo-cpp', HF, SF, **CPP), 'var xs = new lib.Obj[2]; return 0;',
        'is not trivially copyable and destructible')
# A tuple local holding one (`(lib.Obj, int) t;`) compiled, was zero-initialized and ran the C++ destructor on an object
# never constructed at scope end; it is rejected the same way. / Una tupla local con uno destruia un objeto nunca
# construido; se rechaza igual.
rechazo(Caso('x2-tupla-cpp', HF, SF, **CPP), '{ (lib.Obj, int) t; } return 0;',
        'is not trivially copyable and destructible')
# A static field of such a record reached the backend ('cannot map named value type to LLVM carrier'); nothing runs at
# process exit to destroy it, so it is rejected as a static owned field is. / Un campo estatico de ese record llegaba al
# backend; nada lo destruye al salir del proceso, asi que se rechaza como un estatico owned.
rechazo(Caso('x2-estatico-cpp', HF, SF, **CPP), 'return 0;', 'cannot hold C++ type',
        preludio='class K { public static lib.Obj s = new lib.Obj(4); }\n')
# X2-05: the constructed result of a NativeSharp instance method, property getter or virtual override (through a
# base reference, and returned again by another function) is built in the caller's local, and that of a NativeSharp
# function used as a receiver (`Make(4).v`) in a hidden owner destroyed at the end of the full expression. Before, only
# namespace functions into a local were accepted ('requires direct automatic construction', 'cannot copy or transfer').
# X2-05: el resultado construido de un metodo de instancia, getter u override virtual NativeSharp se construye en el
# local del llamador, y el de una funcion usada como receptor en un dueno oculto destruido al final de la expresion.
PRE_INST = ('lib.Obj Make(int k) { return lib.make(k); }\n'
       'class H2 { lib.Obj f = new lib.Obj(7); public lib.Obj G() { return new lib.Obj(in f); }\n'
       '  public lib.Obj P { get { return new lib.Obj(in f); } } public virtual lib.Obj V() { return lib.make(3); } }\n'
       'class H3 : H2 { public override lib.Obj V() { return lib.make(8); } }\n'
       'lib.Obj Fwd(H2 h) { return h.V(); }\n')
BODY_INST = r'''
    {
        var h = new H2();
        if (Make(4).v != 4) return 1;
        if (lib.c().dtor != 1 || lib.c().ctor != 2) return 2;
        var x = h.G();
        var y = h.P;
        if (x.v + y.v != 14 || lib.c().ctor != 4) return 3;
        H2 b = new H3();
        var v = b.V();
        var w = Fwd(b);
        if (v.v + w.v != 16 || lib.c().ctor != 7 || lib.c().dtor != 1) return 4;
    }
    return lib.c().ctor == 7 && lib.c().dtor == 7 && lib.c().assign == 0 ? 42 : 5;
'''
positivo(Caso('x2-resultado-metodo', HF, SF, **CPP), BODY_INST, preludio=PRE_INST, modes=['directo'])
# An automatic property of such a record would copy its backing field on every get; it failed later with no location
# ('experimental C++ local cannot copy'). It is rejected at the property, which names the alternatives (a getter that
# returns an explicit copy works, as above). / Una propiedad automatica de ese record se rechaza en la propiedad.
rechazo(Caso('x2-propiedad-auto-cpp', HF, SF, **CPP), 'return 0;', 'automatic property',
        preludio='class K { public lib.Obj P { get; set; } }'+chr(10))
# X2-05: assigning such a result to a C++ local (`x = h.G()`, `x = Make(5)`) is the record's operator= from a
# temporary that owns the result ('call result is not addressable unless the selected method returns ref' before).
# Asignar ese resultado a un local C++ es el operator= desde un temporal que posee el resultado.
positivo(Caso('x2-resultado-asignacion', HF, SF, **CPP),
         '{ var h = new H2(); var x = Make(1); x = h.G(); if (x.v != 7) return 1; x = Make(5); if (x.v != 5) return 2;'
         ' if (lib.c().assign != 2) return 3; }'
         ' return lib.c().ctor == lib.c().dtor && lib.c().ctor == 4 ? 42 : 4;', preludio=PRE_INST, modes=['directo'])
# X2-05: a C++ record prvalue (`lib.make(3)`, `new lib.Obj(4)`, a NativeSharp `Make(5)`) binds a `const T&`
# parameter as in C++, materialized for the call with no copy and destroyed at the end of the full expression; a
# place still binds with `in` (X1-04 left records out: an lvalue would need a copy). Namespace functions and member
# functions, directo and dll. / Un prvalue record C++ enlaza un parametro `const T&` como en C++, sin copia; un lugar
# sigue enlazandose con `in`.
HF2 = HF.replace('Obj make(int v);', 'Obj make(int v); int peek(const Obj& o); struct Cmp { int base; int diff(const Obj& o) const; };')
SF2 = SF.replace('Obj make(int v) { return Obj(v); }', 'Obj make(int v) { return Obj(v); } int peek(const Obj& o) { return o.v; } int Cmp::diff(const Obj& o) const { return o.v - base; }')
PRE_CR = 'lib.Obj Make(int k) { return lib.make(k); }\n'
BODY_CR = r'''
    if (lib.peek(lib.make(3)) != 3) return 1;
    if (lib.c().ctor != 1 || lib.c().dtor != 1) return 2;
    if (lib.peek(new lib.Obj(4)) != 4) return 3;
    if (lib.peek(Make(5)) != 5) return 4;
    { var x = Make(6); if (lib.peek(in x) != 6) return 5; }
    return lib.c().ctor == 4 && lib.c().dtor == 4 && lib.c().assign == 0 ? 42 : 6;
'''
positivo(Caso('x2-constref-prvalue', HF2, SF2, **CPP), BODY_CR, preludio=PRE_CR, modes=['directo'])
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-constref-miembro', HF2, SF2, **CPP),
         'lib.Cmp k; k.base = 1; int d = k.diff(lib.make(43)); return d == 42 && lib.c().ctor == 1 && lib.c().dtor == 1 ? 42 : 1;',
         modes=['directo', 'dll', 'lib'])
rechazo(Caso('x2-constref-lugar', HF2, SF2, **CPP), 'var x = lib.make(1); return lib.peek(x);', 'lib.peek')
# X2-05: a C++ method returning a non-trivial record by value was not imported ('no compatible member'). It now
# constructs the result in the caller's destination, passed after this as MSVC does: a non-virtual method directly, a
# virtual one through its adapter, which constructs the prvalue at that storage (guaranteed elision); into a local, as
# a receiver temporary, and dispatched to a C++ override through a base pointer, with no copy.
# X2-05: un metodo C++ que devuelve un record no trivial por valor construye el resultado en el destino del llamador.
HMR = HF.replace('Obj make(int v);', 'Obj make(int v);\nstruct Maker { int base; Obj create(int v) const; virtual Obj vcreate(int v); };\nstruct Twice : Maker { Obj vcreate(int v) override; };\nMaker* twice();\nvoid drop(Maker* m);')
SMR = SF.replace('Obj make(int v) { return Obj(v); }', 'Obj make(int v) { return Obj(v); }\nObj Maker::create(int v) const { return Obj(base + v); }\nObj Maker::vcreate(int v) { return Obj(base * v); }\nObj Twice::vcreate(int v) { return Obj(2 * v); }\n')
BODY_MR = r'''
    lib.Maker m = new lib.Maker(); m.base = 2;
    {
        var a = m.create(40);
        var b = m.vcreate(21);
        if (a.v != 42 || b.v != 42 || m.create(1).v != 3) return 1;
        lib.Twice tw = new lib.Twice(); tw.base = 0; lib.Maker* t = &tw;
        var c = t->vcreate(21);
        if (c.v != 42) return 2;
                if (lib.c().ctor != 4 || lib.c().dtor != 1) return 3;
    }
    return lib.c().ctor == 4 && lib.c().dtor == 4 ? 42 : 4;
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-metodo-resultado', HMR, SMR, extra_ref=' Rtti="false"', **CPP), BODY_MR, modes=['directo', 'dll', 'lib'])
# X2-05: a static C++ method returning a non-trivial record by value (not imported before) constructs it in the
# caller's destination, through the first hidden pointer as a free function. / Un metodo estatico C++ que devuelve un
# record no trivial por valor lo construye en el destino del llamador.
HSM = HF.replace('Obj make(int v);', 'Obj make(int v);'+chr(10)+'struct Maker2 { static Obj build(int v); };')
SSM = SF.replace('Obj make(int v) { return Obj(v); }', 'Obj make(int v) { return Obj(v); }'+chr(10)+'Obj Maker2::build(int v) { return Obj(v); }')
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-estatico-resultado', HSM, SSM, **CPP),
         '{ var o = lib.Maker2.build(42); if (o.v != 42) return 1; if (lib.Maker2.build(3).v != 3) return 2; }'
         ' return lib.c().ctor == 2 && lib.c().dtor == 2 ? 42 : 3;', modes=['directo', 'dll', 'lib'])
# X2-05: a free C++ operator returning a non-trivial record by value (`Obj operator+(const Obj&, const Obj&)`, as
# std::string's) failed in the backend ('static call target was not declared'): the operator call is now the
# construction of its record with the factory constructor, as a direct factory call is, into a local or as a
# receiver temporary, with no copy. / Un operador C++ libre que devuelve un record no trivial por valor.
HOP = HF.replace('Obj make(int v);', 'Obj make(int v);'+chr(10)+'Obj operator+(const Obj& a, const Obj& b);')
SOP = SF.replace('Obj make(int v) { return Obj(v); }', 'Obj make(int v) { return Obj(v); }'+chr(10)+
                 'Obj operator+(const Obj& a, const Obj& b) { return Obj(a.v + b.v); }')
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-operador-resultado', HOP, SOP, **CPP),
         '{ var a = lib.make(40); var b = lib.make(2); var c = a + b; if (c.v != 42) return 1;'
         ' if ((a + lib.make(1)).v != 41) return 2; if (lib.c().ctor != 5 || lib.c().dtor != 2) return 3; }'
         ' return lib.c().ctor == 5 && lib.c().dtor == 5 ? 42 : 4;', modes=['directo', 'dll', 'lib'])
# X2-05: a member operator returning such a record (`Obj Vec::operator*(int) const`) was not found ('requires
# compatible numeric operands'); its adapter returns the prvalue into the caller's storage, as a factory's does.
# Un operador miembro que devuelve ese record: su adaptador lo construye en el almacenamiento del llamador.
HOPM = HF.replace('Obj make(int v);', 'Obj make(int v);'+chr(10)+'struct Vec { int x; Obj operator*(int k) const; };')
SOPM = SF.replace('Obj make(int v) { return Obj(v); }', 'Obj make(int v) { return Obj(v); }'+chr(10)+
                  'Obj Vec::operator*(int k) const { return Obj(x * k); }')
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-operador-miembro-resultado', HOPM, SOPM, **CPP),
         '{ lib.Vec w; w.x = 21; var c = w * 2; if (c.v != 42 || (w * 1).v != 21) return 1; }'
         ' return lib.c().ctor == 2 && lib.c().dtor == 2 ? 42 : 2;', modes=['directo', 'dll', 'lib'])
# X2: `(int)h` of an imported C/C++ record with no conversion reached the backend ('unsupported Stage 34 LLVM
# conversion'); it is a frontend diagnostic now. / Un cast de un record importado sin conversion es un diagnostico.
rechazo(Caso('x2-cast-record', HF, SF, **CPP), 'var o = lib.make(3); return (int)o;', 'no conversion from C/C++ type')
# X2: a C++ conversion operator to a builtin value (`explicit operator bool()`, `operator int()`) was ignored; it is
# now the explicit NativeSharp conversion `(T)o`, a static `op_Explicit(in Self)` of the record implemented by its
# member adapter, on a place or a temporary (a non-trivial record too). C++'s implicit conversions stay rejected.
# X2: un operador de conversion C++ es la conversion explicita `(T)o`; las implicitas de C++ siguen rechazadas.
HCV = r'''
namespace lib {
struct Handle { int v; explicit operator bool() const; operator int() const; };
struct Owner { int* p; Owner(int x); ~Owner(); explicit operator bool() const; };
Owner make_owner(int x);
}
'''
SCV = r'''
namespace lib {
static int store[4];
Handle::operator bool() const { return v != 0; } Handle::operator int() const { return v * 2; }
Owner::Owner(int x) : p(x ? &store[x] : nullptr) {} Owner::~Owner() {} Owner::operator bool() const { return p != nullptr; }
Owner make_owner(int x) { return Owner(x); }
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-conversion', HCV, SCV, **CPP), r'''
    lib.Handle h; h.v = 21;
    if (!(bool)h) return 1;
    lib.Handle z; z.v = 0;
    if ((bool)z) return 2;
    var o = lib.make_owner(1);
    if (!(bool)o || (bool)lib.make_owner(0)) return 3;
    return (int)h;
''', modes=['directo', 'dll', 'lib'])
rechazo(Caso('x2-conversion-implicita', HCV, SCV, **CPP), 'lib.Handle h; h.v = 21; int k = h; return k;', 'cannot initialize')
# X2-05: with `const T&` binding prvalues, `take(const Obj&)` and `take(Obj&&)` were ambiguous for `take(new Obj(2))`;
# as in C++, a record prvalue binds `T&&` better. / Como en C++, un prvalue record enlaza mejor `T&&`.
HRK = r'''
namespace lib {
struct Obj { int v; Obj(int x) : v(x) {} Obj(const Obj& o) : v(o.v) {} Obj(Obj&& o) : v(o.v) { o.v = 0; } ~Obj() {} };
int take(const Obj& o);
int take(Obj&& o);
}
'''
SRK = 'namespace lib { int take(const Obj& o) { return 1; } int take(Obj&& o) { return 2; } }'+chr(10)
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-rvalue-preferido', HRK, SRK, **CPP),
         'var o = new lib.Obj(1); return lib.take(in o) * 10 + lib.take(new lib.Obj(2)) == 12 ? 42 : 1;', modes=['directo', 'lib'])
# X2: foreach over a C++ range (a record with begin() and end(), as std::vector is) failed ('no compatible member
# named GetEnumerator'); it iterates as a C++ range-for does, with a pointer iterator or a class iterator's C++ `!=`,
# `*` and prefix `++`. A mutable reference to `*it` would write a copy, so `foreach ref mut` is rejected.
# X2: foreach sobre un rango C++ (begin()/end()) se recorre como un range-for de C++; `ref mut` se rechaza.
HRG = r'''
namespace lib {
struct It { int* p; int operator*() const; It& operator++(); bool operator!=(const It& o) const; };
struct Bag { int data[4]; It begin(); It end(); };
struct Vec3 { int data[3]; int* begin() { return data; } int* end() { return data + 3; } };
}
'''
SRG = r'''
namespace lib {
int It::operator*() const { return *p; } It& It::operator++() { ++p; return *this; } bool It::operator!=(const It& o) const { return p != o.p; }
It Bag::begin() { return It{data}; } It Bag::end() { return It{data + 4}; }
}
'''
CRG = dict(language='C++', standard='c++20', extra_ref=' Exceptions="false"')
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-foreach-rango', HRG, SRG, **CRG),
         'lib.Bag b; b.data[0] = 10; b.data[1] = 12; b.data[2] = 15; b.data[3] = 5; int s = 0; foreach (var x in b) { s += x; }'
         ' lib.Vec3 v; v.data[0] = 0; v.data[1] = 0; v.data[2] = 0; foreach (var y in v) { s += y; } return s;',
         modes=['directo', 'dll', 'lib'])
rechazo(Caso('x2-foreach-rango-ref-mut', HRG, SRG, **CRG), 'lib.Vec3 v; foreach (ref mut var x in v) { x = 1; } return 0;',
        'foreach ref mut cannot iterate a C++ range')
# X2: an operator a base declares is the derived record's too (`D == D` through the base's `operator==`), and in
# C++20 `a != b` is `!(a == b)` when no `!=` is declared, as std iterators rely on; both were not found.
# X2: el operador de una base tambien es del derivado, y en C++20 `!=` se reescribe desde `==`.
HIH = r'''
namespace lib {
struct B { int* p; bool operator==(const B& o) const; };
struct D : B { D& operator++(); int& operator*() const; };
struct Bag { int data[3]; D begin(); D end(); };
}
'''
SIH = r'''
namespace lib {
bool B::operator==(const B& o) const { return p == o.p; }
D& D::operator++() { ++p; return *this; } int& D::operator*() const { return *p; }
D Bag::begin() { D d; d.p = data; return d; } D Bag::end() { D d; d.p = data + 3; return d; }
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-operador-heredado', HIH, SIH, **CRG),
         'lib.Bag b; b.data[0] = 40; b.data[1] = 1; b.data[2] = 1; lib.B x; x.p = null; lib.B y; y.p = null;'
         ' var e = b.end(); var i = b.end(); if (x != y || !(i == e)) return 1; int s = 0; foreach (var v in b) { s += v; }'
         ' return s;', modes=['directo', 'dll', 'lib'])
# X2: C++ defines no implicit equality for a class; `a == b` on a C++ record without an operator compared its bytes
# (or failed internally). It is rejected. / C++ no define igualdad implicita para una clase: se rechaza.
rechazo(Caso('x2-igualdad-implicita', 'namespace lib { struct P { int x; int y; }; }'+chr(10), '', **CRG),
        'lib.P a; a.x = 1; a.y = 2; lib.P b; b.x = 1; b.y = 2; return a == b ? 1 : 0;', "C++ type 'lib.P' has no operator==")
# X2: a member function template of a C++ record called with deduced arguments (`m.add(40)`) had no member; as for a
# free function template (T13-005) Clang deduces and instantiates it, and the specialization is called as an
# extension method of the record through its member adapter. A member the record lacks keeps its diagnostic.
# X2: una plantilla de funcion miembro con argumentos deducidos se instancia en Clang y se llama como extension.
HMT = r'''
namespace lib {
struct M { int base; template <typename T> T add(T v) const { return (T)base + v; } template <typename T> T put(T v) { base = (int)v; return v; } };
}
'''
positivo(Caso('x2-plantilla-miembro', HMT, '', **CRG),
         'lib.M m; m.base = 2; m.put(30); double d = m.add(1.5); int i = m.add(10); return i + (int)(d * 2.0) - 61;',
         modes=['directo', 'dll'])
rechazo(Caso('x2-plantilla-miembro-ausente', HMT, '', **CRG), 'lib.M m; return m.nada(1);',
        "has no compatible member named 'nada'")
# X2: a static member function template (`M.twice(21)`) is deduced the same way and called directly by its
# symbol, as a static extension of its own record only (another record does not get it).
# X2: una plantilla miembro estatica se deduce igual y es extension estatica solo de su propio record.
HMS = r'''
namespace lib {
struct M { int base; template <typename T> static T twice(T v) { return v * 2; } };
struct N { int z; };
}
'''
positivo(Caso('x2-plantilla-miembro-estatica', HMS, '', **CRG), 'return lib.M.twice(20) + (int)lib.M.twice(1.0);',
         modes=['directo', 'dll'])
rechazo(Caso('x2-plantilla-miembro-estatica-otro', HMS, '', **CRG),
        'int a = lib.M.twice(20); return lib.N.twice(21);', "has no compatible member named 'twice'")
# X2: explicit template arguments for a C++ member function template (`m.add<double>(1)`, `m.size<double>()`,
# `M.half<double>(5)`) failed in NativeSharp's generic phase ('no visible Stage 44 generic method template'). A member
# call whose name no NativeSharp generic method has is left to the body analysis; Clang deduces with the explicit
# arguments and the call takes exactly that specialization (by Clang USR), even where another one matches better.
# X2: argumentos de plantilla explicitos para una plantilla miembro C++; la llamada toma exactamente esa especializacion.
HEX = r'''
namespace lib {
struct M { int base; template <typename T> T add(T v) const { return (T)base + v; } template <typename T> int size() const { return (int)sizeof(T) + base; } template <typename T> static T half(T v) { return v / 2; } };
}
'''
positivo(Caso('x2-plantilla-miembro-explicita', HEX, '', **CRG),
         'lib.M m; m.base = 2; int a = m.add(1); double d = m.add<double>(1); m.base = 1; int s = m.size<double>();'
         ' return (int)(d * 10.0) + a + s - 9 + (int)(lib.M.half<double>(5) * 2.0) + lib.M.half(10) - 1;',
         modes=['directo', 'dll'])
rechazo(Caso('x2-plantilla-miembro-explicita-ausente', HEX, '', **CRG), 'lib.M m; return m.nada<int>(1);',
        "has no compatible member named 'nada'")
# X2: a constructor taking `const char* const` (a top-level const by-value pointer, as std::string's) was not
# imported ('lifetime method parameter type is not yet mapped'); it is, and takes a literal as a function does.
# A method returning a reference to its record (`T& add(...)`) used as a statement is no longer loaded (copied).
# X2: constructor con `const char* const` y resultado por referencia descartado como sentencia.
HCS = r'''
namespace lib {
struct Txt { int n; Txt(const char* const s) : n(0) { while (s[n]) ++n; } Txt(const Txt& o) : n(o.n) {} ~Txt() {}
  Txt& add(int k) { n += k; return *this; } };
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-constructor-puntero-const', HCS, '', **CRG),
         'lib.Txt t = new lib.Txt("hola"); t.add(30).add(8); return t.n;', modes=['directo', 'dll', 'lib'])
# X2: a free operator function template of a record's namespace (`template <class T> bool operator==(const Box<T>&,
# const Box<T>&)`) is deduced from the operands as a call's (T13-005), and C++20's `!=` is rewritten from it (an
# adapter negating `==`). / Plantilla de operador libre deducida de los operandos y `!=` reescrito de C++20.
HOT = r'''
namespace lib {
template <class T> struct Box { T v; };
template <class T> bool operator==(const Box<T>& a, const Box<T>& b) { return a.v == b.v; }
struct Use { Box<int> a; };
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-operador-plantilla-libre', HOT, '', **CRG),
         'lib.Use u; u.a.v = 3; lib.Use w; w.a.v = 4; lib.Use z; z.a.v = 3;'
         ' return (u.a == z.a ? 40 : 0) + (u.a != w.a ? 2 : 0) + (u.a != z.a ? 100 : 0);', modes=['directo', 'dll', 'lib'])
# X2: C++20's `a < b` (and `<=`, `>`, `>=`) rewritten from a free `operator<=>` template, an adapter comparing its
# result with 0. / Relacionales de C++20 reescritos desde una plantilla `operator<=>` libre.
HSS = r'''
namespace lib {
template <class T> struct Box { T v; };
template <class T> int operator<=>(const Box<T>& a, const Box<T>& b) { return a.v < b.v ? -1 : a.v > b.v ? 1 : 0; }
struct Use { Box<int> a; };
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-relacional-reescrito', HSS, '', **CRG),
         'lib.Use u; u.a.v = 3; lib.Use w; w.a.v = 4;'
         ' return (u.a < w.a ? 1 : 0) + (u.a <= w.a ? 2 : 0) + (u.a > w.a ? 4 : 0) + (w.a >= u.a ? 8 : 0) + 31;',
         modes=['directo', 'dll', 'lib'])
# X2: overloads by `const T&` and `T&&` of a C++ method or indexer (std::map's `operator[]`): an lvalue binds the
# `const T&`, a prvalue prefers the `T&&` (any T, not only records), and a C++ indexer's reference parameters bind
# like an operator's (they were ambiguous or not viable). / Sobrecargas `const T&`/`T&&` de metodos e indexadores.
HRR = r'''
namespace lib {
struct M { int v[4]; int& at(const int& k) { return v[k]; } int& at(int&& k) { return v[k + 0]; }
  int& operator[](const int& k) { return v[k]; } int& operator[](int&& k) { return v[k]; } };
struct N { int v[4]; int& operator[](const int& k) { return v[k]; } };
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-referencias-sobrecarga', HRR, '', **CRG),
         'lib.M m; int k = 1; m.at(k) = 30; m.at(2) = 2; m[3] = 4; lib.N n; n[k] = 6;'
         ' return m.at(k) + m.at(2) + m[3] + n[k];', modes=['directo', 'dll', 'lib'])
# X2: std::set's insert (`using _Mybase::insert;` of a `template <bool _Multi2 = _Multi, ...>` member returning a
# non-trivial pair) was not imported. A `using Base::name;` brings the base's members back next to the record's own,
# a member template whose parameters all have defaults is instantiated with them, a same-arity overload C++ could
# not select for a projected one's arguments (`put(Handle&&)` vs `put(const int&)`, checked by Clang) no longer
# blocks it, and a discarded non-trivial C++ result is destroyed at the end of its statement (exactly once).
# X2: `using Base::name`, plantillas miembro con parametros por defecto, sobrecargas no viables y resultado descartado.
HUS = r'''
namespace lib {
extern int g_dtors;
struct R { int v; R(int x) : v(x) {} R(const R& o) : v(o.v) {} ~R() { ++g_dtors; } };
struct Handle { int h; Handle(Handle&& o) : h(o.h) {} };
struct Base { int n; template <bool B = true, int I = 0> R put(const int& x) { n += x; return R(n); } int count() const { return n; } };
struct Derived : Base { using Base::put; int put(Handle&& h) { return h.h; } };
inline int dtors() { return g_dtors; }
}
'''
SUS = 'namespace lib { int g_dtors; }'+chr(10)
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-using-base-plantilla-defecto', HUS, SUS, **CRG),
         'lib.Derived d; d.n = 0; int a = 40; int b = 2; d.put(in a); d.put(in b); return d.count() + lib.dtors() - 2;',
         modes=['directo', 'dll', 'lib'])
# X2: ref-qualified and const overloads of a C++ method (std::optional's `value() &` / `const&` / `&&`) were dropped
# as ambiguous or blocked; they are kept apart by their qualifier and selected as C++ does: on a named non-const
# receiver the non-const `&` one. / Sobrecargas con calificador de referencia y const, elegidas como en C++.
HRQ = r'''
namespace lib {
struct Box { int v; int& get() & { return v; } const int& get() const& { return v; } int get() && { return v + 1000; }
  int get() const&& { return v + 2000; } };
inline Box make(int x) { Box b; b.v = x; return b; }
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-calificadores-receptor', HRQ, '', **CRG),
         'lib.Box b; b.v = 1; b.get() = 40; return b.get() + lib.make(2).get() - 1000;', modes=['directo', 'dll', 'lib'])
# X2: a record with one constructor NativeSharp cannot map (a pointer to member) lost every constructor and its
# destructor (std::optional's `optional(nullopt_t)`); an unprojected constructor C++ could not select for a
# projected one's arguments (checked by Clang) no longer discards them.
# X2: un constructor no mapeable ya no oculta los demas constructores ni el destructor.
HCB = r'''
namespace lib {
extern int g_wdtors;
struct W { int v; W(int x) : v(x) {} W(int (W::*)() const) : v(0) {} W(const W& o) : v(o.v) {} ~W() { ++g_wdtors; }
  int get() const { return v; } };
inline int wdtors() { return g_wdtors; }
}
'''
SCB = 'namespace lib { int g_wdtors; }'+chr(10)
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x2-constructor-no-mapeable', HCB, SCB, **CRG),
         'int r = 0; { lib.W w = new lib.W(41); r = w.get(); } return r + lib.wdtors();', modes=['directo', 'dll', 'lib'])
# EN: NSMeta X4-06. A published template that calls public NativeSharp functions returning a non-trivial C++ record by
#     value (constructed in place) or passing a prvalue to `const T&`, for a consumer without the header: the record
#     is declared by the producer's source session. / Una plantilla publicada que llama funciones NativeSharp publicas
#     que devuelven un record C++ no trivial por valor, para un consumidor sin encabezado.
PRE_RET_PUB = ('namespace SdkProbe { public lib.Obj Make(int k) { if (k < 0) return lib.make(-k); return lib.make(k); }\n'
               'public lib.Obj Twice(int k) { return Make(k * 2); } }\n')
# dll: the consumer generates only the member adapters its calls reach (it does not link the producer's C++ library).
#     dll: el consumidor genera solo los adaptadores que alcanzan sus llamadas.
positivo(Caso('x2-retorno-ns-publico', H, S, **CPP), BODY_RET, preludio=PRE_RET_PUB, modes=['lib', 'dll'])
PRE_CR_PUB = 'namespace SdkProbe { public lib.Obj Make(int k) { return lib.make(k); } }\n'
positivo(Caso('x2-constref-prvalue-publico', HF2, SF2, **CPP), BODY_CR, preludio=PRE_CR_PUB, modes=['lib', 'dll'])
fin()
