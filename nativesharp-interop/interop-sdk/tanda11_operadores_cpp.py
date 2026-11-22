"""Tanda 11: operadores C++ (items 11.1-11.11).

11.1 `operator[]` mutable (referencia) y const como indexador de NativeSharp, lectura y escritura; 11.4 asignacion
compuesta con `operator+=` miembro (por su adaptador) y libre `P& operator+=(P&, const P&)`, y `operator*=` con un
escalar por valor; ademas los operadores binarios y de comparacion libres que ya funcionaban (`==` de namespace).
Limitaciones documentadas: 11.2 (`*it`), 11.3 (posfijos), 11.5 (`operator=` de usuario), 11.6 (`operator()`),
11.7 (conversiones y `->`), 11.8 (`&&`/`||`), 11.9 (`<=>`, ademas requiere <compare>), 11.10 (operadores heredados),
11.11.
"""
from comun import Caso, positivo, fin

H = r'''
struct Vec2 { float x, y;
  float& operator[](int i) { return i == 0 ? x : y; }
  float operator[](int i) const { return i == 0 ? x : y; }
  Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
  Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};
float read1(const Vec2* v);
struct P { int x; };
inline P& operator+=(P& a, const P& b) { a.x += b.x; return a; }
inline P& operator-=(P& a, int b) { a.x -= b; return a; }
namespace ns { struct U { int v; }; inline bool operator==(const U& a, const U& b) { return a.v == b.v; } }
'''
S = r'''
float read1(const Vec2* v) { return (*v)[1]; }
'''
BODY = r'''
    Vec2 v; v.x = 40; v.y = 0;
    v[1] = 2;
    if ((int)(v[0] + v[1]) != 42 || (int)read1(&v) != 2) return 1;
    Vec2 b; b.x = 1; b.y = 1;
    v += b;
    if ((int)v.x != 41 || (int)v.y != 3) return 2;
    v *= 2.0f;
    if ((int)v.x != 82) return 3;
    P p; p.x = 40; P q; q.x = 5;
    p += q;
    p -= 3;
    if (p.x != 42) return 4;
    ns.U a; a.v = 1; ns.U c; c.v = 1;
    if (!(a == c)) return 5;
    return 42;
'''
c = Caso('t11-cpp', H, S, language='C++')
positivo(c, BODY, modes=['directo', 'dll'])
# EN: The indexer of a C++ record inside a precompiled generic body is restored from metadata without the indexer
#     mark (item 13.11); the .lib consumer runs the rest.
# ES: El indexador de un record C++ dentro de una plantilla precompilada se restaura de la metadata sin la marca de
#     indexador (item 13.11); el consumidor .lib ejecuta el resto.
BODY_LIB = BODY.replace('    v[1] = 2;', '    v.y = 2;').replace(
    "if ((int)(v[0] + v[1]) != 42 || (int)read1(&v) != 2) return 1;", "if ((int)read1(&v) != 2) return 1;")
positivo(c, BODY_LIB, modes=['lib'])
fin()
