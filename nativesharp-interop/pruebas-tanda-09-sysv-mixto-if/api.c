#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedIF __attribute__((sysv_abi)) sysv_mixed_if(T09SysvMixedIF value) {
    value.x += 10;
    return value;
}
