#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedDI __attribute__((sysv_abi)) sysv_mixed_di(T09SysvMixedDI value) {
    value.x += 10;
    return value;
}
