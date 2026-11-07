#include "Widths.h"
_Atomic(signed char*) width_i8 = 0;
_Atomic(unsigned char*) width_u8 = 0;
_Atomic(unsigned short*) width_u16 = 0;
_Atomic(unsigned long long*) width_u64 = 0;
static signed char i8[2] = {-10, -11};
static unsigned char u8[2] = {20, 21};
static unsigned short u16[2] = {30, 31};
static unsigned long long u64[2] = {40, 41};
signed char* width_i8_expected(void) { return i8; }
unsigned char* width_u8_expected(void) { return u8; }
unsigned short* width_u16_expected(void) { return u16; }
unsigned long long* width_u64_expected(void) { return u64; }
int widths_check(void)
{
    return width_i8 == i8 + 1 && width_u8 == u8 + 1 &&
        width_u16 == u16 + 1 && width_u64 == u64 + 1;
}
