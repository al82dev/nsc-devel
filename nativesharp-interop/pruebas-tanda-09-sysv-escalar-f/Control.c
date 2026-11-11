#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvScalarF original = {1.5f};
    T09SysvScalarF result = sysv_scalar_f(original);
    if (original.x != 1.5f || result.x != 11.5f) return 1;
    return 0;
}
