"""Limitaciones C3: tipos y ABI C (plan de limitaciones, tareas C3-01..C3-10).

C3-01: `_Float16` es NativeSharp.Core.Numerics.Half (IEEE binary16, `ushort Bits`). Campos, punteros y globales
comparten el almacenamiento; por valor, una importacion C lo pasa y devuelve en XMM como `half` de LLVM, igual que
Clang en Windows x64. Un callback con `_Float16` pasa el valor en XMM, como C.
"""
from comun import Caso, positivo, rechazo, ir, fin, _fail, ONLY

H1 = r'''
_Float16 half_add(_Float16 a, _Float16 b);
_Float16 half_from(float v);
float half_to(_Float16 v);
typedef struct Sample { int id; _Float16 gain; } Sample;
float sample_gain(const Sample* s);
void half_store(_Float16* out, float v);
typedef _Float16 (*half_cb)(_Float16);
int with_half_cb(half_cb cb);
int vread(int n, ...);
'''
S1 = r'''
_Float16 half_add(_Float16 a, _Float16 b) { return a + b; }
_Float16 half_from(float v) { return (_Float16)v; }
float half_to(_Float16 v) { return (float)v; }
float sample_gain(const Sample* s) { return (float)s->gain; }
void half_store(_Float16* out, float v) { *out = (_Float16)v; }
int with_half_cb(half_cb cb) { return (int)(float)cb((_Float16)1.0f); }
#include <stdarg.h>
int vread(int n, ...) { va_list l; va_start(l, n); double d = va_arg(l, double); va_end(l); return (int)(d * 4.0); }
'''
BODY1 = r'''
    var a = half_from(1.5f);
    var b = half_from(2.25f);
    if ((int)(half_to(half_add(a, b)) * 8.0f) != 30) return 1;
    Sample s; s.id = 7; s.gain = half_from(0.5f);
    if ((int)(sample_gain(&s) * 4.0f) != 2) return 2;
    var h = half_from(0.0f);
    half_store(&h, 6.0f);
    if ((int)half_to(h) != 6 || h.Bits != 17920) return 3;
    return 42;
'''
c1 = Caso('c3-float16', H1, S1)
positivo(c1, BODY1)
# C3-01: a callback with `_Float16` was a rejection; its value now travels in XMM as C passes it (the thunk and a call
# through the pointer reinterpret the Half carrier as LLVM `half`). / Un callback con `_Float16` era un rechazo; ahora su
# valor viaja en XMM como lo pasa C.
positivo(Caso('c3-float16-callback', H1, S1),
         'half_cb f = (h) => half_from(half_to(h) * 21.0f); if ((int)half_to(f(half_from(2.0f))) != 42) return 1;\n'
         'return with_half_cb(f) * 2;')
# C3-01: C promotes `_Float16` to double in `...`; a Half there was a rejection ('vread'), and now reaches the call as
# that double, as C passes it. / C promueve `_Float16` a double en `...`; un Half alli era un rechazo y ahora llega como
# ese double.
positivo(Caso('c3-float16-variadica-implicita', H1, S1), 'var a = half_from(10.5f); return vread(1, a);')
positivo(Caso('c3-float16-variadica', H1, S1), 'var a = half_from(10.5f); return vread(1, (double)NativeSharp.Core.Numerics.Half.ToFloat(a)) == 42 ? 42 : 1;', modes=['directo'])
# An explicit cast converts between Half and numbers, as C converts `_Float16` (a cast failed in the backend before).
positivo(Caso('c3-float16-cast', H1, S1), r'''
    var a = (NativeSharp.Core.Numerics.Half)2.75f;
    var b = (NativeSharp.Core.Numerics.Half)7;
    var c = (NativeSharp.Core.Numerics.Half)1.5;
    if ((int)a != 2 || (float)a != 2.75f || (double)b != 7.0 || (long)b != 7L || (float)c != 1.5f) return 1;
    return (int)half_to(half_add(b, (NativeSharp.Core.Numerics.Half)35u));''')

if not ONLY or any('c3-ir' in o for o in ONLY):
    result = ir(Caso('c3-ir', H1, S1), 'var a = half_from(1.5f); return (int)half_to(half_add(a, a));')
    text = result.stdout
    if result.returncode or 'half @half_add(half' not in text.replace('"', '') or 'call half @half_from' not in text:
        _fail('c3-ir', 'la llamada no usa `half`:\n' + '\n'.join(l for l in text.splitlines() if 'half' in l)[:2500])
    else:
        print('ok c3-ir', flush=True)

# C3-02: `_BitInt(N)` narrower than its carrier travels as `iN`, as Clang declares it: a result is extended by the
# carrier's signedness (the upper register bits are undefined), an argument is truncated (C's conversion to
# `_BitInt(N)`). Values live in the carrier (`ushort`, `short`, `long`...) and follow its arithmetic in NativeSharp.
H2 = r'''
unsigned _BitInt(12) u12_add(unsigned _BitInt(12) a, unsigned _BitInt(12) b);
_BitInt(12) s12(int v);
int s12_to_int(_BitInt(12) v);
unsigned _BitInt(40) u40_shift(unsigned _BitInt(40) v);
_BitInt(100) wide(void);
typedef int (*bit_cb)(_BitInt(12));
int with_bit_cb(bit_cb cb);
'''
S2 = r'''
unsigned _BitInt(12) u12_add(unsigned _BitInt(12) a, unsigned _BitInt(12) b) { return a + b; }
_BitInt(12) s12(int v) { return (_BitInt(12))v; }
int s12_to_int(_BitInt(12) v) { return (int)v; }
unsigned _BitInt(40) u40_shift(unsigned _BitInt(40) v) { return v << 4; }
_BitInt(100) wide(void) { return 1; }
int with_bit_cb(bit_cb cb) { return cb(1); }
'''
BODY2 = r'''
    if (u12_add(4000, 100) != 4) return 1;
    if (s12(-5) != -5 || s12(2048) != -2048) return 2;
    if (s12_to_int(4095) != -1) return 3;
    if (u40_shift(0xF000000000UL) != 0) return 4;
    if (u40_shift(0x0800000001UL) != 0x8000000010UL) return 5;
    ushort wrapped = 4100;
    if (u12_add(wrapped, 0) != 4) return 6;
    return 42;
'''
c2 = Caso('c3-bitint', H2, S2, standard='c2x')
positivo(c2, BODY2)
# C3-02: `wide()` (a `_BitInt(100)` result) was a rejection; with 64 < N < 128 a function value is carried as the
# 128-bit integer, passed by the address of a copy and returned through memory as Clang's plan says, extended from N
# bits; an argument is truncated to N bits (C's conversion). Record fields of that kind: see c3-bitint-campo.
# C3-02: `wide()` era un rechazo; con 64 < N < 128 el valor de una funcion se lleva como el entero de 128 bits.
H2b = r'''
_BitInt(100) wide(void);
_BitInt(100) w_add(_BitInt(100) a, _BitInt(100) b);
unsigned _BitInt(72) u72_shl(unsigned _BitInt(72) v);
__int128 beyond(void);
unsigned __int128 top72(void);
int w_is_neg(_BitInt(100) v);
'''
S2b = r'''
_BitInt(100) wide(void) { return 1; }
_BitInt(100) w_add(_BitInt(100) a, _BitInt(100) b) { return a + b; }
unsigned _BitInt(72) u72_shl(unsigned _BitInt(72) v) { return v << 8; }
__int128 beyond(void) { return ((__int128)1 << 100) + 5; }
unsigned __int128 top72(void) { return (unsigned __int128)0xFF << 64; }
int w_is_neg(_BitInt(100) v) { return v < 0; }
'''
BODY2b = r'''
    if ((int)wide() != 1) return 1;
    if ((int)w_add(40, 2) != 42 || (int)w_add(-50, 8) != -42) return 2;
    if ((int)w_add(beyond(), 37) != 42) return 3;
    if ((int)u72_shl(1) != 256 || u72_shl(top72()) != 0) return 4;
    if (w_is_neg(w_add(-1, 0)) != 1 || w_is_neg(w_add(1, 0)) != 0) return 5;
    return 42;
'''
positivo(Caso('c3-bitint-ancho', H2b, S2b, standard='c2x'), BODY2b)
# C3-02: a record field `_BitInt(N)` with 64 < N < 128 was not imported ('NativeSharp has no mapping for that field
# type'); it is read and written as the 128-bit integer at Clang's offset and alignment (8 on Windows x64, not 16).
# C3-02: un campo `_BitInt(N)` ancho no se importaba; ahora se accede como el entero de 128 bits en el offset de Clang.
H2c = r'''
struct W { char c; _BitInt(100) w; unsigned _BitInt(72) u; int t; };
_BitInt(100) get_w(struct W* p);
int sizeof_w(void);
void fill(struct W* p);
'''
S2c = r'''
_BitInt(100) get_w(struct W* p) { return p->w; }
int sizeof_w(void) { return sizeof(struct W); }
void fill(struct W* p) { p->c = 1; p->w = -42; p->u = 7; p->t = 9; }
'''
BODY2c = r'''
    W x; fill(&x); if ((int)sizeof(W) != sizeof_w()) return 1;
    if ((int)x.w != -42 || (int)x.u != 7 || x.t != 9 || x.c != 1) return 2; x.w = 40; x.t = 2;
    return (int)get_w(&x) + x.t;
'''
positivo(Caso('c3-bitint-campo', H2c, S2c, standard='c2x'), BODY2c)
# T19-002: a `sizeof` constant (`nuint`) compared with an `int` value was accepted but lowered as `icmp i64, i32`, which
# the LLVM verifier rejected; it now compares in `int`, the type the constant converts to.
# T19-002: una constante `sizeof` comparada con un `int` se bajaba como `icmp i64, i32`; ahora compara en `int`.
positivo(Caso('c3-sizeof-comparacion', H2c, S2c, standard='c2x'),
         'int k = 4; if (sizeof(W) != sizeof_w() || sizeof_w() != sizeof(W)) return 1;\n'
         '    if (sizeof(int) != k || k < sizeof(int) || sizeof(W) <= k) return 2; return 42;')
# C3-02: a callback with a `_BitInt(N)` narrower than its carrier was a rejection ('with_bit_cb' not imported); N is
# now part of the C function type identity: a call through the pointer passes `iN` and extends an `iN` result by the
# carrier's sign, and a lambda reaches the pointer through a thunk that extends each `iN` (whose upper register bits are
# undefined) and truncates its result. A `_BitInt(N)` with N > 64 in a callback stays a rejection.
# C3-02: un callback con `_BitInt(N)` estrecho era un rechazo; ahora N forma parte de la identidad del tipo funcion C.
positivo(Caso('c3-bitint-lambda', H2, S2, standard='c2x'), 'return with_bit_cb((v) => (int)v + 41);')
H2cb = r'''
typedef int (*bit_cb)(_BitInt(12));
typedef unsigned _BitInt(7) (*u7_cb)(unsigned _BitInt(7), int);
int with_bit_cb(bit_cb cb);
int with_u7_cb(u7_cb cb);
_BitInt(12) neg12(_BitInt(12) v);
bit_cb get_cb(void);
u7_cb get_u7(void);
typedef int (*wide_cb)(_BitInt(100));
int with_wide(wide_cb cb);
'''
S2cb = r'''
int with_bit_cb(bit_cb cb) { return cb(-5) + cb(2047); }
int with_u7_cb(u7_cb cb) { unsigned _BitInt(7) r = cb(100, 3); return (int)r; }
_BitInt(12) neg12(_BitInt(12) v) { return -v; }
static int plus(_BitInt(12) v) { return (int)v + 1; }
static unsigned _BitInt(7) add7(unsigned _BitInt(7) a, int b) { return a + (unsigned _BitInt(7))b * 10; }
bit_cb get_cb(void) { return plus; }
u7_cb get_u7(void) { return add7; }
int with_wide(wide_cb cb) { return cb(1); }
'''
positivo(Caso('c3-bitint-callback', H2cb, S2cb), r'''
    bit_cb f = (v) => (int)v * 2;
    if (with_bit_cb(f) != 4084) return 1;
    u7_cb g = (a, b) => (byte)(a + b);
    if (with_u7_cb(g) != 103) return 2;
    var p = get_cb();
    if (p((short)-7) != -6) return 3;
    var q = get_u7();
    if ((int)q((byte)120, 1) != 2) return 4;
    return 42;
''', modes=['directo', 'lib', 'dll'])
# C3-02: a `_BitInt(N)` with 64 < N < 128 in a callback was the remaining rejection ('with_wide'); it now travels as
# Clang passes it: by the address of a copy extended from N and returned through memory (sret), carried as the 128-bit
# integer, with N in the function type identity. / Un `_BitInt(N)` ancho en un callback era el rechazo restante; ahora
# viaja como lo pasa Clang: por la direccion de una copia extendida y devuelto en memoria.
H2cw = r'''
typedef _BitInt(100) (*wide_cb)(_BitInt(100), int);
typedef unsigned _BitInt(90) (*uwide_cb)(unsigned _BitInt(90));
long long with_wide(wide_cb cb);
long long with_uwide(uwide_cb cb);
wide_cb get_wide(void);
uwide_cb get_uwide(void);
'''
S2cw = r'''
long long with_wide(wide_cb cb) {
  _BitInt(100) big = ((_BitInt(100))1 << 98) + 5;
  _BitInt(100) r = cb(big, 2);
  if (r != (((_BitInt(100))1 << 98) + 7) * 1) return -1;
  return (long long)cb(-40, 1);
}
long long with_uwide(uwide_cb cb) {
  unsigned _BitInt(90) top = ((unsigned _BitInt(90))1 << 89);
  unsigned _BitInt(90) r = cb(top);
  return r == 1 ? 7 : (long long)(r >> 60);
}
static _BitInt(100) twice(_BitInt(100) v, int k) { return v * k; }
static unsigned _BitInt(90) dec(unsigned _BitInt(90) v) { return v - 1; }
wide_cb get_wide(void) { return twice; }
uwide_cb get_uwide(void) { return dec; }
'''
positivo(Caso('c3-bitint-callback-ancho', H2cw, S2cw), r'''
    wide_cb f = (v, k) => v + k;
    if (with_wide(f) != -39) return 1;
    uwide_cb g = (v) => v + v + 1;
    if (with_uwide(g) != 7) return 2;
    var p = get_wide();
    if (p((i128)(-21), 2) != (i128)(-42)) return 3;
    i128 big = (i128)1 << 99;
    if (p(big, 2) != 0) return 4;
    var q = get_uwide();
    if (q((u128)0) != (((u128)1 << 90) - 1)) return 5;
    return 42;
''', modes=['directo', 'lib', 'dll'])

# C3-03: C23 enums with a fixed base (8-64 bits, signed and unsigned) are NativeSharp enums. An enum over a 128-bit
# integer is carried as that integer (its positions pass like __int128); its enumerators are i128 constants when they
# fit 64 bits, and a wider one reaches NativeSharp with its exact value through `#define E E`. Before, such an enum
# made the whole header's type provider invalid.
H3 = r'''
enum Small : unsigned char { S_A = 1, S_B = 255 };
enum Neg : short { N_A = -3, N_B = 4 };
enum Big : __int128 { B_SMALL = 7, B_HUGE = (__int128)1 << 100, B_NEG = -((__int128)1 << 90) };
int small_v(enum Small s);
int neg_v(enum Neg n);
int big_shift(enum Big b, int shift);
enum Big big_make(int shift);
'''
S3 = r'''
int small_v(enum Small s) { return (int)s; }
int neg_v(enum Neg n) { return (int)n; }
int big_shift(enum Big b, int shift) { return (int)(b >> shift); }
enum Big big_make(int shift) { return (enum Big)((__int128)1 << shift); }
'''
BODY3 = r'''
    if (small_v(S_B) != 255 || neg_v(N_A) != -3 || (int)N_B != 4) return 1;
    if (big_shift(B_SMALL, 0) != 7) return 2;
    if (big_shift(B_HUGE, 98) != 4 || big_shift(B_NEG, 89) != -2) return 3;
    if (big_shift(big_make(70), 69) != 2) return 4;
    return 42;
'''
positivo(Caso('c3-enums', H3, S3, standard='c2x'), BODY3)

# C3-04: a C `_Atomic T` field (T scalar or pointer) is imported: every load and store is atomic and its address is a
# pointer to atomic storage (`atomic_fetch_add(&s->count, 1)`); ref/in/out cannot reach it.
H4 = r'''
#include <stdatomic.h>
typedef struct Counter { int id; _Atomic int count; _Atomic long long total; _Atomic(int*) last; } Counter;
Counter* counter(void);
int bump(_Atomic int* value);
int read_plain(int* value);
'''
S4 = r'''
static Counter c = {1, 0, 0, 0};
Counter* counter(void) { return &c; }
int bump(_Atomic int* value) { return atomic_fetch_add(value, 1) + 1; }
int read_plain(int* value) { return *value; }
'''
BODY4 = r'''
    Counter* c = counter();
    c->count = 10;
    c->count += 5;
    if (c->count != 15) return 1;
    if (bump(&c->count) != 16 || c->count != 16) return 2;
    c->total = 1L << 40;
    if (c->total != 1099511627776L) return 3;
    int x = 26;
    c->last = &x;
    if (*c->last + c->count != 42) return 4;
    return 42;
'''
c4 = Caso('c3-atomico', H4, S4, standard='c11')
positivo(c4, BODY4)
rechazo(c4, 'Counter* c = counter(); return read_plain(&c->count);', 'read_plain')
rechazo(c4, 'Counter* c = counter(); return bump(ref c->count);', 'ref')

# C3-05: a bit-field reached through volatile storage (`Regs volatile*`, the MMIO pattern) is accessed through its
# Microsoft storage unit (here the 32-bit `unsigned int` unit), as Clang does; a non-volatile access keeps the
# field's window. A volatile field already used its unit (T19-044).
H5 = r'''
typedef struct Regs { unsigned char tag; unsigned int ready : 1; unsigned int mode : 3; unsigned int level : 12; } Regs;
volatile Regs* regs(void);
int regs_mode(void);
'''
S5 = r'''
static Regs r = {9, 0, 0, 0};
volatile Regs* regs(void) { return &r; }
int regs_mode(void) { return (int)r.mode + (int)r.level; }
'''
BODY5 = r'''
    var r = regs();
    r->mode = 5;
    r->level = 37;
    if (r->mode != 5 || r->ready != 0u || r->tag != 9) return 1;
    return regs_mode();
'''
c5 = Caso('c3-bitfield-volatile', H5, S5)
positivo(c5, BODY5)
if not ONLY or any('c3-bitfield-ir' in o for o in ONLY):
    result = ir(Caso('c3-bitfield-ir', H5, S5), 'var r = regs(); r->mode = 5; return (int)r->level;', opt=0)
    text = result.stdout
    lines = [l for l in text.splitlines() if 'volatile' in l and ('load' in l or 'store' in l)]
    if result.returncode or not lines or any(' i8' in l for l in lines) or not any(' i32' in l for l in lines):
        _fail('c3-bitfield-ir', 'el acceso volatile no usa la unidad de 32 bits:\n' + '\n'.join(lines)[:2000])
    else:
        print('ok c3-bitfield-ir', flush=True)

# C3-06: a parameter of a `transparent_union` type whose members are pointers takes an argument of any member type,
# passed with the ABI of the first member, as Clang does (the `__SOCKADDR_ARG` pattern). Other types are rejected.
H6 = r'''
struct sockaddr { int family; };
struct sockaddr_in { int family; int port; };
typedef union { struct sockaddr* sa; struct sockaddr_in* in; } __attribute__((transparent_union)) SockArg;
int family_of(SockArg arg);
'''
S6 = 'int family_of(SockArg arg) { return arg.sa->family; }\n'
BODY6 = r'''
    sockaddr_in a; a.family = 40; a.port = 1;
    sockaddr b; b.family = 2;
    return family_of(&a) + family_of(&b);
'''
c6 = Caso('c3-union-transparente', H6, S6)
positivo(c6, BODY6)
rechazo(c6, 'int x = 1; return family_of(&x);', 'family_of')

# C3-07: a flexible array member whose element is an array (`int rows[][4]`) is imported like `T x[]`: indexed past
# any length, its rows are `int[4]` with their own bound, and it decays to `int (*)[4]` / `int rows[][4]`. A final
# `T x[0]` is flexible (item 4.5); a final `T x[1]` stays a one-element array (not every final array is flexible).
H7 = r'''
typedef struct Grid { int n; int rows[][4]; } Grid;
typedef struct Z { int n; int data[0]; } Z;
typedef struct One { int n; int data[1]; } One;
typedef struct M { int m[3][4]; } M;
Grid* grid(void); Z* zeros(void); One* one(void); M* mat(void);
int sum_rows(int rows[][4], int count);
int sum_ptr(int (*rows)[4], int count);
'''
S7 = r'''
static int storage[64];
Grid* grid(void) { Grid* g = (Grid*)storage; g->n = 2; for (int i = 0; i < 8; ++i) g->rows[i/4][i%4] = i; return g; }
Z* zeros(void) { Z* z = (Z*)storage; z->n = 3; z->data[0] = 5; z->data[1] = 6; z->data[2] = 7; return z; }
One* one(void) { static One o = {1, {9}}; return &o; }
M* mat(void) { static M m; for (int i = 0; i < 12; ++i) m.m[i/4][i%4] = i; return &m; }
int sum_rows(int rows[][4], int count) { int s = 0; for (int i = 0; i < count; ++i) for (int j = 0; j < 4; ++j) s += rows[i][j]; return s; }
int sum_ptr(int (*rows)[4], int count) { return sum_rows(rows, count); }
'''
BODY7 = r'''
    var g = grid();
    if (g->rows[1][3] != 7 || sum_rows(g->rows, 2) != 28 || sum_ptr(g->rows, 2) != 28) return 1;
    g->rows[1][0] = 14;
    if (sum_rows(g->rows, 2) != 38) return 2;
    var z = zeros();
    if (z->data[2] != 7) return 3;
    var o = one();
    if (o->data[0] != 9) return 4;
    var m = mat();
    if (sum_rows(m->m, 3) != 66 || sum_ptr(m->m, 3) != 66) return 5;
    return 42;
'''
c7 = Caso('c3-flexible-arreglos', H7, S7)
positivo(c7, BODY7)
rechazo(c7, 'var o = one(); return o->data[1];', 'outside [0, 1)')
rechazo(c7, 'var g = grid(); return g->rows[0][4];', 'outside [0, 4)')

# C3-08: a pointer to an unprototyped C function (`int (*)()`) takes any arguments with C's default promotions
# (`1.5f` reaches a `double` parameter); `static inline` `sysv_abi` functions with record values keep Clang's plan
# under their link wrapper, as external ones do; and a record built from scalars (`P make(int, int)`) survives the
# precompiled metadata. An unprototyped `sysv_abi` pointer is called as a variadic sysv call (below); one of another
# convention stays unmapped.
H8 = r'''
typedef struct P { int x; int y; } P;
typedef struct Big { long long a, b, c; } Big;
typedef int (*kr)();
typedef int (__attribute__((sysv_abi)) *krs)();
kr get_kr(void);
kr get_krd(void);
krs get_krs(void);
__attribute__((sysv_abi)) P exm(int x, int y);
static inline __attribute__((sysv_abi)) int sx(P p) { return p.x * 10 + p.y; }
static inline __attribute__((sysv_abi)) Big sbig(Big b) { Big r = {b.a + 1, b.b + 1, b.c + 1}; return r; }
static inline __attribute__((sysv_abi)) P smk(int x, int y) { P p = {x, y}; return p; }
'''
S8 = r'''
static int kr_impl(int a, int b) { return a + b; }
static int krd_impl(double d, int k) { return (int)(d * 4.0) + k; }
static int __attribute__((sysv_abi)) krs_impl(void) { return 1; }
kr get_kr(void) { return (kr)kr_impl; }
kr get_krd(void) { return (kr)krd_impl; }
krs get_krs(void) { return (krs)krs_impl; }
P exm(int x, int y) { P p = {x, y}; return p; }
'''
BODY8 = r'''
    var f = get_kr();
    if (f(40, 2) != 42) return 1;
    var d = get_krd();
    if (d(1.5f, 36) != 42 || d(2.5, 32) != 42) return 2;
    P p; p.x = 4; p.y = 2;
    if (sx(p) != 42) return 3;
    Big b; b.a = 1; b.b = 2; b.c = 36;
    var r = sbig(b);
    if (r.a + r.b + r.c != 42) return 4;
    var m = smk(4, 2);
    var e = exm(4, 2);
    if (m.x * 10 + m.y != 42 || e.x * 10 + e.y != 42) return 5;
    return 42;
'''
c8 = Caso('c3-sin-prototipo-sysv', H8, S8)
positivo(c8, BODY8)
# C3-08: an unprototyped `sysv_abi` pointer was a rejection ('get_krs'); a call through it now applies C's default
# promotions and passes the arguments as a variadic sysv_abi call (same registers; AL holds the vector count, as a
# variadic callee needs). / Un puntero `sysv_abi` sin prototipo era un rechazo; ahora se llama como una variadica sysv.
H8b = 'typedef int (__attribute__((sysv_abi)) *krs)();\nkrs get_krs(void);\nkrs get_krs2(void);\n'
S8b = ('static int __attribute__((sysv_abi)) krs_impl(void) { return 1; }\n'
       'static int __attribute__((sysv_abi)) krs2_impl(int a, double d, int b) { return a + (int)(d * 2) + b; }\n'
       'krs get_krs(void) { return (krs)krs_impl; }\nkrs get_krs2(void) { return (krs)krs2_impl; }\n')
positivo(Caso('c3-sin-prototipo-sysv-puntero', H8b, S8b), 'var f = get_krs(); var g = get_krs2(); return f() + g(30, 1.5f, 8);')

# C3-09: C11 `atomic_thread_fence` / `atomic_signal_fence` (functions no library defines; C reaches them through
# macros to Clang builtins) become NativeSharp.Core.Threading.AtomicFence: `consume` is `acquire`, a `relaxed`
# fence is a compiler-only fence, a signal fence is single-thread. A run-time order switches as in Clang. Other system
# macros stay as they are. The NativeSharp alternative, AtomicFence.Thread/Compiler, is verified too.
import comun
from pathlib import Path
H9 = '#include <stdatomic.h>\nint value(void);\n'
S9 = 'int value(void) { return 42; }\n'
c9 = Caso('c3-barreras', H9, S9)
positivo(c9, 'atomic_thread_fence(memory_order_seq_cst); atomic_thread_fence(memory_order_relaxed); '
             'atomic_thread_fence(memory_order_consume); atomic_signal_fence(memory_order_acquire); return value();')
# C3-09: a run-time order was a rejection ('constant memory_order'); it now switches over the order as Clang does
# (consume/acquire -> acquire, release, acq_rel, seq_cst, anything else no fence; a signal fence is single-thread).
# Un orden en ejecucion era un rechazo; ahora se recorre con un switch como en Clang.
positivo(Caso('c3-barreras-dinamicas', H9, S9),
         'int o = 5; atomic_thread_fence((memory_order)o); atomic_signal_fence((memory_order)(o - 3)); '
         'atomic_thread_fence((memory_order)0); return value();')
if not ONLY or any('c3-barreras-dinamicas-ir' in o for o in ONLY):
    result = ir(Caso('c3-barreras-dinamicas-ir', H9, S9),
                'int o = 4; atomic_thread_fence((memory_order)o); atomic_signal_fence((memory_order)o); return value();',
                opt=0)
    fences = [l.strip() for l in result.stdout.splitlines() if l.strip().startswith('fence')]
    expected = ['fence acquire', 'fence release', 'fence acq_rel', 'fence seq_cst',
                'fence syncscope("singlethread") acquire', 'fence syncscope("singlethread") release',
                'fence syncscope("singlethread") acq_rel', 'fence syncscope("singlethread") seq_cst']
    if result.returncode or fences != expected or result.stdout.count('switch i32') != 2:
        _fail('c3-barreras-dinamicas-ir', 'barreras inesperadas: ' + repr(fences))
    else:
        print('ok c3-barreras-dinamicas-ir', flush=True)
if not ONLY or any('c3-barreras-ir' in o for o in ONLY):
    result = ir(Caso('c3-barreras-ir', H9, S9), 'atomic_thread_fence(memory_order_seq_cst); '
                'atomic_thread_fence(memory_order_consume); atomic_signal_fence(memory_order_release); return value();', opt=0)
    fences = [l.strip() for l in result.stdout.splitlines() if l.strip().startswith('fence')]
    if result.returncode or fences != ['fence seq_cst', 'fence acquire', 'fence syncscope("singlethread") release'] or \
            'call void @atomic_' in result.stdout:
        _fail('c3-barreras-ir', 'barreras inesperadas: ' + repr(fences))
    else:
        print('ok c3-barreras-ir', flush=True)
_core = comun.core
def _core_threading(opt):
    folder = Path(comun.CORE_ROOT) / f'O{opt}'
    return _core(opt) + (f'<LibraryReference Include="{folder / "NativeSharpCoreThreading.lib"}" '
                         f'Metadata="{folder / "NativeSharpCoreThreading.nsmeta"}" />')
comun.core = _core_threading
positivo(Caso('c3-barreras-core', H9, S9),
         'NativeSharp.Core.Threading.AtomicFence.Thread(NativeSharp.Core.Threading.MemoryOrder.SequentiallyConsistent); '
         'NativeSharp.Core.Threading.AtomicFence.Compiler(NativeSharp.Core.Threading.MemoryOrder.Acquire); return value();')
comun.core = _core

# C3-10: vector fields of C records (a vector aggregate) are imported as lane views, as in C++ records: an HVA
# (`{ v4f a, b, c, d; }`) passes and returns in XMM0-3 under `__vectorcall` and by address under cdecl, with Clang's
# declaration shape; arrays of vectors in a record keep their stride. Records through `__vectorcall` and cdecl
# function pointers are called as Clang does. 512-bit vectors map to the Core carriers (limitaciones_core512.py); the
# 256-bit ones are covered with AVX by pruebas-tanda-19-traslados/validar-vectores-c.py --avx.
H10 = r'''
typedef float v4f __attribute__((vector_size(16)));
typedef float v16f __attribute__((vector_size(64)));
typedef struct H4 { v4f a, b, c, d; } H4;
typedef struct AV { int tag; v4f lanes[2]; } AV;
typedef struct R { int a; int b; } R;
typedef struct H2d { double x, y; } H2d;
typedef int (__vectorcall *vcr)(R r, H2d h);
float __vectorcall vh4(H4 h);
H4 __vectorcall mkh4(float f);
float cdh4(H4 h);
float sum_av(const AV* p);
void fill_av(AV* p);
vcr get_vcr(void);
float cd16(v16f v);
'''
S10 = r'''
float __vectorcall vh4(H4 h) { return h.a[0] + h.b[1] + h.c[2] + h.d[3]; }
H4 __vectorcall mkh4(float f) { H4 h; for (int i = 0; i < 4; ++i) { h.a[i] = f; h.b[i] = f + 1; h.c[i] = f + 2; h.d[i] = f + 3; } return h; }
float cdh4(H4 h) { return h.a[0] + h.b[1] + h.c[2] + h.d[3]; }
float sum_av(const AV* p) { float s = p->tag; for (int i = 0; i < 2; ++i) for (int j = 0; j < 4; ++j) s += p->lanes[i][j]; return s; }
void fill_av(AV* p) { p->tag = 2; for (int i = 0; i < 2; ++i) for (int j = 0; j < 4; ++j) p->lanes[i][j] = 5.0f; }
static int __vectorcall vcr_impl(R r, H2d h) { return r.a + r.b + (int)(h.x + h.y); }
vcr get_vcr(void) { return vcr_impl; }
float cd16(v16f v) { return v[0]; }
'''
BODY10 = r'''
    var h = mkh4(9.0f);
    if ((int)vh4(h) != 42 || (int)cdh4(h) != 42) return 1;
    AV v; fill_av(&v);
    if (v.lanes[1][3] != 5.0f) return 2;
    v.lanes[0][0] = 6.0f;
    if ((int)sum_av(&v) != 43) return 3;
    var f = get_vcr(); R r; r.a = 20; r.b = 20; H2d d; d.x = 1.0; d.y = 1.0;
    return f(r, d);
'''
c10 = Caso('c3-vectores-agregados', H10, S10)
positivo(c10, BODY10)
# Core 512-bit vectors: the rejection was 'only 128-bit and 256-bit vectors are mapped'. A 512-bit vector now maps to its
# Core carrier (Float32x16, limitaciones_core512.py); with a Core that does not declare it, the import says which one.
# Vectores de 512 bits: ahora se mapean a su portador de Core; con un Core que no lo declara, la importacion dice cual.
rechazo(c10, 'return (int)cd16();', 'it needs NativeSharp.Core.Numerics.Float32x16')
if not ONLY or any('c3-vectores-ir' in o for o in ONLY):
    result = ir(Caso('c3-vectores-ir', H10, S10), 'var h = mkh4(9.0f); return (int)vh4(h) + (int)cdh4(h);', opt=0)
    decls = {l.split('@', 1)[1].split('(')[0].strip('"\01'): l for l in result.stdout.splitlines() if l.startswith('declare')}
    hva = '{ <4 x float>, <4 x float>, <4 x float>, <4 x float> }'
    ok = (result.returncode == 0 and any('vh4@@64' in k and 'x86_vectorcallcc' in d and hva + ' inreg' in d for k, d in decls.items())
          and any('mkh4@@8' in k and 'x86_vectorcallcc ' + hva in d for k, d in decls.items())
          and any(k == 'cdh4' and '(ptr' in d for k, d in decls.items()))
    if not ok:
        _fail('c3-vectores-ir', 'forma inesperada:\n' + '\n'.join(decls.values())[:2500])
    else:
        print('ok c3-vectores-ir', flush=True)
fin()
