"""Limitaciones X4: plantillas y entidades C++ persistentes (plan de limitaciones, tareas X4-01..X4-08).

Complementa tanda13_plantillas_cpp.py (13.4 constexpr y argumentos explicitos, 13.6 NTTP unsigned grande).
X4-01/X4-02: deduccion de una funcion plantilla con tipos distintos (int, double), plantilla de clase con constructor y
metodo, argumento no tipo explicito; cada especializacion es la que Clang instancia.
X4-03: especializacion parcial (`S<T*>`) elegida por Clang; una restriccion `requires` incumplida no reaparece: Clang
no deduce y el diagnostico lo dice.
X4-05: un struct NativeSharp como argumento de una plantilla de clase C++ se rechaza con diagnostico (sin espejo con
layout, constructores y destructor acordados).
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
positivo(c, BODY, modes=['directo', 'dll'])
rechazo(c, 'short s = 1; return lib.wide(s);', 'Clang cannot deduce the template arguments')
rechazo(c, 'return 0;', 'is not imported in its project',
        preludio='struct P { public int x; }\nint Use() { P p; p.x = 1; var b = new lib.Box<P>(p); return b.get().x; }\n')
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
positivo(Caso('x4-plantilla-variable', HV, None, **CPP),
         'int a = lib.sz<int>; bool s = lib.same<int, int>; if (!s || lib.same<int, long>) return 1;\n'
         'long v = lib.sq<6>; return lib.sz<long> * 5 + 2 == 42 && a == 4 && v == 36 ? 42 : 2;',
         modes=['directo', 'dll'])
positivo(Caso('x4-comparacion', HV, None, **CPP), 'int a = 3, b = 2, c = 1; bool x = a < b; return x || c > a ? 1 : 42;',
         modes=['directo'])
rechazo(Caso('x4-plantilla-variable-double', HV, None, **CPP), 'return (int)lib.zero<double>;',
        'not an integral or bool constant')
fin()
