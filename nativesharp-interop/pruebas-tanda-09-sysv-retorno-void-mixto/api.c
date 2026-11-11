#include "api.h"
__declspec(selectany) int _fltused=0;
void __attribute__((sysv_abi)) sysv_return_void_mixed(float factor, T09Byte first, T09Wide wide, int n, int* out) { int r=(int)(first.x*2)+first.y+(int)(wide.x+wide.y+wide.z+wide.w)+n+4; wide.x=0; first.y=0; *out=factor==1.5f ? r : 0; }
