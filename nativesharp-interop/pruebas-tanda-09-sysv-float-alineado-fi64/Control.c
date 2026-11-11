#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2}; T09Wide wide={11,13,17,19}; T09Out probe={1.5f,5000000000LL}; T09Out result=sysv_float_aligned_fi64(1.5f,first,probe,wide,7); if(first.x!=1.5 || first.y!=2 || wide.x!=11 || probe.a!=1.5f || probe.b!=5000000000LL || result.a!=2.5f || result.b!=5000000002LL) return 1; return 0; }
