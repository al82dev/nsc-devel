#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2}; T09Wide wide={11,13,17,19}; T09Out probe={3,2.5}; T09Out result=sysv_padded_pair_id(1.5f,first,probe,wide,7); if(first.x!=1.5 || first.y!=2 || wide.x!=11 || probe.a!=3 || probe.b!=2.5 || result.a!=5 || result.b!=13.5) return 1; return 0; }
