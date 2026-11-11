#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvAlign16Indirect original = {1, 2, 3.5f, 4.5f, 5};
    T09SysvAlign16Indirect result = sysv_align16_indirect(original);
    if (original.x != 1 || original.y != 2 || original.z != 3.5f || original.w != 4.5f || original.a != 5 || result.x != 11 || result.y != 2 || result.z != 3.5f || result.w != 4.5f || result.a != 5) return 1;
    return 0;
}
