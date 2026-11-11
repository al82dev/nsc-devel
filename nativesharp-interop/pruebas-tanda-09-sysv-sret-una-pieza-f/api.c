#include "api.h"
__declspec(selectany) int _fltused=0;
T09Result __attribute__((sysv_abi)) sysv_sret_one_f(float factor, T09Byte first, int n) { T09Result result; result.x=(int)(first.x*2)+n; result.y=7; result.z=(int)(first.x*10); result.w=0; result.extra=0; first.x+=5; if(factor!=1.5f) result.x=0; return result; }
