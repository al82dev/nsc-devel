#include "api.h"
T09SysvTwoPairs __attribute__((sysv_abi)) sysv_two_pairs(T09SysvTwoPairs first, T09SysvTwoPairs second, int n) {
    first.x += second.x + n; first.h = second.h; return first;
}
