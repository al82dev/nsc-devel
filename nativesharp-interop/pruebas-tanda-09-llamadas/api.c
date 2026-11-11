#include "api.h"
__declspec(selectany) int _fltused = 0;
int shared_value = 5;
int no_args(void) { return 17; }
int sum_scalars(int a, int b) { return a * 10 + b; }
int increment_pointer(int *p) { *p += 3; return *p; }
int check_variadic(int count, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, count);
    int narrow = __builtin_va_arg(ap, int);
    double floating = __builtin_va_arg(ap, double);
    int boolean = __builtin_va_arg(ap, int);
    int *pointer = __builtin_va_arg(ap, int *);
    int signed_byte = __builtin_va_arg(ap, int);
    int unsigned_byte = __builtin_va_arg(ap, int);
    int unsigned_short = __builtin_va_arg(ap, int);
    long long wide = __builtin_va_arg(ap, long long);
    unsigned int unsigned_wide = __builtin_va_arg(ap, unsigned int);
    int enumeration = __builtin_va_arg(ap, int);
    T09Record record = __builtin_va_arg(ap, T09Record);
    __builtin_va_end(ap);
    return count == 11 && narrow == -2 && floating == 2.25 &&
           boolean == 1 && pointer && *pointer == 12 && signed_byte == -5 &&
           unsigned_byte == 250 && unsigned_short == 65000 && wide == -5000000000LL &&
           unsigned_wide == 4000000000U && enumeration == 7 &&
           record.a == 3 && record.b == 4 && record.c == 5;
}
