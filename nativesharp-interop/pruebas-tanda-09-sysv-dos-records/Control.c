#include "api.h"
int main(void) {
    T09SysvTwo a = {1,2,3,4}, b = {5,6,7,8};
    T09SysvTwo r = sysv_two(a,b);
    if (a.x != 1 || b.x != 5 || r.x != 15 || r.y != 6 || r.z != 3 || r.w != 8) return 1;
    return 0;
}
