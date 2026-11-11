#include "api.h"
T09SysvR1 __attribute__((sysv_abi)) sysv_r1(T09SysvR1 value) {
    value.x += 10;
    return value;
}
