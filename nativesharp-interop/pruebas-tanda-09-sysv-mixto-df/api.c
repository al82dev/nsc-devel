#include "api.h"
__declspec(selectany) int _fltused = 0;
T09SysvMixedDF __attribute__((sysv_abi)) sysv_mixed_df(T09SysvMixedDF value) {
    value.x += 10;
    return value;
}
