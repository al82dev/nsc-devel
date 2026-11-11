#include "api.h"
__declspec(selectany) int _fltused = 0;
static int __attribute__((sysv_abi)) mixed_pair(T09MixedPair first, T09MixedPair second, int tail) {
    int result = first.x == 1.5 && second.x == 2.5 ? first.y + 2 * second.y + 3 * tail : -1;
    first.x = 99.0; second.y = 99;
    return result;
}
T09MixedPairCallback t09_get_mixed_pair_callback(void) { return mixed_pair; }
