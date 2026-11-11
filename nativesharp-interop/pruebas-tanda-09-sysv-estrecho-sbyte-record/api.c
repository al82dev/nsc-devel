#include "api.h"
__declspec(selectany) int _fltused=0;
signed char __attribute__((sysv_abi)) sysv_narrow_sbyte_record(float factor, T09Byte first, T09Wide wide, signed char small, int n) { int valid=factor==1.5f && first.x==11 && wide.x==1.5f && wide.y==2.5f && n==7; first.x=0; wide.x=0; return valid ? small : 0; }
