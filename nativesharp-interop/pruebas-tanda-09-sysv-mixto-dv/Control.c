#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvMixedDV original = {1.5, 2, 3};
    T09SysvMixedDV result = sysv_mixed_dv(original);
    if (original.x != 1.5 || original.y != 2 || original.z != 3 || result.x != 11.5 || result.y != 2 || result.z != 3) return 1;
    return 0;
}
