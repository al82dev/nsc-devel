#include "api.h"
static T09OnlyResult __attribute__((sysv_abi)) only_result(int seed) {
    T09OnlyResult result = {seed, seed + 10, seed + 20};
    return result;
}
T09OnlyResultCallback t09_get_only_result_callback(void) { return only_result; }
