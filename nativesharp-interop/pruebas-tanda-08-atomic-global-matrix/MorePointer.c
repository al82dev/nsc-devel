#include "MorePointer.h"
_Atomic(float*) more_float_ptr = 0;
_Atomic(long*) more_long_ptr = 0;
_Atomic(unsigned long*) more_ulong_ptr = 0;
static float f[2] = {1.25f, 2.5f};
static long l[2] = {11, 12};
static unsigned long u[2] = {21, 22};
float* more_float_expected(void) { return f; }
long* more_long_expected(void) { return l; }
unsigned long* more_ulong_expected(void) { return u; }
int more_pointer_check(void)
{
    return more_float_ptr == f + 1 && more_long_ptr == l + 1 && more_ulong_ptr == u + 1;
}
