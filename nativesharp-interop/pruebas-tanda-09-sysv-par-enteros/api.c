#include "api.h"
T09SysvPairInts __attribute__((sysv_abi)) sysv_pair_ints(T09SysvPairInts value, unsigned int a, long long b, unsigned long long c) {
    value.x += a + (b >> 32) + (c >> 32);
    return value;
}
