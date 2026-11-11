#include "api.h"
int main(void) {
    T09SysvEight a = {1,2,3,4}; T09SysvEight b = {2,3,3,4}; T09SysvEight c = {3,4,3,4}; T09SysvEight d = {4,5,3,4}; T09SysvEight e = {5,6,3,4}; T09SysvEight f = {6,7,3,4}; T09SysvEight g = {7,8,3,4}; T09SysvEight h = {8,9,3,4};
    T09SysvEight r = sysv_eight(a, b, c, d, e, f, g, h);
    if (a.x != 1 || h.x != 8 || r.x != 204 || r.y != 9 || r.z != 3 || r.w != 4) return 1;
    return 0;
}
