#include "api.h"
__declspec(selectany) int _fltused=0;
T09Byte __attribute__((sysv_abi)) sysv_return_record_mixed(float factor, T09Byte first, T09Wide wide, int n) { first.x+=(double)wide.x; first.y+=(int)wide.y+n; wide.x=0; if(factor!=1.5f) first.y=0; return first; }
