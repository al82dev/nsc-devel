#include "api.h"
T09SysvR16 __attribute__((sysv_abi)) sysv_r16(T09SysvR16 value) {
    value.x += 10;
    return value;
}
