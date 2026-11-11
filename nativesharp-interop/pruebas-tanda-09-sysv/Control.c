#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    if (sysv_mix(1, 2, 3, 4, 5, 6) != 91) return 1;
    sysv_set(41);
    if (sysv_value != 41) return 2;
    if (sysv_fp(1.5, 2.25f, 3, 4.0) != 10.75) return 3;
    if (sysv_float(2.5f) != 3.5f) return 4;
    int value = 10;
    int* result = sysv_pointer(&value, 3);
    if (result != &value || value != 13 || *result != 13) return 5;
    if (sysv_wide(-5000000000LL, 7U) != -4999999993LL) return 6;
    if (sysv_unsigned(4000000000U) != 4000000007U) return 7;
    if (sysv_unsigned64(100ULL) != 101ULL) return 8;
    if (sysv_s8(-7) != -6 || sysv_u8(250) != 251) return 9;
    if (sysv_s16(-1000) != -999 || sysv_u16(65000) != 65001) return 10;
    if (sysv_bool(1) != 0 || sysv_bool(0) != 1) return 11;
    return 0;
}
