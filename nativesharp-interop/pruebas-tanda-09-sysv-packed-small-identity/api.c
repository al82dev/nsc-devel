#include "api.h"
__declspec(selectany) int _fltused=0;
T09Packed __attribute__((sysv_abi)) sysv_packed_small_identity(T09Packed original) { original.tag+=1; original.value+=0.5; return original; }
