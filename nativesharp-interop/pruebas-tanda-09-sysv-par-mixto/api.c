#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvPairMix __attribute__((sysv_abi)) sysv_pair_mix(T09SysvPairMix value, int a, float b) {
    value.x += a + b;
    return value;
}
