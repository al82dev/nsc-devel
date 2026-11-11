#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedFD __attribute__((sysv_abi)) sysv_mixed_fd(T09SysvMixedFD value) {
    value.x += 10;
    return value;
}
