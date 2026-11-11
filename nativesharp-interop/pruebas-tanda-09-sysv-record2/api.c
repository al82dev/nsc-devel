#include "api.h"
T09SysvR2 __attribute__((sysv_abi)) sysv_r2(T09SysvR2 value) {
    value.x += 10;
    return value;
}
