"""Limitaciones C5: casos C adicionales (plan de limitaciones, tareas C5-01..C5-08).

C5-01: literales hacia `char*`, `signed char*` y `unsigned char*`: primer byte, vacio, nulos embebidos y duracion
(un puntero retenido por C sigue valido: el literal es estatico).
C5-02: `_Generic` distingue `char` de `signed char`: un `sbyte` que una funcion C devuelve como `signed char` conserva
ese tipo; un `sbyte` NativeSharp sin origen C es `char` (item 5.5). Cada llamada enlaza su propio envoltorio.
C5-03: `extern T a[]` y `extern T a[N]` en ambos ordenes dan el tipo compuesto C11 `T[N]` (longitud y limites).
C5-04: un bit-field pasado a `...` se promueve como en C: `unsigned :3` e `int :5` a `int`, `unsigned long long :40`
a `unsigned long long` (comprobado en el IR, no solo por valor).
C5-05: `setjmp` (returns_twice) sigue rechazado con su motivo.
C5-06: un tipo complejo y un enum C23 de base fija con nombres que comparten espacio de nombres con records.
C5-07: funciones `static inline` `sysv_abi` con records usadas como callback C, directo y precompilado (C3-08 cubre
la llamada). Una lambda o funcion NativeSharp hacia un puntero `sysv_abi` es un thunk `x86_64_sysvcc`; los records
y enteros estrechos siguen el plan de Clang del typedef del puntero.
C5-08: `Implementation="MACRO"` con un encabezado de implementacion unica que incluye encabezados de sistema y otro
encabezado propio, en ese orden.
"""
from comun import Caso, positivo, rechazo, ir, fin, _fail, ONLY

H1 = r'''
int clen(const char* s);
int slen(const signed char* s);
int ulen(const unsigned char* s);
int byte_at(const char* s, int i);
void keep(const char* s);
int kept_len(void);
'''
S1 = r'''
int clen(const char* s) { int n = 0; while (s[n]) ++n; return n; }
int slen(const signed char* s) { int n = 0; while (s[n]) ++n; return n; }
int ulen(const unsigned char* s) { int n = 0; while (s[n]) ++n; return n; }
int byte_at(const char* s, int i) { return (unsigned char)s[i]; }
static const char* saved;
void keep(const char* s) { saved = s; }
int kept_len(void) { return clen(saved); }
'''
positivo(Caso('c5-literales', H1, S1),
         'keep("persist"); if (kept_len() != 7) return 1;\n'
         '    return clen("abc") + slen("ab") + ulen("") + byte_at("a\\0b", 2) + clen("a\\0b") + 35 - 98 + 1;')

H2 = r'''
#define KIND(x) _Generic((x), char: 1, signed char: 2, unsigned char: 3, default: 0)
char get_c(void);
signed char get_sc(void);
unsigned char get_uc(void);
'''
S2 = "char get_c(void) { return 'a'; }\nsigned char get_sc(void) { return -1; }\nunsigned char get_uc(void) { return 7; }\n"
positivo(Caso('c5-generic', H2, S2),
         'var c = get_c(); sbyte s = 5;\n'
         '    if (KIND(c) != 1 || KIND(s) != 1 || KIND(get_sc()) != 2 || KIND(get_c()) != 1 || KIND(get_uc()) != 3) return 1;\n'
         '    return 42;')
# C5-02 (item 5.5): a local lost the C origin of its value; a local declared with `var` from a C `signed char` value now
# has that C type, as a C variable has its declared type. A local written `sbyte` is NativeSharp's 8-bit signed type,
# which goes back to C as `char` (item 5.5). / Un local `var` desde un valor C `signed char` tiene ese tipo C.
positivo(Caso('c5-generic-local', H2, S2),
         'var v = get_sc(); sbyte e = get_sc(); var c = get_c();\n'
         '    if (KIND(v) != 2 || KIND(e) != 1 || KIND(c) != 1 || v != -1) return 1;\n'
         '    return 42;')

H3 = r'''
extern int arr1[];
extern int arr1[4];
extern int arr2[3];
extern int arr2[];
'''
S3 = 'int arr1[4] = {1, 2, 3, 4};\nint arr2[3] = {10, 11, 12};\n'
c3 = Caso('c5-extern-arreglos', H3, S3)
positivo(c3, 'if (arr1.Length != 4 || arr2.Length != 3) return 1; return arr1[3] + arr2[2] + 26;')
rechazo(c3, 'return arr1[4];', 'outside [0, 4)')

H4 = r'''
#include <stdarg.h>
typedef struct BF { unsigned u3 : 3; int s5 : 5; unsigned long long w40 : 40; } BF;
int vtypes(int n, ...);
BF make_bf(void);
'''
S4 = r'''
int vtypes(int n, ...) { va_list ap; va_start(ap, n); int a = va_arg(ap, int); int b = va_arg(ap, int); long long c = va_arg(ap, long long); va_end(ap); return a + b + (int)(c >> 32); }
BF make_bf(void) { BF b; b.u3 = 5; b.s5 = -3; b.w40 = 40ull << 32; return b; }
'''
positivo(Caso('c5-bitfield-variadica', H4, S4), 'var b = make_bf(); return vtypes(3, b.u3, b.s5, b.w40);')
if not ONLY or any('c5-bitfield-ir' in o for o in ONLY):
    result = ir(Caso('c5-bitfield-ir', H4, S4), 'var b = make_bf(); return vtypes(3, b.u3, b.s5, b.w40);', opt=0)
    calls = [l for l in result.stdout.splitlines() if 'call' in l and '@vtypes' in l]
    if result.returncode or len(calls) != 1 or '(i32 3, i32 %' not in calls[0] or ', i32 %' not in calls[0] or \
            ', i64 %' not in calls[0]:
        _fail('c5-bitfield-ir', 'promociones inesperadas: ' + repr(calls))
    else:
        print('ok c5-bitfield-ir', flush=True)

H5 = '#include <setjmp.h>\nint value(void);\n'
rechazo(Caso('c5-setjmp', H5, 'int value(void) { return 1; }\n'),
        'byte* buf = stackalloc byte[256]; return _setjmp((void*)buf);', 'setjmp')

H6 = r'''
typedef struct cplx { int tag; } cplx;
typedef double _Complex cplx_t;
enum color : unsigned char { red = 1, green = 2, blue = 40 };
struct color_rec { enum color c; int pad; };
cplx_t mul2(cplx_t z);
int pick(enum color c, struct color_rec r, cplx s);
'''
S6 = r'''
cplx_t mul2(cplx_t z) { return z * 2.0; }
int pick(enum color c, struct color_rec r, cplx s) { return (int)c + (int)r.c + s.tag; }
'''
positivo(Caso('c5-nombres', H6, S6, standard='c23'),
         'cplx s; s.tag = 0; color_rec r; r.c = green; r.pad = 0;\n'
         '    var z = mul2(default(cplx_t)); return pick(blue, r, s) + (int)z.real;')

# C5-06: C separates tags from ordinary names (`enum mode` and `typedef int mode`); NativeSharp has one namespace, so
# naming the type is diagnosed, while calls that use both work.
H6b = r'''
enum mode : unsigned char { fast = 1, slow = 41 };
typedef int mode;
int use_mode(enum mode m, mode n);
'''
c6b = Caso('c5-nombres-etiqueta', H6b, 'int use_mode(enum mode m, mode n) { return (int)m + n; }\n', standard='c23')
positivo(c6b, 'return use_mode(slow, 1);')
# C5-06: naming the type was a rejection ("C/C++ type alias conflicts with a native type 'mode'"); as in C, the ordinary
# name `mode` written as a type is the typedef (`int`), while `enum mode` stays the tag of use_mode's first parameter.
# C5-06: nombrar el tipo era un rechazo; como en C, el nombre ordinario `mode` es el typedef.
S6b = 'int use_mode(enum mode m, mode n) { return (int)m + n; }\n'
positivo(Caso('c5-nombres-etiqueta-typedef', H6b, S6b, standard='c23'),
         'mode n = 1; int k = n + 0; return use_mode(slow, k);')

H7 = r'''
typedef struct P { int x, y; } P;
typedef int (__attribute__((sysv_abi)) *scb)(P p, int k);
int call_scb(scb f);
static inline __attribute__((sysv_abi)) int s_impl(P p, int k) { return p.x + p.y + k; }
'''
S7 = 'int call_scb(scb f) { P p = {30, 10}; return f(p, 2); }\n'
c7 = Caso('c5-sysv-callback', H7, S7)
positivo(c7, 'if (call_scb(s_impl) != 42) return 1; scb f = s_impl; P p; p.x = 30; p.y = 10; return f(p, 2);')
# C5-07: this lambda to a sysv_abi pointer with a record was a rejection ('sysv_abi function pointer is not yet
# supported'); the record now follows the Clang plan of the typedef (positive case). A precompiled template body names
# the typedef so its catalog carries it. / Esta lambda era un rechazo; el record sigue ahora el plan de Clang del typedef.
# NSMeta R1-05: a precompiled consumer without the header (lib) now imports this case through the producer's
# source session. / Un consumidor precompilado sin encabezado (lib) importa ahora este caso por la sesion de
# fuente del productor.
positivo(Caso('c5-sysv-lambda-record', H7, S7), 'return call_scb((p, k) => p.x + p.y + k);', modes=['directo', 'dll', 'lib'])
positivo(Caso('c5-sysv-lambda-typedef', H7, S7), 'scb f = (p, k) => p.x + p.y + k; return call_scb(f);', modes=['lib'])
# C5-07: a scalar sysv_abi callback (32/64-bit integers, floating values, bool, pointers) is a NativeSharp thunk of
# that convention. / Un callback sysv_abi escalar es un thunk NativeSharp de esa convencion.
H7b = 'typedef long long (__attribute__((sysv_abi)) *scalar_cb)(int a, double d, long long* p, _Bool f);\nlong long call_scalar(scalar_cb f);\n'
S7b = 'long long call_scalar(scalar_cb f) { long long v = 30; return f(5, 2.5, &v, 1); }\n'
positivo(Caso('c5-sysv-escalar', H7b, S7b), 'return (int)call_scalar((a, d, p, f) => a + (long)(d * 2) + *p + (f ? 2 : 0));')
# C5-07: records by value in registers (INTEGER/SSE pairs, <2 x float>), byval, sret and register returns, and narrow
# integers (extension), as Clang classifies them for the typedef. / Records en registros, byval, sret y enteros estrechos.
H7c = r'''
typedef struct P { int x, y; } P;
typedef struct M { double d; int i; } M;
typedef struct B { long long a, b, c; } B;
typedef struct F { float a, b, c; } F;
typedef P (__attribute__((sysv_abi)) *rcb)(M m, short s);
typedef B (__attribute__((sysv_abi)) *bcb)(B b, F f, unsigned char u);
int call_rcb(rcb f);
int call_bcb(bcb f);
'''
S7c = r'''
int call_rcb(rcb f) { M m = {2.5, 7}; P r = f(m, -3); return r.x * 10 + r.y; }
int call_bcb(bcb f) { B b = {1, 2, 3}; F x = {0.5f, 1.5f, 2.0f}; B r = f(b, x, 200); return (int)(r.a + r.b * 2 + r.c * 3); }
'''
c7c = Caso('c5-sysv-records', H7c, S7c)
BODY7C = ('rcb r = (m, s) => { P v; v.x = (int)(m.d * 2) + s; v.y = m.i; return v; };\n'
          'bcb b = (x, f, u) => { B v; v.a = x.a + u; v.b = x.b + (long)(f.a + f.b); v.c = x.c * (long)f.c; return v; };\n'
          'return call_rcb(r) == 27 && call_bcb(b) == 227 ? 42 : 1;')
positivo(c7c, BODY7C)
# C5-07: a `sysv_abi` pointer with a 128-bit integer was a rejection ("parameter 0 ('wide_cb') is not yet mapped");
# Clang passes and returns it as LLVM's direct `i128` (two integer registers, RAX:RDX), which x86_64_sysvcc lowers the
# same way, so a NativeSharp callback and a call through the pointer use it. / Un puntero `sysv_abi` con un entero de
# 128 bits era un rechazo; Clang lo pasa y devuelve como el `i128` directo de LLVM.
H7d = r'''
typedef long long (__attribute__((sysv_abi)) *wide_cb)(__int128 v);
typedef unsigned __int128 (__attribute__((sysv_abi)) *wide_r)(long long a, __int128 b);
long long call_wide(wide_cb f);
long long call_r(wide_r f);
wide_r get_r(void);
'''
S7d = r'''
long long call_wide(wide_cb f) { return f(((__int128)1 << 70) + 5) - ((long long)1 << 40); }
static unsigned __int128 __attribute__((sysv_abi)) twice(long long a, __int128 b) { return (unsigned __int128)(b * 2 + a); }
wide_r get_r(void) { return twice; }
long long call_r(wide_r f)
{
    unsigned __int128 v = f(2, ((__int128)3 << 64) + 20);
    return (long long)(v >> 64) * 100 + (long long)(unsigned long long)v;
}
'''
BODY7D = r'''
    wide_cb w = (v) => (long)(v >> 30);
    if (call_wide(w) != 0) return 1;
    wide_r r = (a, b) => (u128)(b + a);
    if (call_r(r) != 322) return 2;
    var g = get_r();
    if ((long)g(1, 20) != 41 || (long)(g(0, (i128)1 << 80) >> 64) != 1 << 17) return 3;
    return 42;
'''
positivo(Caso('c5-sysv-ancho', H7d, S7d), BODY7D)

SINGLE = r'''
#ifndef ONE_H
#define ONE_H
#include <stddef.h>
#include <stdint.h>
#include "one_extra.h"
int64_t one_sum(const int32_t* values, size_t n);
#ifdef ONE_IMPLEMENTATION
int64_t one_sum(const int32_t* values, size_t n) { int64_t s = ONE_BASE; for (size_t i = 0; i < n; ++i) s += values[i]; return s; }
#endif
#endif
'''
EXTRA = '#ifndef ONE_EXTRA_H\n#define ONE_EXTRA_H\n#define ONE_BASE 2\n#endif\n'


class CasoUnico(Caso):
    """A single-file library with a second user header next to it."""

    def native(self, folder):
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'one_extra.h').write_text(EXTRA, encoding='utf-8')
        return super().native(folder)


positivo(CasoUnico('c5-implementacion', SINGLE, None, extra_ref=' Implementation="ONE_IMPLEMENTATION"'),
         'int* v = stackalloc int[3]; v[0] = 10; v[1] = 20; v[2] = 10; return (int)one_sum(v, 3);')
fin()
