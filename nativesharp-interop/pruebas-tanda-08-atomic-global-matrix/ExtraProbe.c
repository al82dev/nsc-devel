#include "Extra.h"
void probe_extra_write(void)
{
    extra_u8 = 42; extra_i16 = -43;
    extra_u32_ptr = expected_u32(); extra_i16_ptr = expected_i16();
    extra_i64_ptr = expected_i64(); extra_f64_ptr = expected_f64();
    extra_const_ptr = expected_int(); extra_volatile_ptr = expected_int();
}
int probe_extra_read(void)
{
    return extra_u8 == 42 && extra_i16 == -43 && extra_u32_ptr == expected_u32() &&
        extra_i16_ptr == expected_i16() && extra_i64_ptr == expected_i64() &&
        extra_f64_ptr == expected_f64() && extra_const_ptr == expected_int() &&
        extra_volatile_ptr == expected_int();
}
