#include "api.h"
__declspec(selectany) int _fltused=0;
T09Out __attribute__((sysv_abi)) sysv_return_one_i24_mixed(float factor, T09Byte first, T09Wide wide, int n) { T09Out result; result.a=(unsigned char)wide.x; result.b=(unsigned char)first.y; result.c=(unsigned char)n; first.x=0; wide.x=0; if(factor!=1.5f) result.a=0; return result; }
