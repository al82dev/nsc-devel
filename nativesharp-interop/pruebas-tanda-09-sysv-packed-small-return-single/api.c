#include "api.h"
__declspec(selectany) int _fltused=0;
T09Out __attribute__((sysv_abi)) sysv_packed_small_return_single(float factor, T09Byte first, T09Wide wide, int n) { T09Out result; result.x=(int)wide.y+n; first.x=0; wide.x=0; if(factor!=1.5f) result.x=0; return result; }
