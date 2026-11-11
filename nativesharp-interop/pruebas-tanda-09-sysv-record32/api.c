#include "api.h"
T09SysvR32 __attribute__((sysv_abi)) sysv_r32(T09SysvR32 value) {
    value.x += 10;
    return value;
}
