#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={11}; T09Wide wide={1.5f,2.5f}; signed char result=sysv_narrow_sbyte_record(1.5f,first,wide,(signed char)-100,7); if(first.x!=11 || wide.x!=1.5f || wide.y!=2.5f || result!=(signed char)-100) return 1; return 0; }
