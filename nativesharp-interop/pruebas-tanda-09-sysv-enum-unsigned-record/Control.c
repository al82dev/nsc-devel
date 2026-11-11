#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={11}; T09Wide wide={1.5f,2.5f}; if(sysv_enum_unsigned_record(1.5f,first,wide,T09A,7)!=T09A || sysv_enum_unsigned_record(1.5f,first,wide,T09B,7)!=T09B) return 1; if(first.x!=11 || wide.x!=1.5f || wide.y!=2.5f) return 2; return 0; }
