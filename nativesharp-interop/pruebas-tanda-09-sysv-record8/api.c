#include "api.h"
T09SysvR8 __attribute__((sysv_abi)) sysv_r8(T09SysvR8 value) {
    value.x += 10;
    return value;
}
