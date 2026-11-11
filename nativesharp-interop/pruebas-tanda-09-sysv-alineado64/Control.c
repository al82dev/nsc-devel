#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvAlign64 original = {1, 2, 3.5f, 4.5f};
    T09SysvAlign64 result = sysv_align64(original);
    if (original.x != 1 || original.y != 2 || original.z != 3.5f || original.w != 4.5f || result.x != 11 || result.y != 2 || result.z != 3.5f || result.w != 4.5f) return 1;
    return 0;
}
