#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvAlign16Indirect __attribute__((sysv_abi)) sysv_align16_indirect(T09SysvAlign16Indirect value) {
    value.x += 10;
    return value;
}
