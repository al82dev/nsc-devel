"""Tanda 3: escalares, cadenas y enums C (items 3.1-3.13).

3.1 `char* p = "hi"` apunta a los caracteres; 3.2 literal hacia `const char*` y `const unsigned char*` C;
3.2b literal hacia `char*` C rechazado con nota; 3.3 `double -> float` e `int -> unsigned` hacia parametros C;
3.4 `(bool)2` es verdadero; 3.5 `long` C de 32 bits y bool de otros anchos; 3.7 `_BitInt(N)` por valor;
3.8 `long double` (binary64 en Win64); 3.9 enumerador C23 con tipo base fijo; 3.11 identidad de cada enum;
3.12 `char16_t`, `char32_t`, `wchar_t`, `char8_t` en C++; 3.13 `_Complex double` por valor.
Limitaciones documentadas (sin prueba positiva): 3.6 `_Float16`, 3.10 enumeradores de mas de 64 bits.
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
int slen(const char* s);
int sulen(const unsigned char* s);
int wlen(char* s);
float half(float x);
unsigned int twice(unsigned int x);
_Bool truth(_Bool b);
long lw(long x);
unsigned long ul(unsigned long x);
typedef unsigned char FT_Bool;
FT_Bool ftb(FT_Bool b);
long double ld(long double x);
enum C1 { X1 = 1 };
enum C2 { Y2 = 1 };
int e1(enum C1 v);
int e2(enum C2 v);
double re(double _Complex z);
double _Complex mk(double a, double b);
'''
S = r'''
int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }
int sulen(const unsigned char* s) { int n = 0; while (s[n]) n++; return n; }
int wlen(char* s) { return 0; }
float half(float x) { return x / 2; }
unsigned int twice(unsigned int x) { return x * 2; }
_Bool truth(_Bool b) { return b; }
long lw(long x) { return x + 1; }
unsigned long ul(unsigned long x) { return x; }
FT_Bool ftb(FT_Bool b) { return b; }
long double ld(long double x) { return x + 1; }
int e1(enum C1 v) { return v + 40; }
int e2(enum C2 v) { return v; }
double re(double _Complex z) { return __real__ z; }
double _Complex mk(double a, double b) { double _Complex z; __real__ z = a; __imag__ z = b; return z; }
'''
BODY = r'''
    char* p = "hi";
    if (p[0] != 104 || p[1] != 105 || p[2] != 0) return 1;
    if (slen("hello") != 5 || sulen("ab") != 2) return 2;
    double d = 4.0; int i = 3;
    if ((int)half(d) != 2 || twice(i) != 6u) return 3;
    bool b = (bool)2; bool f = (bool)0.5;
    if (!b || !f || (bool)0) return 4;
    if (lw(40) != 41 || ul(7) != 7u || ftb(1) != 1) return 5;
    if ((int)ld(41.0) != 42) return 6;
    if (e1(X1) + e2(Y2) != 42) return 7;
    var z = mk(40.0, 2.0);
    if ((int)(re(z) + z.imag) != 42) return 8;
    return 42;
'''
c = Caso('t3-c', H, S)
positivo(c, BODY)
rechazo(c, 'return wlen("x");', 'a string literal is read-only')

H23 = r'''
enum E : unsigned char { A = 1, B = 41 };
int ev(enum E e);
_BitInt(7) b7(_BitInt(7) x);
unsigned _BitInt(12) u12(unsigned _BitInt(12) x);
'''
S23 = r'''
int ev(enum E e) { return e; }
_BitInt(7) b7(_BitInt(7) x) { return x + 1; }
unsigned _BitInt(12) u12(unsigned _BitInt(12) x) { return x; }
'''
c23 = Caso('t3-c23', H23, S23, standard='c23')
positivo(c23, 'if (ev(B) + A != 42) return 1; var a = b7(40); var u = u12(1); return (int)a + (int)u;')

HPP = r'''
int c16(const char16_t* s);
int c32(const char32_t* s);
int wc(const wchar_t* s);
char16_t first16();
int c8(const char8_t* s);
'''
SPP = r'''
int c16(const char16_t* s) { return s[0]; }
int c32(const char32_t* s) { return s[0]; }
int wc(const wchar_t* s) { return s[0]; }
char16_t first16() { return 7; }
int c8(const char8_t* s) { return s[0]; }
'''
cpp = Caso('t3-cpp', HPP, SPP, language='C++')
positivo(cpp, 'ushort[2] a; a[0] = 10; uint[1] b; b[0] = 20; ushort[1] w; w[0] = 5; byte[1] e; e[0] = 0; '
              'return c16(&a[0]) + c32(&b[0]) + wc(&w[0]) + first16() + c8(&e[0]);')
fin()
