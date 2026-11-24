"""Limitaciones X4: plantillas y entidades C++ persistentes (plan de limitaciones, tareas X4-01..X4-08).

Complementa tanda13_plantillas_cpp.py (13.4 constexpr y argumentos explicitos, 13.6 NTTP unsigned grande).
X4-01/X4-02: deduccion de una funcion plantilla con tipos distintos (int, double), plantilla de clase con constructor y
metodo, argumento no tipo explicito; cada especializacion es la que Clang instancia.
X4-03: especializacion parcial (`S<T*>`) elegida por Clang; una restriccion `requires` incumplida no reaparece: Clang
no deduce y el diagnostico lo dice.
X4-05: un struct NativeSharp como argumento de una plantilla de clase C++ se instancia sobre su espejo C++ (T13-007),
con el layout que da Clang.
X4-06: desde una DLL las mismas especializaciones funcionan. Una plantilla NativeSharp precompilada cuyo cuerpo usa
plantillas C++ no se puede consumir sin el encabezado: el productor no cataloga sus especializaciones (13.10/13.11,
R2); el consumidor falla con diagnostico, sin recurrir al encabezado original.
"""
import comun
from comun import Caso, positivo, rechazo, fin

H = r'''
namespace lib {
template<class T> T maxv(T a, T b) { return a > b ? a : b; }
template<class T> struct Box { T v; Box(T x) : v(x) {} T get() const { return v; } };
template<class T> struct S { static int k() { return 1; } };
template<class T> struct S<T*> { static int k() { return 2; } };
template<int N> int times(int x) { return N * x; }
template<class T> requires (sizeof(T) >= 4) int wide(T x) { return (int)x + 1; }
}
'''
CPP = dict(language='C++', standard='c++20')
BODY = r'''
    if (lib.maxv(30, 42) != 42 || lib.maxv(1.5, 0.5) != 1.5) return 1;
    var b = new lib.Box<int>(40);
    if (b.get() != 40) return 2;
    if (lib.S<int*>.k() != 2 || lib.S<int>.k() != 1) return 3;
    if (lib.times<3>(14) != 42) return 4;
    return lib.wide(41);
'''
c = Caso('x4-plantillas', H, None, **CPP)
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(c, BODY, modes=['directo', 'dll', 'lib'])
rechazo(c, 'short s = 1; return lib.wide(s);', 'Clang cannot deduce the template arguments')
# X4-05: a NativeSharp struct as a class template argument was a rejection ('is not imported in its project'): Box<P>
# is instantiated on the C++ mirror of P (T13-007), whose layout Clang gives and NativeSharp checks; the constructor
# now accepts the mirrored struct as its methods already did. / Un struct NativeSharp como argumento de una plantilla
# de clase era un rechazo; Box<P> se instancia sobre el espejo C++ de P.
positivo(Caso('x4-struct-ns-argumento', H, None, **CPP), 'return Use() + 41;',
         preludio='struct P { public int x; }\nint Use() { P p; p.x = 1; var b = new lib.Box<P>(p); return b.get().x; }\n',
         modes=['directo'])
# X4-04: a C++ variable template written as a value (`lib.sz<long>`) is a value when the token after `>` cannot
# continue a comparison (C# disambiguation) or an argument is a predefined type; Clang evaluates the specialization and
# an integral or bool constant replaces it. Other variable templates are rejected with the reason.
# X4-04: una plantilla de variable C++ escrita como valor; Clang evalua la especializacion (constante entera o bool).
HV = r'''
namespace lib {
template<class T> constexpr int sz = sizeof(T);
template<class T, class U> constexpr bool same = false;
template<class T> constexpr bool same<T, T> = true;
template<int N> constexpr long long sq = (long long)N * N;
template<class T> inline T zero = T(0);
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x4-plantilla-variable', HV, None, **CPP),
         'int a = lib.sz<int>; bool s = lib.same<int, int>; if (!s || lib.same<int, long>) return 1;\n'
         'long v = lib.sq<6>; return lib.sz<long> * 5 + 2 == 42 && a == 4 && v == 36 ? 42 : 2;',
         modes=['directo', 'dll', 'lib'])
# X4-04: a `float`/`double` constexpr variable template was rejected (only integers and bool were read); its bits are
# read through `__builtin_bit_cast` and it becomes the decimal literal that converts back to the same value.
# X4-04: una plantilla de variable constexpr float/double se rechazaba; ahora se lee por sus bits.
HVF = r'''
namespace lib {
template<class T> constexpr T pi = T(3.25);
template<class T> constexpr T tenth = T(0.1);
template<int N> constexpr double half = N / 2.0;
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x4-plantilla-variable-flotante', HVF, None, **CPP),
         'double d = lib.pi<double>; float f = lib.pi<float>; double t = lib.tenth<double>;\n'
         '    if (d != 3.25 || f != 3.25f || t != 0.1 || lib.tenth<float> != 0.1f || lib.half<5> != 2.5) return 1;\n'
         '    return 42;', modes=['directo', 'dll', 'lib'])
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x4-comparacion', HV, None, **CPP), 'int a = 3, b = 2, c = 1; bool x = a < b; return x || c > a ? 1 : 42;',
         modes=['directo', 'lib'])
# X4-04: a variable template that is not a constant (`inline T zero`) was a rejection ('not an integral or bool
# constant'); its specialization is now its storage, reached through an accessor the session declares (as for a macro
# that is not a constant, T19-041): NativeSharp reads and writes the same object C++ sees.
# X4-04: una plantilla de variable no constante era un rechazo; su especializacion es ahora su almacenamiento.
HVN = r'''
namespace lib {
template<class T> inline T zero = T(0);
template<class T> inline T counter = T(5);
inline int read_counter() { return counter<int>; }
}
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x4-plantilla-variable-double', HVN, None, **CPP),
         'if ((int)lib.zero<double> != 0) return 1; lib.counter<int> = 40; lib.counter<int> += 2;\n'
         '    return lib.read_counter() == 42 && lib.counter<int> == 42 ? 42 : 2;', modes=['directo', 'dll', 'lib'])
# X4/X2-06: Exceptions="false" on the reference (a library built with -fno-exceptions) compiles NSC's Clang session
# with -fno-exceptions too: the inline template constructor and destructor NSC emits for a class field of
# `lib.Box<int>` then need no __CxxFrameHandler3 from the C runtime (they did before). A bad value is rejected.
# Exceptions="false" compila la sesion de Clang sin excepciones; el codigo inline de la plantilla no exige el CRT.
HEXC = r'''
namespace lib {
int& ct(); int& dt();
template <typename T> struct Box { T v; Box() : v(T(7)) { ++ct(); } explicit Box(T x) : v(x) { ++ct(); } ~Box() { ++dt(); }
  Box& operator=(const Box& o) { v = o.v; return *this; } };
inline int touch(Box<int>* b) { return b->v; }
}
'''
SEXC = r'''
namespace lib { static int c, d; int& ct() { return c; } int& dt() { return d; } }
'''
CPPEXC = dict(language='C++', standard='c++20')
positivo(Caso('x4-exceptions-false', HEXC, SEXC, extra_ref=' Exceptions="false"', **CPPEXC),
         '{ var h = new H(); if (h.a.v != 7 || h.b.v != 5 || lib.touch(&h.b) != 5) return 1; }'
         ' return lib.dt() * 10 + lib.ct() == 22 ? 42 : 2;',
         preludio='class H { public lib.Box<int> a; public lib.Box<int> b = new lib.Box<int>(5); }'+chr(10),
         modes=['directo', 'dll'])
rechazo(Caso('x4-exceptions-invalido', HEXC, SEXC, extra_ref=' Exceptions="no"', **CPPEXC), 'return 0;',
        'Exceptions must be true or false')
# X4/X2-05: a function template returning a non-trivial record by value (`Box<T> wrap(T)`): the in-place return
# adapter of the specialization was never requested (only plain functions were walked) and, once emitted, the
# specialization it calls was left undefined (Sema instantiated it with no code generator attached). The adapter
# compilation now walks function template specializations and offers the vague-linkage definitions the adapters
# reach, as T15-014 does. / Una plantilla de funcion que devuelve un record no trivial por valor.
HTPL = r'''
namespace lib {
int& ct(); int& dt();
template <typename T> struct Box { T v; explicit Box(T x) : v(x) { ++ct(); } Box(const Box& o) : v(o.v) { ++ct(); } ~Box() { ++dt(); } };
template <typename T> Box<T> wrap(T x) { return Box<T>(x); }
struct Maker { int base; template <typename T> Box<T> make(T x) const { return Box<T>(x + base); } };
}
'''
STPL = r'''
namespace lib { static int c, d; int& ct() { return c; } int& dt() { return d; } }
'''
# NSMeta X4-06: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('x4-plantilla-resultado', HTPL, STPL, language='C++', standard='c++20', extra_ref=' Exceptions="false"'),
         '{ var b = lib.wrap<int>(42); if (b.v != 42 || lib.wrap<int>(7).v != 7) return 1; }'
         ' return lib.ct() == 2 && lib.dt() == 2 ? 42 : 2;', modes=['directo', 'dll', 'lib'])
# EN: A call to a variadic function template with explicit arguments that do not bind its pack (`make_unique<T>(a)`,
#     `sum<int>(40, 1, 1)`) deduces the rest from the call, as C++ does; it was instantiated with an empty pack
#     ("no overload ... accepts 3 argument(s)"). / Una llamada a una plantilla variadica con argumentos explicitos que
#     no fijan su paquete deduce el resto de la llamada; antes se instanciaba con el paquete vacio.
HPK = r'''
namespace lib {
template <class R, class... A> R sum(A... a) { return (R)(0 + ... + a); }
template <class R, class... A> R count(A...) { return (R)sizeof...(A); }
}
'''
positivo(Caso('x4-paquete-explicito', HPK, None, language='C++', standard='c++20'),
         'return lib.sum<int>(40, 1, 1) + lib.count<int>() + (int)lib.count<long>(1, 2) - 2;',
         # lib: the published template's call is deduced by the consumer over the producer's source session
         # (NSMeta X4-06); it was "reusable template closure is incomplete". / lib: lo deduce el consumidor.
         modes=['directo', 'dll', 'lib'])
fin()
