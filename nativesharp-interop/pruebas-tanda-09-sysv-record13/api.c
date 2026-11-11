#include "api.h"
T09SysvR13 __attribute__((sysv_abi)) sysv_r13(T09SysvR13 value) {
    value.x += 10;
    return value;
}
