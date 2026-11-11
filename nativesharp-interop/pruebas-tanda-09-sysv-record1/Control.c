#include "api.h"
int main(void) {
    T09SysvR1 original = {1};
    T09SysvR1 result = sysv_r1(original);
    if (original.x != 1 || result.x != 11) return 1;
    return 0;
}
