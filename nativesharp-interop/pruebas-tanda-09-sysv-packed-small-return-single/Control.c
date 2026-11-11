#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2}; T09Wide wide={11,13}; T09Out result=sysv_packed_small_return_single(1.5f,first,wide,7); if(first.x!=1.5 || first.y!=2 || wide.x!=11 || result.x!=20) return 1; return 0; }
