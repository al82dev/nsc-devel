#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedIV __attribute__((sysv_abi)) sysv_mixed_iv(T09SysvMixedIV value) {
    value.x += 10;
    return value;
}
