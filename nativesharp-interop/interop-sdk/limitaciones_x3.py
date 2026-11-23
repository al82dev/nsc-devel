"""Limitaciones X3: herencia, dispatch, RTTI y COM (plan de limitaciones, tareas X3-01..X3-07).

Complementa pruebas-tanda-12-herencia (bases, ajuste de this, static_cast, dispatch virtual, destruccion por la base,
base virtual, typeid y dynamic_cast en la biblioteca con --dll), que pasa con el binario actual:
X3-01/X3-02: herencia multiple con la segunda base en offset distinto de cero: conversion derivado->base y referencia
a la base.
X3-04: dispatch virtual desde NativeSharp sobre un objeto C++ (herencia simple y multiple con la base polimorfica no
primaria); implementar desde NativeSharp un tipo C++ (override entrante) sigue rechazado.
X3-06: punteros a miembro rechazados con su motivo (su representacion depende del modelo de herencia, T14-007).
X3-07: COM consumido: interfaz abstracta `__stdcall` con AddRef/Release/QueryInterface por su vtable; una interfaz
entrante implementada en NativeSharp queda pendiente con diagnostico.
X3-03: una base virtual cuyo constructor implicito NativeSharp tendria que emitir conserva la guarda.
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
rechazo(c3, 'return 0;', 'cannot derive from the C/C++ record', preludio='class MyCalc : ICalc { }\n')

H4 = 'namespace lib { struct VB { int v = 40; }; struct M1 : virtual VB { int m1 = 1; }; struct M2 : virtual VB { int m2 = 1; };\n' \
     'struct DD : M1, M2 { int dd = 0; }; int vb_value(DD* p); }\n'
rechazo(Caso('x3-base-virtual', H4, 'namespace lib { int vb_value(DD* p) { return p->v; } }\n', **CPP),
        'lib.DD d = new lib.DD(); return lib.vb_value(&d);', 'cannot emit the requested implicit C++ lifetime member')
fin()
