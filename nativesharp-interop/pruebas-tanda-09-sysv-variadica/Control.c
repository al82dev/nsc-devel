#include "api.h"
int _fltused = 0;
int main(void) {
    int value = 123;
    if (sysv_varargs(3, (signed char)-2, (float)2.25f, -5000000000LL) != 1) return 1;
    if (sysv_many(9, (signed char)-7, (unsigned char)250, (short)-1000,
                  (unsigned short)65000, -5000000000LL, 4000000000U, &value, 2.25f, 3.5) != 1) return 2;
    short s = -1234; T09VarRecord record = {42, 0}; signed char b = -12;
    if (sysv_ptrs(3, &s, &record, &b) != 1 || s != 1234 || record.x != 43 || b != 12) return 3;
    if (sysv_empty(0) != 1) return 4;
    if (sysv_floatspill(9, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 2.25f, 3.5) != 1) return 5;
    s = -1234; record.x = 42;
    if (sysv_fixed_ptrs(&record, &s, (signed char)-7, 2.25f) != 1 || s != 1234 || record.x != 43) return 6;
    if (sysv_return_ptr(&record, 7) != &record || record.x != 44) return 7;
    return 0;
}
