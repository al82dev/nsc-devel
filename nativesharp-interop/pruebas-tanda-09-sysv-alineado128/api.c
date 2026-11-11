#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvAlign128 __attribute__((sysv_abi)) sysv_align128(T09SysvAlign128 value) {
    value.x += 10;
    return value;
}
