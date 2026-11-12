#include "api.h"
static T09ReturnRecord __attribute__((sysv_abi)) return_record(T09ReturnRecord value) {
    value.x += 10; value.y += 20; value.z += 30; value.w += 40;
    return value;
}
T09ReturnCallback t09_get_return_callback(void) { return return_record; }
