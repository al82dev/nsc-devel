#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) { T09Byte first={1.5,2,4};  T09Result result=sysv_sret_double_i64(1.5f,first,7); if (first.x!=1.5 || first.y!=2 || first.z!=4 || result.x!=10 || result.y!=7 || result.z!=19) return 1; return 0; }
