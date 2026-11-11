#include "api.h"
T09SysvR11 __attribute__((sysv_abi)) sysv_r11(T09SysvR11 value) {
    value.x += 10;
    return value;
}
