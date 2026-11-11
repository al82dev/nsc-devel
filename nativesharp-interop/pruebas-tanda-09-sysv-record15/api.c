#include "api.h"
T09SysvR15 __attribute__((sysv_abi)) sysv_r15(T09SysvR15 value) {
    value.x += 10;
    return value;
}
