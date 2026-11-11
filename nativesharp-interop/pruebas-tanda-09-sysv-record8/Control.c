#include "api.h"
int main(void) {
    T09SysvR8 original = {1, 2, 3, 4, 5, 6, 7, 8};
    T09SysvR8 result = sysv_r8(original);
    if (original.x != 1 || original.y != 2 || original.z != 3 || original.w != 4 || original.a != 5 || original.b != 6 || original.c != 7 || original.d != 8 || result.x != 11 || result.y != 2 || result.z != 3 || result.w != 4 || result.a != 5 || result.b != 6 || result.c != 7 || result.d != 8) return 1;
    return 0;
}
