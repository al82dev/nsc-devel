#include "api.h"
T09SysvR24 __attribute__((sysv_abi)) sysv_r24(T09SysvR24 value) {
    value.x += 10;
    return value;
}
