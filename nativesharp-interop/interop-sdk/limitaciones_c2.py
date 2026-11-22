"""Limitaciones C2: macros C y contextos sintacticos (plan de limitaciones, tareas C2-01..C2-09).

C2-01/C2-02: una macro de objeto que se expande a una lista de argumentos (`AI_MATKEY_COLOR`, las macros URL de
opusfile) equivale, como argumento de una llamada, a todos sus elementos en orden, como la expande el preprocesador C;
fuera de una llamada da un diagnostico propio y nunca se toma solo el ultimo valor.
"""
from comun import Caso, positivo, rechazo, diagnostico, fin

H = r'''
#define MATKEY_COLOR "$clr.diffuse", 0, 0
#define PAIR 20, 22
#define NESTED (1, 2), 40
int key_sum(const char* key, unsigned type, unsigned index);
int add(int a, int b);
int sum3(int a, int b, int c);
int vsum(int n, ...);
'''
S = r'''
#include <stdarg.h>
int key_sum(const char* key, unsigned type, unsigned index) { int n = 0; while (key[n]) n++; return n + (int)type + (int)index; }
int add(int a, int b) { return a + b; }
int sum3(int a, int b, int c) { return a + b + c; }
int vsum(int n, ...) { va_list list; va_start(list, n); int total = 0; for (int i = 0; i < n; i++) total += va_arg(list, int); va_end(list); return total; }
'''
BODY = r'''
    if (add(PAIR) != 42) return 1;
    if (sum3(0, PAIR) != 42) return 2;
    if (key_sum(MATKEY_COLOR) != 12) return 3;
    if (vsum(2, PAIR) != 42) return 4;
    if (add(NESTED) != 42) return 5;
    return 42;
'''
c = Caso('c2-lista', H, S)
positivo(c, BODY)
rechazo(c, 'int x = PAIR; return x;', 'expands to a list of 2 arguments')
# NativeSharp lookup wins inside a unit that declares the name (no silent C preprocessing of NativeSharp code).
rechazo(c, 'int PAIR = 1; return add(PAIR);', 'add')

# C2-08: constant macros inside a precompiled generic body reach the consumer (the .lib mode instantiates the body in
# a separate project without the header): the consumer evaluates them over the producer's macro source.
H8 = r'''
#define ANSWER 40
#define HALF 2.5
#define LETTER 'A'
#define NAME "ab"
#define BIG 0xFFFFFFFFu
int twice(int v);
'''
S8 = 'int twice(int v) { return 2 * v; }\n'
BODY8 = r'''
    double h = HALF;
    sbyte const* name = NAME;
    uint big = BIG;
    if ((int)(h * 2.0) != 5 || LETTER != 65 || name[1] != 98 || big != 4294967295u) return 1;
    return twice(ANSWER / 2) + 2;
'''
positivo(Caso('c2-constantes', H8, S8), BODY8)
fin()
