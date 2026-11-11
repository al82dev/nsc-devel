#include "api.h"
T09SysvR17 __attribute__((sysv_abi)) sysv_r17(T09SysvR17 value) {
    value.x += 10;
    return value;
}
