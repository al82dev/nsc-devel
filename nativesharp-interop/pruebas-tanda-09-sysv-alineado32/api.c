#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvAlign32 __attribute__((sysv_abi)) sysv_align32(T09SysvAlign32 value) {
    value.x += 10;
    return value;
}
