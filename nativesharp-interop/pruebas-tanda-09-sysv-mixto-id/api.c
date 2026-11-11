#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedID __attribute__((sysv_abi)) sysv_mixed_id(T09SysvMixedID value) {
    value.x += 10;
    return value;
}
