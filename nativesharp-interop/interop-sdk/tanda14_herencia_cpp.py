"""Tanda 14: herencia, virtuales y COM (items 14.0-14.9).

14.3 interfaz con metodos virtuales puros (estilo COM: AddRef/Release) llamada desde NativeSharp sobre un objeto
creado en C++; 14.0 derivar una clase NativeSharp de un record C++ se rechaza con un diagnostico propio.
Limitaciones documentadas: 14.1/14.2 (clases NativeSharp derivadas de clases C++ con overrides llamados por la
biblioteca: requiere una clase derivada adaptadora generada por Clang), 14.3 sentido entrante, 14.4, 14.5 (bases
virtuales: los campos de la base virtual no se importan), 14.6, 14.7, 14.8, 14.9. Una clase polimorfica definida en el
encabezado lleva RTTI y necesita el runtime de C++ (Tanda 18).
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
struct IUnk { virtual unsigned AddRef() = 0; virtual unsigned Release() = 0; virtual int Value() = 0; };
IUnk* create_obj();
struct Visitor { virtual int Visit(int v) = 0; };
'''
S = r'''
struct Impl : IUnk { unsigned refs; constexpr Impl() : refs(1) {}
  unsigned AddRef() override { return ++refs; } unsigned Release() override { return --refs; }
  int Value() override { return 42; } };
static Impl inst;
IUnk* create_obj() { return &inst; }
'''
c = Caso('t14-cpp', H, S, language='C++')
positivo(c, 'IUnk* o = create_obj(); if (o->AddRef() != 2u) return 1; int v = o->Value(); '
            'if (o->Release() != 1u) return 2; return v;', modes=['directo', 'dll'])
# EN: An abstract C++ record named by a precompiled generic body cannot be published in its metadata (item 13.11).
# ES: Un record C++ abstracto nombrado por una plantilla precompilada no se puede publicar en su metadata (13.11).
# EN: X3-04. Implementing the abstract C++ interface `Visitor` was a rejection ("cannot derive from the C/C++ record");
#     an abstract C++ interface is now implemented, not inherited (the class keeps no NativeSharp base), so this is a
#     positive case. Deriving from a C++ class with data or non-pure virtuals is still rejected (limitaciones_x3.py).
# ES: X3-04. Implementar la interfaz C++ abstracta `Visitor` era un rechazo; una interfaz C++ abstracta ahora se
#     implementa, no se hereda, asi que es un caso positivo. Derivar de una clase C++ con datos o virtuales no puros
#     sigue rechazado (limitaciones_x3.py).
positivo(Caso('t14-visitante', H, S, language='C++'), 'MyVisitor m = new MyVisitor(); return m.Visit(42);',
         preludio='public class MyVisitor : Visitor { public int Visit(int v) { return v; } }', modes=['directo'])
fin()
