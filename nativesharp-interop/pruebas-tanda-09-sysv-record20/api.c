#include "api.h"
T09SysvR20 __attribute__((sysv_abi)) sysv_r20(T09SysvR20 value) {
    value.x += 10;
    return value;
}
