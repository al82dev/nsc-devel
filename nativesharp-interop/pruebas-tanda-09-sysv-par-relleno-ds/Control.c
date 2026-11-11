#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2}; T09Wide wide={11,13,17,19}; T09Out probe={2.5,3}; T09Out result=sysv_padded_pair_ds(1.5f,first,probe,wide,7); if(first.x!=1.5 || first.y!=2 || wide.x!=11 || probe.a!=2.5 || probe.b!=3 || result.a!=13.5 || result.b!=5) return 1; return 0; }
