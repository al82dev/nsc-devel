#include "api.h"
__declspec(selectany) int _fltused=0;
float __attribute__((sysv_abi)) sysv_return_float_mixed(float factor, T09Byte first, T09Wide wide, int n) { int r=(int)(first.x*2)+first.y+(int)(wide.x+wide.y+wide.z+wide.w)+n+4; wide.x=0; first.y=0; return factor==1.5f ? (float)r + 0.5f : 0; }
