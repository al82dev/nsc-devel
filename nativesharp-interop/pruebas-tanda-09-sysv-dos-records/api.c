#include "api.h"
T09SysvTwo __attribute__((sysv_abi)) sysv_two(T09SysvTwo first, T09SysvTwo second) {
    first.x += 10 + second.x - 1; first.y = second.y; first.w = second.w; return first;
}
