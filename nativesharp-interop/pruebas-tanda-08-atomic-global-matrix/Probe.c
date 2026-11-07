#include "Api.h"
void probe_write(void)
{
    value_u32 = 42; value_i64 = 43; value_u64 = 44;
    value_i8 = -45; value_u16 = 46;
    value_f32 = 3.5f; value_f64 = 4.5;
    value_bool = 1; value_pointer = expected_pointer();
}
int probe_read(void)
{
    return value_u32 == 42 && value_i64 == 43 && value_u64 == 44 &&
        value_i8 == -45 && value_u16 == 46 && value_f32 == 3.5f &&
        value_f64 == 4.5 && value_bool && value_pointer == expected_pointer();
}
