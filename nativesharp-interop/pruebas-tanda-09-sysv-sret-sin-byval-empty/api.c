#include "api.h"
__declspec(selectany) int _fltused=0;
T09Result __attribute__((sysv_abi)) sysv_sret_empty(void) { T09Result result; result.x=10; result.y=7; result.z=15; result.w=0; result.extra=0; return result; }
