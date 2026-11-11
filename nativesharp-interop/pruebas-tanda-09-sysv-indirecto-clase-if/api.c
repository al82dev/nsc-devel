#include "api.h"
__declspec(selectany) int _fltused=0;
T09Result __attribute__((sysv_abi)) sysv_class_if(float factor, T09Byte first, int n, T09Wide second) {
T09Result result; result.w=0; result.extra=0; result.x=first.x+second.x+n; first.y+=5; second.y+=7; result.y=first.y+second.y; result.z=(int)(first.x*10)+(int)first.z-3; if(factor!=1.5f) result.x=0; return result;
}
