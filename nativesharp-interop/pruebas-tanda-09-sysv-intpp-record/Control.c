#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={11}; T09Wide wide={1.5f,2.5f}; int value=10; int* inner=&value; int** p=&inner; int** result=sysv_intpp_record(1.5f,first,wide,p,7); if(result!=p) return 1; if(first.x!=11 || wide.x!=1.5f || wide.y!=2.5f) return 2; return 0; }
