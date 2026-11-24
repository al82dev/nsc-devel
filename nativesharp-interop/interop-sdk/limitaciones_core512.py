"""Limitaciones Core: vectores C/C++ de 512 bits (C3-10, matriz "Vectores 512 bits").

Un vector de 512 bits por valor (`vector_size(64)` de float, double, int o long long; `__m512`) se mapea a su portador
de NativeSharp.Core.Numerics: Float32x16, Float64x8, Int32x16 e Int64x8 (structs secuenciales de 64 bytes, nuevos en
Core). Con cdecl la convencion Win64 pasa el vector por la direccion de una copia alineada y lo devuelve por memoria;
LLVM lo baja desde la misma declaracion `<N x T>` que Clang, asi que no requiere AVX-512 (esta maquina no lo tiene).
Con `__vectorcall` viajaria en registros ZMM: sigue rechazado con su motivo.

Requiere un Core que declare los portadores: NSC_CORE_ROOT apunta a una construccion de NativeSharp.Core con ellos
(la instalacion congelada no se sustituye). Directo, lib y dll; O0-O3.
"""
import os

from comun import Caso, positivo, rechazo, fin

H = r'''
typedef float f16v __attribute__((vector_size(64)));
typedef double d8v __attribute__((vector_size(64)));
typedef int i16v __attribute__((vector_size(64)));
typedef long long l8v __attribute__((vector_size(64)));
float sum_f16(f16v v);
f16v make_f16(float x);
double sum_d8(d8v v);
d8v scale_d8(d8v v, double k);
int sum_i16(i16v v);
long long sum_l8(l8v v);
l8v make_l8(long long x);
float __vectorcall vc_f16(f16v v);
'''
S = r'''
float sum_f16(f16v v) { float s = 0; for (int i = 0; i < 16; ++i) s += v[i]; return s; }
f16v make_f16(float x) { f16v r; for (int i = 0; i < 16; ++i) r[i] = x + i; return r; }
double sum_d8(d8v v) { double s = 0; for (int i = 0; i < 8; ++i) s += v[i]; return s; }
d8v scale_d8(d8v v, double k) { for (int i = 0; i < 8; ++i) v[i] *= k; return v; }
int sum_i16(i16v v) { int s = 0; for (int i = 0; i < 16; ++i) s += v[i]; return s; }
long long sum_l8(l8v v) { long long s = 0; for (int i = 0; i < 8; ++i) s += v[i]; return s; }
l8v make_l8(long long x) { l8v r; for (int i = 0; i < 8; ++i) r[i] = x * (i + 1); return r; }
float __vectorcall vc_f16(f16v v) { return v[0]; }
'''
PRE = 'using NativeSharp.Core.Numerics;\n'
c = Caso('core512-vectores', H, S)
# make_f16(1): lanes 1..16, sum 136. scale_d8 of lanes 1..8 by 0.5: sum 18. make_l8(1): 1..8, sum 36.
BODY = r'''
Float32x16 f = make_f16(1.0f);
if (sum_f16(f) != 136.0f) return 1;
if (f.L != 16.0f) return 2;
Float64x8 d;
d.X = 1; d.Y = 2; d.Z = 3; d.W = 4; d.A = 5; d.B = 6; d.C = 7; d.D = 8;
if (sum_d8(scale_d8(d, 0.5)) != 18.0) return 3;
Int32x16 n;
n.X = 1; n.Y = 1; n.Z = 1; n.W = 1; n.A = 1; n.B = 1; n.C = 1; n.D = 1;
n.E = 1; n.F = 1; n.G = 1; n.H = 1; n.I = 1; n.J = 1; n.K = 1; n.L = 27;
if (sum_i16(n) != 42) return 4;
Int64x8 l = make_l8(1);
if (sum_l8(l) != 36 || l.D != 8) return 5;
return 42;
'''
positivo(c, BODY, preludio=PRE, modes=['directo', 'lib', 'dll'])
rechazo(c, 'Float32x16 f; return (int)vc_f16(f);', 'ZMM', preludio=PRE)
fin()
