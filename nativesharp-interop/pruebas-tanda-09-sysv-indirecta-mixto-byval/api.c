#include "api.h"
__declspec(selectany) int _fltused = 0;
static int __attribute__((sysv_abi)) split_byval(int before, T09SplitValue small, T09ByvalValue large, int after) {
    int result = small.x == 1.5 ? before + 2 * small.y + large.a + large.b + large.c + large.d + large.e + after : -1;
    small.y = 99; large.a = 99;
    return result;
}
T09SplitByvalCallback t09_get_split_byval_callback(void) { return split_byval; }
