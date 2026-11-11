#include "api.h"
T09SysvA8 __attribute__((sysv_abi)) sysv_a8(T09SysvA8 value) {
    value.x += 10;
    return value;
}
