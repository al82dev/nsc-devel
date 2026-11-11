#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvAlign16 __attribute__((sysv_abi)) sysv_align16(T09SysvAlign16 value) {
    value.x += 10;
    return value;
}
