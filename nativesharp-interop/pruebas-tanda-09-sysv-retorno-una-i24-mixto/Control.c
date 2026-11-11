#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2}; T09Wide wide={11,13,17,19}; T09Out result=sysv_return_one_i24_mixed(1.5f,first,wide,7); if(first.x!=1.5 || first.y!=2 || wide.x!=11 || result.a!=11 || result.b!=2 || result.c!=7) return 1; return 0; }
