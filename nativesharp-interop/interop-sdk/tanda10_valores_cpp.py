"""Tanda 10: valores C++ no triviales (items 10.1-10.13).

10.1 record con constructor por defecto con cuerpo (ImVec2, BodyID, Vector3) pasado y devuelto por valor;
10.5 NSDMI con `new NS()`; 10.7 clase vacia por valor; 10.8 record `alignas(16)` devuelto por valor;
10.10 `new Q()` de un tipo C++; 10.13 local construido con `new Q()` y destruido al salir.
Rechazo: inicializar por defecto un local cuyo constructor por defecto no es trivial (se pide `new T()`).
Limitaciones documentadas: 10.2/10.3/10.4/10.6/10.9 (copia, movimiento o destruccion no triviales por valor,
std::string, temporales), 10.11 (delete con destructor virtual), 10.12 (placement).
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
struct V { float x, y; V() : x(0), y(0) {} V(float a, float b) : x(a), y(b) {} };
V make_v(float a, float b);
float sum_v(V v);
struct Q { int x; Q(); ~Q(); };
int q_count();
struct NS { int a = 40; int b = 2; };
int ns_sum(const NS* n);
struct Empty {};
int take_empty(Empty e);
struct alignas(16) A16 { float v[4]; };
A16 make_a16();
'''
S = r'''
V make_v(float a, float b) { return V(a, b); }
float sum_v(V v) { return v.x + v.y; }
static int alive = 0;
Q::Q() : x(42) { alive++; }
Q::~Q() { alive--; }
int q_count() { return alive; }
int ns_sum(const NS* n) { return n->a + n->b; }
int take_empty(Empty e) { return 42; }
A16 make_a16() { A16 a; a.v[3] = 42; return a; }
'''
PRELUDIO = 'public static class Scope { public static int Inner() { Q q = new Q(); return q.x + q_count(); } }'
BODY = r'''
    V v = make_v(40, 2);
    if ((int)sum_v(v) != 42 || (int)v.x != 40) return 1;
    NS n = new NS();
    if (ns_sum(&n) != 42) return 2;
    Empty e;
    if (take_empty(e) != 42) return 3;
    A16 a = make_a16();
    if ((int)a.v[3] != 42) return 4;
    if (Scope.Inner() != 43 || q_count() != 0) return 5;
    return 42;
'''
c = Caso('t10-cpp', H, S, language='C++')
positivo(c, BODY, PRELUDIO, modes=['directo', 'dll'])
# EN: Constructing a C++ object inside a precompiled generic body is item 13.11 (T19-016); the .lib consumer runs
#     the rest. / ES: Construir un objeto C++ en el cuerpo de una plantilla precompilada es el item 13.11.
BODY_LIB = '\n'.join(line for line in BODY.splitlines()
                     if not any(token in line for token in ('new NS', 'ns_sum', 'Scope.Inner')))
positivo(c, BODY_LIB, PRELUDIO, modes=['lib'])
rechazo(c, 'V v; return (int)v.x;', "has a non-trivial default constructor")
fin()
