#include "MoreQualified.h"
_Atomic(const float*) more_float_const = 0;
_Atomic(volatile float*) more_float_volatile = 0;
_Atomic(const long*) more_long_const = 0;
_Atomic(volatile long*) more_long_volatile = 0;
_Atomic(const unsigned long*) more_ulong_const = 0;
_Atomic(volatile unsigned long*) more_ulong_volatile = 0;
static float f[2] = {1.25f, 2.5f};
static long l[2] = {11, 12};
static unsigned long u[2] = {21, 22};
float* qualified_float_expected(void) { return f; }
long* qualified_long_expected(void) { return l; }
unsigned long* qualified_ulong_expected(void) { return u; }
int qualified_pointer_check(void)
{
    return more_float_const == f && more_float_volatile == f && f[1] == 3.5f &&
        more_long_const == l && more_long_volatile == l && l[1] == 13 &&
        more_ulong_const == u && more_ulong_volatile == u && u[1] == 23;
}
