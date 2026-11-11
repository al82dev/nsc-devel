#include "api.h"
T09SysvR5 __attribute__((sysv_abi)) sysv_r5(T09SysvR5 value) {
    value.x += 10;
    return value;
}
