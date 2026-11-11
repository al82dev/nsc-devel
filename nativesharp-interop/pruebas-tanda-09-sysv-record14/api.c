#include "api.h"
T09SysvR14 __attribute__((sysv_abi)) sysv_r14(T09SysvR14 value) {
    value.x += 10;
    return value;
}
