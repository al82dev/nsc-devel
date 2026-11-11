#include "api.h"
T09SysvR3 __attribute__((sysv_abi)) sysv_r3(T09SysvR3 value) {
    value.x += 10;
    return value;
}
