#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={11}; T09Wide wide={1.5f,2.5f}; short value=10; short* p=&value; short* result=sysv_shortp_record(1.5f,first,wide,p,7); if(result!=p) return 1; if(first.x!=11 || wide.x!=1.5f || wide.y!=2.5f) return 2; return 0; }
