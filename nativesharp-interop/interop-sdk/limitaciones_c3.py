"""Limitaciones C3: tipos y ABI C (plan de limitaciones, tareas C3-01..C3-10).

C3-01: `_Float16` es NativeSharp.Core.Numerics.Half (IEEE binary16, `ushort Bits`). Campos, punteros y globales
comparten el almacenamiento; por valor, una importacion C lo pasa y devuelve en XMM como `half` de LLVM, igual que
Clang en Windows x64. Un callback con `_Float16` queda sin mapear (su thunk usaria un registro general).
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
rechazo(c1, 'return with_half_cb(null);', 'with_half_cb')
# C promotes `_Float16` to double in `...`; a Half is not passed there implicitly (write (double)Half.ToFloat(h)).
rechazo(c1, 'var a = half_from(1.0f); return vread(1, a);', 'vread')
positivo(Caso('c3-float16-variadica', H1, S1), 'var a = half_from(10.5f); return vread(1, (double)NativeSharp.Core.Numerics.Half.ToFloat(a)) == 42 ? 42 : 1;', modes=['directo'])

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
rechazo(c2, 'return (int)wide();', 'wide')
rechazo(c2, 'return with_bit_cb(null);', 'with_bit_cb')

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
fin()
