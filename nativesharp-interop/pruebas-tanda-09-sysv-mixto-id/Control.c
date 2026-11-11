#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvMixedID original = {1, 2, 3.5};
    T09SysvMixedID result = sysv_mixed_id(original);
    if (original.x != 1 || original.y != 2 || original.z != 3.5 || result.x != 11 || result.y != 2 || result.z != 3.5) return 1;
    return 0;
}
