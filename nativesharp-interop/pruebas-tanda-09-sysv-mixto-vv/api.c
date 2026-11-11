#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedVV __attribute__((sysv_abi)) sysv_mixed_vv(T09SysvMixedVV value) {
    value.x += 10;
    return value;
}
