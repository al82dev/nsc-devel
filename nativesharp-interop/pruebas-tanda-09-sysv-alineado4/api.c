#include "api.h"
T09SysvA4 __attribute__((sysv_abi)) sysv_a4(T09SysvA4 value) {
    value.x += 10;
    return value;
}
