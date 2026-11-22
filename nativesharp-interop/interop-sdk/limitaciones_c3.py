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
fin()
