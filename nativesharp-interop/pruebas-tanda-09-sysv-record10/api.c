#include "api.h"
T09SysvR10 __attribute__((sysv_abi)) sysv_r10(T09SysvR10 value) {
    value.x += 10;
    return value;
}
