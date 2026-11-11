#include "api.h"
__declspec(selectany) int _fltused=0;
T09Out __attribute__((sysv_abi)) sysv_float_aligned_i8f(float factor, T09Byte first, T09Out probe, T09Wide wide, int n) { T09Out result; result.a=probe.a+2; result.b=probe.b+1.0f; probe.a=0; wide.x=0; if(factor!=1.5f || first.x!=1.5 || first.y!=2 || n!=7) result.a=0; return result; }
