#include "api.h"
_Static_assert(sizeof(T09Out) == 16 && _Alignof(T09Out) == 8, "unexpected T09Out layout");
__declspec(selectany) int _fltused=0;
T09Out __attribute__((sysv_abi)) sysv_padded_integers_i8(float factor, T09Byte first, T09Out probe, T09Wide wide, int n) { T09Out result; result.a=probe.a+2; result.b=probe.b+2; probe.a=0; wide.x=0; if(factor!=1.5f || first.x!=1.5 || first.y!=2 || n!=7) result.a=0; return result; }
