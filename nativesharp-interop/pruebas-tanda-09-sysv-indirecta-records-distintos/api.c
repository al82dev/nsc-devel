#include "api.h"
__declspec(selectany) int _fltused = 0;
static int __attribute__((sysv_abi)) distinct(T09MixedDistinct first, T09TripleDistinct second, int tail) {
    int result = first.x == 1.5 ? first.y + 2 * second.x + 3 * second.y + 4 * second.z + tail : -1;
    first.x = 99.0; second.y = 99;
    return result;
}
T09DistinctCallback t09_get_distinct_callback(void) { return distinct; }
