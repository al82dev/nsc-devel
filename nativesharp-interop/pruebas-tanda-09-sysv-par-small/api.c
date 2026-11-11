#include "api.h"
T09SysvPairSmall __attribute__((sysv_abi)) sysv_pair_small(T09SysvPairSmall first, T09Tiny second, int n) {
    first.x += second.x + n; first.h = second.w; return first;
}
