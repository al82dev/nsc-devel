#include "api.h"
int main(void) {
    T09SysvA8 original = {1, 2, 3};
    T09SysvA8 result = sysv_a8(original);
    if (original.x != 1 || original.y != 2 || original.z != 3 || result.x != 11 || result.y != 2 || result.z != 3) return 1;
    return 0;
}
