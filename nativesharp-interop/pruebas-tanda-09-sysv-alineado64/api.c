#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvAlign64 __attribute__((sysv_abi)) sysv_align64(T09SysvAlign64 value) {
    value.x += 10;
    return value;
}
