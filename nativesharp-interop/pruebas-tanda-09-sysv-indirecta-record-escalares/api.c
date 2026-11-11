#include "api.h"
__declspec(selectany) int _fltused = 0;
static int __attribute__((sysv_abi)) mixed_args(int before, T09MixedArgs value, int after) {
    int result = value.x == 1.5 ? before + value.y * 2 + after * 3 : -1;
    value.x = 99.0; value.y = 99;
    return result;
}
T09MixedArgsCallback t09_get_mixed_args_callback(void) { return mixed_args; }
int t09_mixed_args_next(void) { static int value; return ++value; }
