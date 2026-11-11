#include "api.h"
T09SysvSmallInts __attribute__((sysv_abi)) sysv_small_ints(T09SysvSmallInts value, unsigned int a, long long b, unsigned long long c) {
    value.x += a + (b >> 32) + (c >> 32);
    return value;
}
