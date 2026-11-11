#include "api.h"
int _fltused = 0;
int main(void) {
    int value = 123;
    if (sysv_varargs(3, (signed char)-2, (float)2.25f, -5000000000LL) != 1) return 1;
    if (sysv_many(9, (signed char)-7, (unsigned char)250, (short)-1000,
                  (unsigned short)65000, -5000000000LL, 4000000000U, &value, 2.25f, 3.5) != 1) return 2;
    return 0;
}
