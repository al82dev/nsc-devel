#include "api.h"
T09SysvR4 __attribute__((sysv_abi)) sysv_r4(T09SysvR4 value) {
    value.x += 10;
    return value;
}
