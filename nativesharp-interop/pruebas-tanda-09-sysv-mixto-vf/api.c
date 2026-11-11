#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedVF __attribute__((sysv_abi)) sysv_mixed_vf(T09SysvMixedVF value) {
    value.x += 10;
    return value;
}
