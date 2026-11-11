#include "api.h"
T09SysvA2 __attribute__((sysv_abi)) sysv_a2(T09SysvA2 value) {
    value.x += 10;
    return value;
}
