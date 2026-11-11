#include "api.h"
T09SysvThreePairs __attribute__((sysv_abi)) sysv_three_pairs(T09SysvThreePairs first, T09SysvThreePairs second, T09SysvThreePairs third, int n) {
    first.x += second.x + third.x + n; first.h = third.h; return first;
}
