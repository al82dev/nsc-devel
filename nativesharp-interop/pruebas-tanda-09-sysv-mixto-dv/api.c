#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedDV __attribute__((sysv_abi)) sysv_mixed_dv(T09SysvMixedDV value) {
    value.x += 10;
    return value;
}
