#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvMixedV original = {1, 2};
    T09SysvMixedV result = sysv_mixed_v(original);
    if (original.x != 1 || original.y != 2 || result.x != 11 || result.y != 2) return 1;
    return 0;
}
