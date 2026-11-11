#include "api.h"
T09SysvR12 __attribute__((sysv_abi)) sysv_r12(T09SysvR12 value) {
    value.x += 10;
    return value;
}
