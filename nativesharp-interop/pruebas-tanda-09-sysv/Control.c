#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    if (sysv_mix(1, 2, 3, 4, 5, 6) != 91) return 1;
    sysv_set(41);
    if (sysv_value != 41) return 2;
    if (sysv_fp(1.5, 2.25f, 3, 4.0) != 10.75) return 3;
    if (sysv_float(2.5f) != 3.5f) return 4;
    return 0;
}
