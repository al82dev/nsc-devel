#include "api.h"
int main(void) {
    T09SysvA2 original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    T09SysvA2 result = sysv_a2(original);
    if (original.x != 1 || original.y != 2 || original.z != 3 || original.a != 4 || original.b != 5 || original.c != 6 || original.d != 7 || original.e != 8 || original.f != 9 || original.g != 10 || result.x != 11 || result.y != 2 || result.z != 3 || result.a != 4 || result.b != 5 || result.c != 6 || result.d != 7 || result.e != 8 || result.f != 9 || result.g != 10) return 1;
    return 0;
}
