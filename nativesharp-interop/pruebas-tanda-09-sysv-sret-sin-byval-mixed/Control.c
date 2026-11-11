#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2};  T09Result result=sysv_sret_mixed(1.5f,first,7); if (first.x!=1.5 || first.y!=2 || result.x!=10 || result.y!=7 || result.z!=15) return 1; return 0; }
