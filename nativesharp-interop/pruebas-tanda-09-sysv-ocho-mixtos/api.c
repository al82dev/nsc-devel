#include "api.h"
T09SysvMixed __attribute__((sysv_abi)) sysv_mixed(T09SysvMixed a, T09SysvMixed b, int c, T09SysvMixed d, int e, T09SysvMixed f, int g, T09SysvMixed h) {
    a.x += b.x*2 + c*3 + d.x*4 + e*5 + f.x*6 + g*7 + h.x*8; a.y = h.y; return a;
}
