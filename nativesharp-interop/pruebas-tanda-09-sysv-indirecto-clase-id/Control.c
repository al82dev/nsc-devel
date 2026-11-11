#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={4294967297LL,2.5}; T09Wide second={31,32,33,34}; T09Result result=sysv_class_id(1.5f,first,7,second); if(first.x!=4294967297LL || first.y!=2.5 || second.y!=32 || result.x!=39 || result.y!=46 || result.z!=10) return 1; return 0; }
