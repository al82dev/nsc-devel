#include "api.h"
static T09LargeReturn __attribute__((sysv_abi)) large_return(T09LargeReturn value) {
    value.a += 10; value.b += 20; value.c += 30; value.d += 40; value.e += 50;
    return value;
}
T09LargeReturnCallback t09_get_large_return_callback(void) { return large_return; }
