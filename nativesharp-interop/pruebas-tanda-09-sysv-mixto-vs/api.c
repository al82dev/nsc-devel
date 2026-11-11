#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedVS __attribute__((sysv_abi)) sysv_mixed_vs(T09SysvMixedVS value) {
    value.x += 10;
    return value;
}
