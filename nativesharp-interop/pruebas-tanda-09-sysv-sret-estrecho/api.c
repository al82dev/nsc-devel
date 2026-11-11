#include "api.h"
__declspec(selectany) int _fltused=0;
T09Result __attribute__((sysv_abi)) sysv_sret_narrow(float factor, T09Byte first, signed char small, int n) { T09Result result; result.x=(int)(first.x*2)+n; first.y+=5; result.y=first.y; result.z=(int)(first.x*10)+small; result.w=0; result.extra=0; if(factor!=1.5f) result.x=0; return result; }
