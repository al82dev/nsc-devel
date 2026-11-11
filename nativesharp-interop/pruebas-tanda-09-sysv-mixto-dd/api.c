#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedDD __attribute__((sysv_abi)) sysv_mixed_dd(T09SysvMixedDD value) {
    value.x += 10;
    return value;
}
