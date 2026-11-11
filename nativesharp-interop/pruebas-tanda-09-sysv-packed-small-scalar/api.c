#include "api.h"
__declspec(selectany) int _fltused=0;
int __attribute__((sysv_abi)) sysv_packed_small_scalar(float scale, T09Packed original, int extra) { int result=(int)original.tag+(int)original.value+extra+(int)scale; original.tag=0; return result; }
