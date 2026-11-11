#include "api.h"
static T09NoArgsResult __attribute__((sysv_abi)) noargs_result(void) {
    T09NoArgsResult result = {7, 17, 27};
    return result;
}
T09NoArgsCallback t09_get_noargs_callback(void) { return noargs_result; }
