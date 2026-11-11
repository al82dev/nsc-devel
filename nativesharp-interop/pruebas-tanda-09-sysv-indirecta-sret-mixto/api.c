#include "api.h"
__declspec(selectany) int _fltused = 0;
static T09SretLarge __attribute__((sysv_abi)) sret_mixed(int before, T09SretSplit small, T09SretLarge large, int after) {
    large.a += before; large.b += small.x == 1.5 ? small.y : 100; large.e += after;
    small.y = 99;
    return large;
}
T09SretMixedCallback t09_get_sret_mixed_callback(void) { return sret_mixed; }
