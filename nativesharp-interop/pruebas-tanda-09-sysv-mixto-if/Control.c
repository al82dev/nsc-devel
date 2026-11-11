#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvMixedIF original = {1, 2, 3.5f};
    T09SysvMixedIF result = sysv_mixed_if(original);
    if (original.x != 1 || original.y != 2 || original.z != 3.5f || result.x != 11 || result.y != 2 || result.z != 3.5f) return 1;
    return 0;
}
