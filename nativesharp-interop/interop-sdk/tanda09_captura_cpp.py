"""Tanda 9: captura y resolucion C++ basicas (items 9.1-9.19).

9.1 familia de sobrecargas con una no proyectable de otra aridad; 9.2 sobrecarga `= delete`; 9.4 `operatorNorm`;
9.5 funcion friend definida en la clase (ADL); 9.6 operador libre de namespace; 9.7 `static const` en clase sin
definicion externa y `static constexpr`; 9.8 miembro static heredado de una base; 9.9 enum sin ambito promovido a
int; 9.10 `operator|` de `enum class`; 9.11 arreglo de enum en un record C++; 9.12 referencias en `extern "C"`;
9.13 campo puntero a funcion en un record C++; 9.14 referencias en la firma de un callback; 9.17 campo referencia;
9.19 alias de namespace y `using`.
Limitaciones documentadas: 9.15 (la ambiguedad `h(long)` / `h(long long)` choca con T10-009; se revirtio), 9.3 (familia de constructores), 9.16 (`explicit`, depende de la Tanda 10), 9.18 (una
funcion con parametro de enum anonimo tiene enlace interno en C++: ver 15.6).
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
struct Imp { int ReadFile(const char* f, unsigned flags); int ReadFile(const char* f, unsigned flags, volatile int** bad); };
struct DL { void AddRect(int a); void AddRect(int a, float* b) = delete; int PathStroke(int c) { return c; } };
struct Mat { int operatorNorm() const { return 42; } };
struct FP { int x, y; friend int dot(const FP& a, const FP& b) { return a.x * b.x + a.y * b.y; } };
namespace ns { struct UUID { int v; }; inline bool operator==(const UUID& a, const UUID& b) { return a.v == b.v; } }
struct TS { static const int kSize = 40; static constexpr int kTwo = 2; };
struct Base { static const int value = 42; };
struct Derived : Base {};
enum Flags { FA = 1, FB = 2 };
int takes_int(int x);
enum class Mask : unsigned { M1 = 1, M2 = 4 };
inline Mask operator|(Mask a, Mask b) { return (Mask)((unsigned)a | (unsigned)b); }
inline unsigned bits(Mask m) { return (unsigned)m; }
struct Lbl { const Flags* labels; Flags arr[3]; };
extern "C" int c_ref(int& out);
struct Exp { int (*fn)(int); };
typedef void (*cb_ref)(int& a, const int& b);
int with_cbref(cb_ref f);
int h(long v);
int h(long long v);
namespace very_long_name { inline namespace v1 { struct P { int x; }; inline int getx(const P& p) { return p.x; } } }
namespace vln = very_long_name;
using PAlias = very_long_name::P;
int plus1(int v);
'''
S = r'''
int Imp::ReadFile(const char* f, unsigned flags) { return 42; }
int Imp::ReadFile(const char* f, unsigned flags, volatile int** bad) { return 0; }
void DL::AddRect(int a) {}
int takes_int(int x) { return x; }
extern "C" int c_ref(int& out) { out = 42; return 0; }
int with_cbref(cb_ref f) { int x = 0, y = 42; f(x, y); return x; }
int h(long v) { return 1; }
int h(long long v) { return 2; }
int plus1(int v) { return v + 1; }
'''
PRELUDIO = 'public static class Cb { public static void F(int* a, int const* b) { *a = *b; } }'
BODY = r'''
    Imp i;
    if (i.ReadFile("x", 0u) != 42) return 1;
    DL d; d.AddRect(1);
    if (d.PathStroke(42) != 42) return 2;
    Mat m;
    if (m.operatorNorm() != 42) return 3;
    FP a; a.x = 6; a.y = 1; FP b; b.x = 7; b.y = 0;
    if (dot(in a, in b) != 42) return 4;
    ns.UUID u1; u1.v = 1; ns.UUID u2; u2.v = 1;
    if (!(u1 == u2)) return 5;
    if (TS.kSize + TS.kTwo != 42 || Derived.value != 42) return 6;
    if (takes_int(FA | FB) != 3) return 7;
    Mask mk = Mask.M1 | Mask.M2;
    if (bits(mk) != 5u) return 8;
    Lbl l; l.arr[1] = FB;
    if ((int)l.arr[1] != 2) return 9;
    int r = 0; c_ref(ref r);
    if (r != 42) return 10;
    Exp e; e.fn = plus1;
    if (e.fn(41) != 42) return 11;
    if (with_cbref(Cb.F) != 42) return 12;
    PAlias p; p.x = 42;
    if (vln.getx(in p) != 42) return 13;
    return 42;
'''
c = Caso('t9-cpp', H, S, language='C++')
positivo(c, BODY, PRELUDIO)
fin()
