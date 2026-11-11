#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvPair16Mix __attribute__((sysv_abi)) sysv_pair16_mix(T09SysvPair16Mix value, int a, float b) {
    value.x += a + b;
    return value;
}
