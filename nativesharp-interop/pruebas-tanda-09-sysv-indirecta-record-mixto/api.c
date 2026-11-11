#include "api.h"
__declspec(selectany) int _fltused = 0;
static int __attribute__((sysv_abi)) mixed_value(T09IndirectMixed value) {
    int result = value.x == 1.5 && value.y == 7 ? 42 : 0;
    value.x = 99.0; value.y = 99;
    return result;
}
T09MixedCallback t09_get_mixed_callback(void) { return mixed_value; }
