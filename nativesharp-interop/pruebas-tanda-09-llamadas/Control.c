#include "api.h"
#ifdef API_DLL_IMPORT
__declspec(selectany) int _fltused = 0;
#endif
int main(void) {
    if (no_args() != 17) return 1;
    if (sum_scalars(3, 7) != 37) return 2;
    int value = 9;
    if (increment_pointer(&value) != 12 || value != 12) return 3;
    if (shared_value != 5) return 4;
    shared_value = 31;
    if (shared_value != 31) return 5;
    T09Record record; record.a = 3; record.b = 4; record.c = 5;
    if (check_variadic(11, (short)-2, 2.25f, (_Bool)1, &value, (signed char)-5,
                       (unsigned char)250, (unsigned short)65000, -5000000000LL,
                       4000000000U, T09EnumValue, record) != 1) return 6;
    T09Variadic call_indirect = check_variadic;
    if (call_indirect(11, (short)-2, 2.25f, (_Bool)1, &value, (signed char)-5,
                      (unsigned char)250, (unsigned short)65000, -5000000000LL,
                      4000000000U, T09EnumValue, record) != 1) return 7;
    return 0;
}
