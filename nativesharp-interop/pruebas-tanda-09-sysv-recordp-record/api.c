#include "api.h"
__declspec(selectany) int _fltused=0;
T09Byte* __attribute__((sysv_abi)) sysv_recordp_record(float factor, T09Byte first, T09Wide wide, T09Byte* p, int n) { if (!(factor==1.5f && first.x==11 && wide.x==1.5f && wide.y==2.5f && n==7)) return 0; first.x=0; wide.x=0; p->x+=3; return p; }
