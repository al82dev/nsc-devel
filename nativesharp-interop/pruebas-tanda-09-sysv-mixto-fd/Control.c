#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvMixedFD original = {1.5f, 2};
    T09SysvMixedFD result = sysv_mixed_fd(original);
    if (original.x != 1.5f || original.y != 2 || result.x != 11.5f || result.y != 2) return 1;
    return 0;
}
