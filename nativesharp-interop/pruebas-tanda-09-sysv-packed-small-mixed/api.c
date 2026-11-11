#include "api.h"
__declspec(selectany) int _fltused=0;
T09Packed __attribute__((sysv_abi)) sysv_packed_small_mixed(float scale, T09Packed original, int extra) { original.tag+=(unsigned char)extra; original.value+=scale; return original; }
