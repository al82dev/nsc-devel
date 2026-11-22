"""Tanda 7: callbacks y variadicas C (items 7.1-7.15).

7.1 lambda sin capturas escrita como argumento de un callback C; funcion estatica como callback; 7.5 campos
puntero a funcion de un record C (allocator); 7.6 nombre de funcion importada como puntero; 7.8 callback que
recibe otro puntero a funcion; 7.9 `va_list` recibido por un callback NativeSharp y reenviado a una funcion C `v*`
(Windows x64: `va_list` es `char*`); 7.11 variadica `__cdecl` (WINAPIV); 7.12 bit-field estrecho promovido a `int` en
una variadica; 7.13 `double` en variadicas (copia XMM/GPR de Win64); 7.14 comparacion y conversion de punteros a
funcion; 7.15 `null` hacia un puntero a funcion (sin llamarlo).
Rechazos: lambda con capturas hacia un callback C (7.2, limitacion documentada).
Limitaciones documentadas: 7.2 y 7.3 (closures y metodos de instancia como callback necesitan el contrato de
contexto de la Tanda 8), 7.4 (un panic en un callback termina el proceso: frontera definida), 7.7 (no reproducido),
7.10 (definir en NativeSharp el cuerpo de una funcion C variadica).
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
#include <stdarg.h>
typedef int (*cb_t)(int, void*);
int run_cb(cb_t cb, void* ud);
typedef struct Alloc { void* (*alloc)(int); void (*release)(void*); int tag; } Alloc;
int use_alloc(Alloc* a);
typedef int (*inner_t)(int);
typedef int (*outer_t)(inner_t, int);
int nest(outer_t o);
int plain(int x);
inner_t getf(void);
int is_null(cb_t cb);
int vsum(int n, va_list ap);
typedef int (*vfn)(int n, va_list ap);
int call_with(vfn f, int n, ...);
int sum(int n, ...);
double dsum(int n, ...);
typedef struct BF { unsigned small : 3; int big; } BF;
int first_arg(int n, ...);
int __cdecl winapiv(int n, ...);
'''
S = r'''
int run_cb(cb_t cb, void* ud) { return cb(40, ud); }
int use_alloc(Alloc* a) { void* p = a->alloc(8); a->release(p); return a->tag; }
static int dbl(int v) { return v * 2; }
int nest(outer_t o) { return o(dbl, 21); }
int plain(int x) { return x; }
inner_t getf(void) { return plain; }
int is_null(cb_t cb) { return cb == 0; }
int vsum(int n, va_list ap) { int t = 0; for (int i = 0; i < n; i++) t += va_arg(ap, int); return t; }
int call_with(vfn f, int n, ...) { va_list ap; va_start(ap, n); int r = f(n, ap); va_end(ap); return r; }
int sum(int n, ...) { va_list ap; va_start(ap, n); int t = vsum(n, ap); va_end(ap); return t; }
double dsum(int n, ...) { va_list ap; va_start(ap, n); double t = 0; for (int i = 0; i < n; i++) t += va_arg(ap, double); va_end(ap); return t; }
int first_arg(int n, ...) { va_list ap; va_start(ap, n); int v = va_arg(ap, int); va_end(ap); return v; }
int __cdecl winapiv(int n, ...) { va_list ap; va_start(ap, n); int v = va_arg(ap, int); va_end(ap); return v + n; }
'''
PRELUDIO = r'''
public static class Cb
{
    public static int Add2(int v, void* u) { return v + 2; }
    public static void* A(int n) { return null; }
    public static void Release(void* p) { }
    public static int O(inner_t f, int v) { return f(v); }
    public static int F(int n, sbyte* ap) { return vsum(n, ap); }
}
'''
BODY = r'''
    if (run_cb((int v, void* u) => v + 2, null) != 42) return 1;
    if (run_cb(Cb.Add2, null) != 42) return 2;
    Alloc a; a.alloc = Cb.A; a.release = Cb.Release; a.tag = 42;
    if (use_alloc(&a) != 42) return 3;
    if (nest(Cb.O) != 42) return 4;
    inner_t f = plain;
    inner_t g = getf();
    void* raw = (void*)g;
    if (f(42) != 42 || !(f == g) || raw == null) return 5;
    if (is_null(null) != 1) return 6;
    if (call_with(Cb.F, 2, 40, 2) != 42) return 7;
    if (sum(3, 40, 1, 1) != 42) return 8;
    if ((int)dsum(3, 40.5, 1.25, 0.25) != 42) return 9;
    BF b; b.small = 5u; b.big = 37;
    if (first_arg(1, b.small) + b.big != 42) return 10;
    if (winapiv(2, 40) != 42) return 11;
    return 42;
'''
c = Caso('t7-c', H, S)
positivo(c, BODY, PRELUDIO)
rechazo(c, 'int k = 2; return run_cb((int v, void* u) => v + k, null);', 'cannot capture state')
fin()
