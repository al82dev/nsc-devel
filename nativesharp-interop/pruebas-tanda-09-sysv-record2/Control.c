#include "api.h"
int main(void) {
    T09SysvR2 original = {1, 2};
    T09SysvR2 result = sysv_r2(original);
    if (original.x != 1 || original.y != 2 || result.x != 11 || result.y != 2) return 1;
    return 0;
}
