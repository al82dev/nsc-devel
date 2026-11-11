#include "api.h"
T09SysvDifferentPairs __attribute__((sysv_abi)) sysv_different_pairs(T09SysvDifferentPairs first, T09Wide second, int n) {
    first.x += second.x + n; first.h = second.l; return first;
}
