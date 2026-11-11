#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvScalarD __attribute__((sysv_abi)) sysv_scalar_d(T09SysvScalarD value) {
    value.x += 10;
    return value;
}
