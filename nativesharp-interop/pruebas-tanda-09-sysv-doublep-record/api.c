#include "api.h"
__declspec(selectany) int _fltused=0;
double* __attribute__((sysv_abi)) sysv_doublep_record(float factor, T09Byte first, T09Wide wide, double* p, int n) { if (!(factor==1.5f && first.x==11 && wide.x==1.5f && wide.y==2.5f && n==7)) return 0; first.x=0; wide.x=0; *p+=0.5; return p; }
