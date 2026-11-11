#include "api.h"
T09SysvEight __attribute__((sysv_abi)) sysv_eight(T09SysvEight a, T09SysvEight b, T09SysvEight c, T09SysvEight d, T09SysvEight e, T09SysvEight f, T09SysvEight g, T09SysvEight h) {
    a.x += b.x*2 + c.x*3 + d.x*4 + e.x*5 + f.x*6 + g.x*7 + h.x*8; a.y = h.y; return a;
}
