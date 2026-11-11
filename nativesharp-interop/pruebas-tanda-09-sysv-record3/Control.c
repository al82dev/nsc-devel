#include "api.h"
int main(void) {
    T09SysvR3 original = {1, 2, 3};
    T09SysvR3 result = sysv_r3(original);
    if (original.x != 1 || result.x != 11 || result.y != 2 || result.z != 3) return 1;
    return 0;
}
