#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvScalarD original = {1.5};
    T09SysvScalarD result = sysv_scalar_d(original);
    if (original.x != 1.5 || result.x != 11.5) return 1;
    return 0;
}
