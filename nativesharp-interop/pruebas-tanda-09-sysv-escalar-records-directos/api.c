#include "api.h"
__declspec(selectany) int _fltused=0;
int __attribute__((sysv_abi)) sysv_scalar_direct_records(float factor, T09Byte first, T09Wide wide, int n) { int r=first.x+(int)(wide.x*10)+(int)(wide.y*10)+n; first.x=0; wide.x=0; wide.y=0; return factor==1.5f ? r : 0; }
