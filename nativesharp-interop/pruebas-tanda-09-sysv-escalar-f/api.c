#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvScalarF __attribute__((sysv_abi)) sysv_scalar_f(T09SysvScalarF value) {
    value.x += 10;
    return value;
}
