"""Limitaciones X3: herencia, dispatch, RTTI y COM (plan de limitaciones, tareas X3-01..X3-07).

Complementa pruebas-tanda-12-herencia (bases, ajuste de this, static_cast, dispatch virtual, destruccion por la base,
base virtual, typeid y dynamic_cast en la biblioteca con --dll), que pasa con el binario actual:
X3-01/X3-02: herencia multiple con la segunda base en offset distinto de cero: conversion derivado->base y referencia
a la base.
X3-04: dispatch virtual desde NativeSharp sobre un objeto C++ (herencia simple y multiple con la base polimorfica no
primaria); una clase NativeSharp implementa una interfaz C++ abstracta (adaptador de Clang, `(ICalc*)obj`).
X3-06: punteros a miembro rechazados con su motivo (su representacion depende del modelo de herencia, T14-007).
X3-07: COM consumido y entrante: interfaz abstracta `__stdcall` con AddRef/Release/QueryInterface por su vtable,
tambien implementada por una clase NativeSharp y llamada desde C++.
X3-03: un record con bases virtuales se construye y destruye por adaptadores de Clang (indicador de mas derivado, destructor de bases virtuales).
"""
import comun
from comun import Caso, positivo, rechazo, fin

CPP = dict(language='C++', standard='c++20', clang_args=['-fno-exceptions', '-fno-rtti', '-fno-threadsafe-statics'])

H1 = r'''
namespace lib {
struct A { int a = 10; int geta() const { return a; } };
struct B { int b = 20; int getb() const { return b; } };
struct D : A, B { int d = 12; int sum() const { return a + b + d; } };
int take_b(const B* p);
int take_a(A& r);
}
'''
S1 = 'namespace lib { int take_b(const B* p) { return p->b; } int take_a(A& r) { return r.a; } }\n'
positivo(Caso('x3-bases', H1, S1, **CPP),
         'lib.D d = new lib.D(); if (d.sum() != 42 || d.geta() != 10 || d.getb() != 20) return 1;\n'
         '    return lib.take_b(&d) + lib.take_a(ref d) + 12;', modes=['directo', 'dll'])

H2 = r'''
namespace lib {
struct Shape { virtual int area() const; virtual int scale(int k) const; int tag = 2; };
struct Sq : Shape { int s; Sq(int x); int area() const override; int scale(int k) const override; };
struct L { int l = 5; virtual int lv() const; };
struct Multi : L, Sq { Multi(int x); int area() const override; };
Shape* make_sq(int s);
Shape* make_multi(int s);
}
'''
S2 = r'''
namespace lib {
int Shape::area() const { return 0; }
int Shape::scale(int k) const { return k; }
Sq::Sq(int x) : s(x) {}
int Sq::area() const { return s * s; }
int Sq::scale(int k) const { return s * k; }
int L::lv() const { return l; }
Multi::Multi(int x) : Sq(x) {}
int Multi::area() const { return 100 + s; }
Shape* make_sq(int s) { static Sq q(0); q.s = s; return &q; }
Shape* make_multi(int s) { static Multi m(0); m.s = s; return &m; }
}
'''
positivo(Caso('x3-virtual', H2, S2, **CPP),
         'var s = lib.make_sq(6); int r = s->area() + s->tag + s->scale(1) - 6 + 4;\n'
         '    var m = lib.make_multi(1); if (m->area() != 101 || m->scale(3) != 3) return 1;\n'
         '    return r;', modes=['directo', 'dll'])

H3 = r'''
typedef long HRESULT;
typedef unsigned long ULONG;
struct GUID_ { unsigned long d1; unsigned short d2, d3; unsigned char d4[8]; };
struct IUnk {
  virtual HRESULT __stdcall QueryInterface(const GUID_& iid, void** out) = 0;
  virtual ULONG __stdcall AddRef() = 0;
  virtual ULONG __stdcall Release() = 0;
};
struct ICalc : IUnk { virtual HRESULT __stdcall Add(int a, int b, int* out) = 0; };
extern "C" HRESULT CreateCalc(ICalc** out);
extern "C" ULONG LiveRefs();
struct Pt { int x; };
int Pt_x(int Pt::* m, Pt* p);
'''
S3 = r'''
struct Calc : ICalc {
  ULONG refs = 1;
  HRESULT __stdcall QueryInterface(const GUID_&, void** out) override { *out = this; AddRef(); return 0; }
  ULONG __stdcall AddRef() override { return ++refs; }
  ULONG __stdcall Release() override { return --refs; }
  HRESULT __stdcall Add(int a, int b, int* out) override { *out = a + b; return 0; }
};
static Calc calc;
extern "C" HRESULT CreateCalc(ICalc** out) { calc.refs = 1; *out = &calc; return 0; }
extern "C" ULONG LiveRefs() { return calc.refs; }
int Pt_x(int Pt::* m, Pt* p) { return p->*m; }
'''
c3 = Caso('x3-com', H3, S3, **CPP)
positivo(c3, 'ICalc* c = null; if (CreateCalc(&c) != 0) return 1; c->AddRef(); int r = 0;\n'
             '    GUID_ g; void* q = null; if (c->QueryInterface(in g, &q) != 0 || q == null) return 4; c->Release();\n'
             '    if (c->Add(40, 2, &r) != 0) return 2; c->Release(); c->Release(); return LiveRefs() == 0 ? r : 3;',
         modes=['directo', 'dll'])
rechazo(c3, 'Pt p; p.x = 4; return Pt_x(null, &p);', 'pointer to member')
# X3-04/X3-07: `class MyCalc : ICalc` was a rejection ('cannot derive from the C/C++ record'). A NativeSharp class now
# implements an abstract C++ interface (polymorphic, only its vtable pointer, no virtual bases or virtual destructor,
# pure virtual methods with distinct names, COM included): Clang compiles an adapter deriving from the interface
# whose overrides call the class's methods through their C boundaries, held in two hidden fields of the object;
# `(ICalc*)obj` binds it and yields the pointer C++ calls. The object's lifetime stays NativeSharp's.
# X3-04/X3-07: una clase NativeSharp implementa ahora una interfaz C++ abstracta (COM incluido) mediante un adaptador
# que compila Clang; `(ICalc*)obj` entrega el puntero que llama C++.
SCOM = S3 + r'''
extern "C" int UseCalc(ICalc* c)
{
    int r = 0;
    if (c->Add(40, 2, &r) != 0) return 1;
    c->AddRef();
    if (c->Release() != 1) return 2;
    void* q = nullptr; GUID_ g{};
    if (c->QueryInterface(g, &q) != 0 || q != c) return 3;
    return c->Release() == 1 ? r : 4;
}
'''
HCOM = H3 + 'extern "C" int UseCalc(ICalc* c);\n'
PRECOM = r'''
class MyCalc : ICalc
{
    public uint refs = 1;
    public int QueryInterface(in GUID_ iid, void** out) { *out = (void*)(ICalc*)this; AddRef(); return 0; }
    public uint AddRef() { refs += 1; return refs; }
    public uint Release() { refs -= 1; return refs; }
    public int Add(int a, int b, int* out) { *out = a + b; return 0; }
}
'''
positivo(Caso('x3-com-entrante', HCOM, SCOM, **CPP), 'var c = new MyCalc(); return UseCalc((ICalc*)c);',
         preludio=PRECOM, modes=['directo', 'dll'])
# A class must implement every pure virtual method with its NativeSharp signature. / Debe implementar cada metodo
# virtual puro con su firma NativeSharp.
rechazo(c3, 'return 0;', "it must declare one public instance method 'QueryInterface'", preludio='class MyCalc : ICalc { }\n')
# X3-01: deriving from `lib.Shape` (data with a trivial destructor, virtual methods with a body) was a rejection
# ('cannot derive from the C/C++ record'); the override is reached through C++'s virtual dispatch, also from
# NativeSharp through the C++ pointer. / Derivar de `lib.Shape` era un rechazo; la sobreescritura se alcanza por el
# despacho virtual de C++, tambien desde NativeSharp por el puntero C++.
positivo(Caso('x3-base-no-interfaz', H2, S2, **CPP),
         'var m = new MyShape(); lib.Shape* p = (lib.Shape*)m; return p->area() + p->tag + p->scale(0);',
         preludio='class MyShape : lib.Shape { public int area() { return 40; } }\n', modes=['directo', 'dll'])

H4 = 'namespace lib { struct VB { int v = 40; }; struct M1 : virtual VB { int m1 = 1; }; struct M2 : virtual VB { int m2 = 1; };\n' \
     'struct DD : M1, M2 { int dd = 0; }; int vb_value(DD* p); }\n'
# X3-03: this record with virtual bases and implicit members was a rejection ('cannot emit the requested implicit C++
# lifetime member'); a record with virtual bases now constructs and destroys through adapters Clang compiles
# (`new (self) T(...)` passes the Microsoft most-derived flag, `self->~T()` runs the vbase destructor), explicit or
# implicit. / Este record con bases virtuales era un rechazo; ahora su vida pasa por adaptadores que compila Clang.
positivo(Caso('x3-base-virtual', H4, 'namespace lib { int vb_value(DD* p) { return p->v + p->m1 + p->m2; } }\n', **CPP),
         'lib.DD d = new lib.DD(); return lib.vb_value(&d);', modes=['directo', 'dll'])
H4b = r'''
namespace lib {
struct VB { int v; VB(); };
struct M1 : virtual VB { int m1; M1(); };
struct M2 : virtual VB { int m2; M2(); };
struct DD : M1, M2 { int dd; DD(); ~DD(); };
int vb_value(DD* p);
int dtors();
}
'''
S4b = r'''
namespace lib {
static int d;
VB::VB() : v(40) {}
M1::M1() : m1(1) {}
M2::M2() : m2(1) {}
DD::DD() : dd(0) {}
DD::~DD() { ++d; }
int vb_value(DD* p) { return p->v + p->m1 + p->m2; }
int dtors() { return d; }
}
'''
positivo(Caso('x3-base-virtual-explicita', H4b, S4b, **CPP),
         '{ var d = new lib.DD(); if (lib.vb_value(&d) != 42) return 1; } return lib.dtors() == 1 ? 42 : 2;',
         modes=['directo', 'dll'])
# X3-04: an observer interface with a virtual destructor and a virtual method with a C++ body: the class implements the
# pure method and may override the other (`Full`) or leave it to C++ (`Doubler`). C++ must not delete the object (its
# deleting destructor traps). / Una interfaz de observador con destructor virtual y un virtual con cuerpo C++: la clase
# implementa el puro y puede sobrescribir el otro o dejarlo a C++.
HOBS = r'''
namespace lib {
struct Listener {
  virtual ~Listener() = default;
  virtual int onEvent(int x) { return x; }
  virtual int compute(int x) const = 0;
};
int drive(Listener* l);
}
'''
PREOBS = r'''
class Doubler : lib.Listener { public int compute(int x) { return x * 2; } }
class Full : lib.Listener
{
    public int total;
    public int onEvent(int x) { total += x; return x * 3; }
    public int compute(int x) { return total + x; }
}
'''
positivo(Caso('x3-observador', HOBS, 'namespace lib { int drive(Listener* l) { return l->onEvent(10) + l->compute(1); } }\n',
              **CPP),
         'var d = new Doubler(); if (lib.drive((lib.Listener*)d) != 12) return 1;\n'
         '    var f = new Full(); return lib.drive((lib.Listener*)f) == 41 && f.total == 10 ? 42 : 2;',
         preludio=PREOBS, modes=['directo', 'dll'])
# X3-01: a NativeSharp class derived from a polymorphic C++ class with data, default-constructible and with a trivial
# destructor: the C++ object (base and adapter) lives in the hidden fields, constructed once by default (its `id = 7`
# persists across conversions) and never destroyed there. A base with a non-trivial destructor stays a rejection.
# X3-01: una clase NativeSharp derivada de una clase C++ polimorfica con datos y destructor trivial; el objeto C++ vive
# en los campos ocultos, construido una sola vez.
HW = r'''
namespace lib {
struct Widget { int id = 7; long long pad[2] = {1, 2}; virtual int draw() const { return id; }
                virtual int scale(int k) { id += 1; return (id - 1) * k; } };
int render(Widget* w);
}
'''
SW = 'namespace lib { int render(Widget* w) { return w->draw() + w->scale(2) + (int)w->pad[1]; } }\n'
positivo(Caso('x3-base-con-datos', HW, SW, **CPP),
         'var w = new MyWidget(); if (lib.render((lib.Widget*)w) != 42) return 1;\n'
         '    return lib.render((lib.Widget*)w) == 44 ? 42 : 2;',
         preludio='class MyWidget : lib.Widget { public int draw() { return 26; } }\n', modes=['directo', 'dll'])
# X3-01: a base with a non-trivial destructor was a rejection ('cannot derive from the C/C++ record'); the C++ object is
# now destroyed with the NativeSharp object, after the class's own fields as a C++ base subobject is, and only if a
# conversion constructed it; a virtual destructor too. / Una base con destructor no trivial era un rechazo; el objeto
# C++ se destruye ahora con el objeto NativeSharp, solo si una conversion lo construyo.
HO = r'''
namespace lib {
struct Owning { int id = 5; virtual int f() { return id; } ~Owning(); };
struct VOwning { int id = 9; virtual int g() = 0; virtual ~VOwning(); };
int use(Owning* o);
int vuse(VOwning* o);
int dtors();
}
'''
SO = r'''
namespace lib {
static int d;
Owning::~Owning() { ++d; }
VOwning::~VOwning() { d += 10; }
int use(Owning* o) { return o->f() + o->id; }
int vuse(VOwning* o) { return o->g() + o->id; }
int dtors() { return d; }
}
'''
BODYO = r'''
    { var x = new X(); if (lib.use((lib.Owning*)x) != 37 || lib.use((lib.Owning*)x) != 37) return 1; }
    if (lib.dtors() != 1) return 2;
    { var x = new X(); }
    if (lib.dtors() != 1) return 3;
    { var y = new Y(); if (lib.vuse((lib.VOwning*)y) != 42) return 4; }
    return lib.dtors() == 11 ? 42 : 5;
'''
# X3-01: `base(args)` of a class derived from a C++ class constructs the C++ object with that base constructor (the
# adapter inherits it and exposes an entry with its ABI); without a default constructor only `base(args)` builds it.
# X3-01: `base(args)` construye el objeto C++ con ese constructor de la base.
HN = 'namespace lib { struct Node { int id; Node(int i); virtual int value() const; }; int read(Node* n); }\n'
SN = ('namespace lib { Node::Node(int i) : id(i) {} int Node::value() const { return id; }\n'
      'int read(Node* n) { return n->value() + n->id; } }\n')
positivo(Caso('x3-base-argumentos', HN, SN, **CPP), 'var l = new Leaf(12); return lib.read((lib.Node*)l);',
         preludio='class Leaf : lib.Node { public Leaf(int k) : base(k) { } public int value() { return 30; } }\n',
         modes=['directo', 'dll'])
positivo(Caso('x3-base-destructor', HO, SO, **CPP), BODYO,
         preludio='class X : lib.Owning { public int f() { return 32; } }\nclass Y : lib.VOwning { public int g() { return 33; } }\n',
         modes=['directo', 'dll'])
fin()
