"""Limitaciones C4: callbacks y variadicas C (plan de limitaciones, tareas C4-01..C4-05).

C4-01: una lambda escrita como argumento de una funcion C importada en una posicion puntero a funcion toma ese tipo
(parametros y resultado de la firma C, tambien con cuerpo de bloque) y llega al puntero por su thunk; una funcion o
metodo estatico NativeSharp tambien. Un estado capturado y un metodo de instancia se rechazan: el puntero C no tiene
canal de entorno ni receptor.
C4-02: el par `callback + user_data` lleva el estado explicito: registro, invocacion posterior, desregistro, y pares
asignador/liberador con `opaque`. La vida del estado es la del almacenamiento que el programa le da (R1).
C4-03: un metodo de instancia no es callback C sin contexto aprobado; el receptor se pasa por `user_data`.
C4-04: la llamada variadica C (promociones, records por valor como Clang) y el cuerpo NativeSharp quedan separados:
un cuerpo NativeSharp no es variadico; un `va_list` (char* en Win64) pasa de un callback C a otra funcion C; una
lambda detras de un puntero variadico recibe sus parametros fijos (Win64: el llamador limpia la pila).
C4-05: una excepcion no atraviesa la frontera: lanzarla en un callback sin capturarla es un error de compilacion; un
`try` dentro del callback la contiene.
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
#include <stdarg.h>
#include <stddef.h>
typedef int (*binop)(int, int);
typedef void (*visit)(void* user, int value);
int apply(binop f, int a, int b);
int each(int n, visit cb, void* user);
typedef void (*listener)(void* user, int event);
int subscribe(listener cb, void* user);
void unsubscribe(int id);
int fire(int event);
typedef void (*logfn)(void* user, const char* fmt, va_list ap);
int run_logged(logfn cb, void* user);
int format_into(char* buf, size_t n, const char* fmt, va_list ap);
typedef void* (*alloc_fn)(void* opaque, int items, int size);
typedef void (*free_fn)(void* opaque, void* p);
int use_allocator(alloc_fn a, free_fn f, void* opaque);
typedef struct P { int x, y; } P;
typedef struct Big { long long a, b, c; } Big;
double mix(int count, ...);
int recs(int count, ...);
typedef int (*vcb)(int n, ...);
int take_vcb(vcb f);
typedef double (*vcbd)(double d, ...);
double take_vcbd(vcbd f);
'''
S = r'''
int apply(binop f, int a, int b) { return f(a, b); }
int each(int n, visit cb, void* user) { for (int i = 1; i <= n; ++i) cb(user, i); return n; }
static listener slots[4]; static void* users[4];
int subscribe(listener cb, void* user) { for (int i = 0; i < 4; ++i) if (!slots[i]) { slots[i] = cb; users[i] = user; return i; } return -1; }
void unsubscribe(int id) { slots[id] = 0; users[id] = 0; }
int fire(int event) { int n = 0; for (int i = 0; i < 4; ++i) if (slots[i]) { slots[i](users[i], event); ++n; } return n; }
static void emit(logfn cb, void* user, const char* fmt, ...) { va_list ap; va_start(ap, fmt); cb(user, fmt, ap); va_end(ap); }
int run_logged(logfn cb, void* user) { emit(cb, user, "%d-%s-%.1f", 40, "x", 2.5); return 0; }
int format_into(char* buf, size_t n, const char* fmt, va_list ap)
{
    (void)n; (void)fmt;
    int a = va_arg(ap, int); const char* s = va_arg(ap, const char*); double d = va_arg(ap, double);
    buf[0] = (char)('0' + a / 10); buf[1] = (char)('0' + a % 10); buf[2] = '-'; buf[3] = s[0]; buf[4] = '-';
    buf[5] = (char)('0' + (int)d); buf[6] = 0; return 6;
}
int use_allocator(alloc_fn a, free_fn f, void* opaque)
{
    int* p = (int*)a(opaque, 4, sizeof(int)); if (!p) return -1;
    for (int i = 0; i < 4; ++i) p[i] = 10 + i;
    int s = p[0] + p[3] - 1; f(opaque, p); return s;
}
double mix(int count, ...) { va_list ap; va_start(ap, count); double d = va_arg(ap, double); int b = va_arg(ap, int); int t = va_arg(ap, int); int s = va_arg(ap, int); va_end(ap); return d + b + t + s; }
int recs(int count, ...) { va_list ap; va_start(ap, count); P p = va_arg(ap, P); Big b = va_arg(ap, Big); int t = va_arg(ap, int); va_end(ap); return p.x + p.y + (int)(b.a + b.b + b.c) + t; }
int take_vcb(vcb f) { return f(40, 2, 3.5); }
double take_vcbd(vcbd f) { return f(41.5, 7); }
'''
PRE = '''
int Add(int a, int b) { return a + b; }
struct Counter { public int sum; public int calls; }
struct Pool { public int allocs; public int frees; public int* storage; }
class Calc { public int k; public int AddK(int a, int b) { return a + b + k; } public static int Sum(int a, int b) { return a + b; } }
'''

# C4-01 / C4-03
BODY1 = r'''
    if (apply((a, b) => a + b, 40, 2) != 42) return 1;
    if (apply((int a, int b) => a * b, 6, 7) != 42) return 2;
    if (apply(Calc.Sum, 40, 2) != 42) return 3;
    Counter c; c.sum = 0; c.calls = 0;
    each(6, (u, v) => { Counter* p = (Counter*)u; p->sum += v; p->calls++; }, &c);
    return c.calls == 6 ? c.sum + 21 : 4;
'''
c1 = Caso('c4-lambda-c', H, S)
positivo(c1, BODY1, preludio=PRE)
# A namespace function of the program is a callback too (direct mode: the prelude is in the program).
positivo(Caso('c4-funcion-ns', H, S), 'return apply(Add, 40, 2);', preludio=PRE, modes=['directo'])
rechazo(c1, 'int k = 2; return apply((a, b) => a + b + k, 40, 0);', 'no environment channel', preludio=PRE)
rechazo(c1, 'Calc c = new Calc(); c.k = 0; return apply(c.AddK, 40, 2);', 'a C function pointer has no receiver',
        preludio=PRE)

# C4-02
BODY2 = r'''
    Counter c; c.sum = 0; c.calls = 0;
    int id = subscribe((u, e) => { Counter* p = (Counter*)u; p->sum += e; p->calls++; }, &c);
    fire(30); fire(12);
    unsubscribe(id);
    if (fire(100) != 0 || c.calls != 2) return 1;
    Pool pool; pool.allocs = 0; pool.frees = 0; pool.storage = stackalloc int[16];
    int r = use_allocator((o, items, size) => { Pool* p = (Pool*)o; p->allocs++; return (void*)p->storage; },
                          (o, ptr) => { Pool* p = (Pool*)o; p->frees++; }, &pool);
    if (pool.allocs != 1 || pool.frees != 1 || r != 22) return 2;
    return c.sum;
'''
positivo(Caso('c4-user-data', H, S), BODY2, preludio=PRE)

# C4-04
BODY4 = r'''
    sbyte* buf = stackalloc sbyte[64];
    run_logged((u, fmt, ap) => { sbyte* b = (sbyte*)u; format_into(b, 64, fmt, ap); }, buf);
    if (buf[0] != (sbyte)'4' || buf[1] != (sbyte)'0' || buf[3] != (sbyte)'x' || buf[5] != (sbyte)'2') return 1;
    byte b8 = 10; short s16 = -2;
    if (mix(4, 1.5f, b8, true, s16) != 10.5) return 2;
    P p; p.x = 10; p.y = 20; Big g; g.a = 3; g.b = 4; g.c = 4;
    if (recs(3, p, g, 1) != 42) return 3;
    if (take_vcb((n) => n + 2) != 42 || take_vcbd((d) => d + 0.5) != 42.0) return 4;
    return 42;
'''
c4 = Caso('c4-variadicas', H, S)
positivo(c4, BODY4, preludio=PRE)
rechazo(c4, 'return V(1, 2);', "'...' is valid only in a body-less extern C declaration",
        preludio='int V(int a, ...) { return a; }\n')

# C4-05
c5 = Caso('c4-excepciones', H, S)
positivo(c5, 'return apply((a, b) => { try { if (a > 0) throw new Exception("x"); return 0; } catch (Exception e) { return a + b; } }, 40, 2);')
rechazo(c5, 'return apply((a, b) => { if (a > 0) throw new Exception("x"); return b; }, 1, 2);',
        'is neither caught here nor declared')
fin()
