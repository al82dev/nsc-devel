#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvMixedDD original = {1.5, 2};
    T09SysvMixedDD result = sysv_mixed_dd(original);
    if (original.x != 1.5 || original.y != 2 || result.x != 11.5 || result.y != 2) return 1;
    return 0;
}
