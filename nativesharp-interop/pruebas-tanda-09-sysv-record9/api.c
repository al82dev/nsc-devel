#include "api.h"
T09SysvR9 __attribute__((sysv_abi)) sysv_r9(T09SysvR9 value) {
    value.x += 10;
    return value;
}
