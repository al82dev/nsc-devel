#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2}; T09Wide wide={11,13,17,19}; T09Out probe={1.5f,2.5f,3.5f}; T09Out result=sysv_float_aligned_fv(1.5f,first,probe,wide,7); if(first.x!=1.5 || first.y!=2 || wide.x!=11 || probe.a!=1.5f || probe.b!=2.5f || probe.c!=3.5f || result.a!=2.5f || result.b!=4.5f || result.c!=6.5f) return 1; return 0; }
