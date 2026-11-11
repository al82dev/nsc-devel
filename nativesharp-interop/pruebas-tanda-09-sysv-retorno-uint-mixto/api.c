#include "api.h"
__declspec(selectany) int _fltused=0;
unsigned int __attribute__((sysv_abi)) sysv_return_uint_mixed(float factor, T09Byte first, T09Wide wide, int n) { int r=(int)(first.x*2)+first.y+(int)(wide.x+wide.y+wide.z+wide.w)+n+4; wide.x=0; first.y=0; return factor==1.5f ? (unsigned int)r + 4000000000U : 0; }
