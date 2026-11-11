#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1,2,3}; T09Wide second={31,32,33,34}; T09Result result=sysv_class_if(1.5f,first,7,second); if(first.x!=1 || first.y!=2 || second.y!=32 || result.x!=39 || result.y!=46 || result.z!=10) return 1; return 0; }
