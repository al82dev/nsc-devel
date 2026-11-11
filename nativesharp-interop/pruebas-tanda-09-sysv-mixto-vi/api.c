#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedVI __attribute__((sysv_abi)) sysv_mixed_vi(T09SysvMixedVI value) {
    value.x += 10;
    return value;
}
