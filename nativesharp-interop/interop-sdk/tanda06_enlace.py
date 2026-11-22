"""Tanda 6: codigo en headers, globales y enlace C (items 6.1-6.14).

6.1 biblioteca de un archivo con `Implementation="MACRO"` (definiciones externas, `static` internos y estado);
6.2 definiciones C99 `inline` y `extern inline` del encabezado; 6.3 estado `static` compartido por helpers;
6.6 `static const` con inicializador (record con puntero a funcion y arreglo); 6.7 datos `dllimport` de una DLL C;
6.8 definicion tentativa interna; 6.10 `f()` y `f(int)`; 6.13 `_MSC_VER` y `__clang__`; 6.14 `alias`.
Limitaciones documentadas: 6.4 y 6.5 (plan ABI de funciones `static` con records `sysv_abi`), 6.11 (puntero a
funcion sin prototipo llamado con argumentos).
"""
from comun import Caso, positivo, fin

SINGLE = r'''
#ifndef MYLIB_H
#define MYLIB_H
int mylib_add(int a, int b);
int mylib_count(void);
#ifdef MYLIB_IMPLEMENTATION
static int calls = 0;
static int twice(int v) { return v + v; }
int mylib_add(int a, int b) { calls++; return twice(a) - a + b; }
int mylib_count(void) { return calls; }
#endif
#endif
'''
positivo(Caso('t6-single', SINGLE, None, extra_ref=' Implementation="MYLIB_IMPLEMENTATION"'),
         'int r = mylib_add(40, 1); return r + mylib_count();')

H = r'''
inline int twice_i(int x) { return 2 * x; }
extern inline int thrice_i(int x) { return 3 * x; }
static int counter = 40;
static inline int bump(void) { return ++counter; }
static inline int peek(void) { return counter; }
typedef struct CB { int (*f)(int); int k; } CB;
static int inc(int v) { return v + 1; }
static const CB cbs = { inc, 41 };
static const int table[3] = {1, 2, 39};
static int tent;
static inline int bump_t(void) { return ++tent; }
int old_fn();
int old_fn(int x);
#if defined(_MSC_VER) && defined(__clang__)
#define BOTH 42
#else
#define BOTH 1
#endif
static inline int both(void) { return BOTH; }
int base_fn(void);
int alias_fn(void);
'''
S = r'''
int old_fn(int x) { return x; }
int base_fn(void) { return 42; }
int alias_fn(void) __attribute__((alias("base_fn")));
'''
BODY = r'''
    if (twice_i(18) + thrice_i(2) != 42) return 1;
    bump(); bump();
    if (peek() != 42) return 2;
    if (cbs.f(cbs.k) + table[0] + table[1] + table[2] != 84) return 3;
    bump_t(); bump_t();
    if (bump_t() != 3) return 4;
    if (old_fn(42) != 42 || both() != 42 || alias_fn() != 42) return 5;
    return 42;
'''
positivo(Caso('t6-c', H, S, standard='c11'), BODY)

DLL_H = '__declspec(dllimport) extern int lib_version;\n__declspec(dllimport) extern int lib_table[3];\n' \
        '__declspec(dllimport) int lib_get(void);\n'
DLL_S = '__declspec(dllexport) int lib_version = 40;\n__declspec(dllexport) int lib_table[3] = {0, 2, 0};\n' \
        '__declspec(dllexport) int lib_get(void) { return lib_version + lib_table[1]; }\n'
positivo(Caso('t6-dlldata', DLL_H, DLL_S, shared=True),
         'if (lib_get() != 42) return 1; lib_version = 30; return lib_version + lib_table[1] + 10;')
fin()
