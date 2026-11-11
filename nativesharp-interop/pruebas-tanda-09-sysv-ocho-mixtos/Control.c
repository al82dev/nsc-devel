#include "api.h"
int main(void) {
    T09SysvMixed a = {1,2,3,4}; T09SysvMixed b = {2,3,3,4}; int c = 3; T09SysvMixed d = {4,5,3,4}; int e = 5; T09SysvMixed f = {6,7,3,4}; int g = 7; T09SysvMixed h = {8,9,3,4};
    T09SysvMixed r = sysv_mixed(a, b, c, d, e, f, g, h);
    if (a.x != 1 || h.x != 8 || r.x != 204 || r.y != 9 || r.z != 3 || r.w != 4) return 1;
    return 0;
}
