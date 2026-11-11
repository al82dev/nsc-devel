#include "api.h"
T09SysvR6 __attribute__((sysv_abi)) sysv_r6(T09SysvR6 value) {
    value.x += 10;
    return value;
}
