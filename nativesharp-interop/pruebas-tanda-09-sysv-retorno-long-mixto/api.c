#include "api.h"
__declspec(selectany) int _fltused=0;
long long __attribute__((sysv_abi)) sysv_return_long_mixed(float factor, T09Byte first, T09Wide wide, int n) { int r=(int)(first.x*2)+first.y+(int)(wide.x+wide.y+wide.z+wide.w)+n+4; wide.x=0; first.y=0; return factor==1.5f ? r + 5000000000LL : 0; }
