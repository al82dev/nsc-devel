#include "api.h"
static int __attribute__((sysv_abi)) large_value(T09IndirectLarge value) {
    int result = value.a + 2 * value.b + 3 * value.c + 4 * value.d + 5 * value.e;
    value.a = 99;
    return result;
}
T09LargeCallback t09_get_large_callback(void) { return large_value; }
