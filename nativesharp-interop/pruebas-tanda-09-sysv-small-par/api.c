#include "api.h"
T09SysvSmallPair __attribute__((sysv_abi)) sysv_small_pair(T09Tiny second, T09SysvSmallPair first, int n) {
    first.x += second.x + n; first.h = second.w; return first;
}
