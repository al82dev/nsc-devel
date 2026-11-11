#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedVD __attribute__((sysv_abi)) sysv_mixed_vd(T09SysvMixedVD value) {
    value.x += 10;
    return value;
}
