#include "Api.h"
_Atomic(unsigned int) value_u32 = 3;
_Atomic(long long) value_i64 = 4;
_Atomic(unsigned long long) value_u64 = 5;
_Atomic(signed char) value_i8 = -6;
_Atomic(unsigned short) value_u16 = 7;
_Atomic(float) value_f32 = 1.25f;
_Atomic(double) value_f64 = 2.5;
_Atomic(_Bool) value_bool = 0;
_Atomic(int*) value_pointer = 0;
static int items[2] = {73, 74};
int* expected_pointer(void) { return items; }
int verify_values(void)
{
    return value_u32 == 42 && value_i64 == 43 && value_u64 == 44 &&
        value_i8 == -45 && value_u16 == 46 && value_f32 == 3.5f &&
        value_f64 == 4.5 && value_bool && value_pointer == items &&
        *value_pointer == 73;
}
