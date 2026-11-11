#include "api.h"
T09SysvScalarPairs __attribute__((sysv_abi)) sysv_scalar_pairs(int n, T09SysvScalarPairs first, T09Wide second) {
    first.x += second.x + n; first.h = second.l; return first;
}
