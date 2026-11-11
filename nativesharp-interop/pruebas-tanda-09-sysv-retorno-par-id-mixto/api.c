#include "api.h"
__declspec(selectany) int _fltused=0;
T09Out __attribute__((sysv_abi)) sysv_return_pair_id_mixed(float factor, T09Byte first, T09Wide wide, int n) { T09Out result; result.x=first.y+(int)wide.x; result.y=n+(int)wide.y; result.z=first.x+(double)wide.z; first.x=0; wide.x=0; if(factor!=1.5f) result.x=0; return result; }
