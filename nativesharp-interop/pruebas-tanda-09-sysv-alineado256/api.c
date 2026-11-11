#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvAlign256 __attribute__((sysv_abi)) sysv_align256(T09SysvAlign256 value) {
    value.x += 10;
    return value;
}
