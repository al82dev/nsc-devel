#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={11}; T09Wide wide={1.5f,2.5f};  _Bool result=sysv_bool_record(1.5f,first,wide,1,7); if(result!=0) return 1; if(first.x!=11 || wide.x!=1.5f || wide.y!=2.5f) return 2; return 0; }
