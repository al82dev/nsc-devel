#include "api.h"
int main(void) {
    T09SysvR4 original = {1, 2, 3, 4};
    T09SysvR4 result = sysv_r4(original);
    if (original.x != 1 || original.y != 2 || original.z != 3 || original.w != 4 ||
        result.x != 11 || result.y != 2 || result.z != 3 || result.w != 4) return 1;
    return 0;
}
