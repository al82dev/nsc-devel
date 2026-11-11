#include "api.h"
T09SysvR7 __attribute__((sysv_abi)) sysv_r7(T09SysvR7 value) {
    value.x += 10;
    return value;
}
