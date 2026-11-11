#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedV __attribute__((sysv_abi)) sysv_mixed_v(T09SysvMixedV value) {
    value.x += 10;
    return value;
}
