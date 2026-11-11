#include "api.h"
__declspec(selectany) int _fltused=0;
int main(void) {
    T09SysvIndirectMix first={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    T09SysvIndirectMix second={31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54};
    T09SysvIndirectMix result=sysv_indirect_mix(1.5f,first,7,second);
    if (first.x!=1 || second.x!=31 || first.y!=2 || second.y!=32 || result.x!=39 || result.y!=35 || result.t!=24) return 1;
    return 0;
}
