#include "Extra.h"
_Atomic(unsigned char) extra_u8 = 2;
_Atomic(short) extra_i16 = -3;
_Atomic(unsigned int*) extra_u32_ptr = 0;
_Atomic(short*) extra_i16_ptr = 0;
_Atomic(long long*) extra_i64_ptr = 0;
_Atomic(double*) extra_f64_ptr = 0;
_Atomic(const int*) extra_const_ptr = 0;
_Atomic(volatile int*) extra_volatile_ptr = 0;
static unsigned int data_u32 = 73;
static short data_i16 = 74;
static long long data_i64 = 75;
static double data_f64 = 76.0;
static int data_int = 77;
unsigned int* expected_u32(void) { return &data_u32; }
short* expected_i16(void) { return &data_i16; }
long long* expected_i64(void) { return &data_i64; }
double* expected_f64(void) { return &data_f64; }
int* expected_int(void) { return &data_int; }
int verify_extra(void)
{
    return extra_u8 == 42 && extra_i16 == -43 && extra_u32_ptr == &data_u32 &&
        extra_i16_ptr == &data_i16 && extra_i64_ptr == &data_i64 &&
        extra_f64_ptr == &data_f64 && extra_const_ptr == &data_int &&
        extra_volatile_ptr == &data_int && data_int == 78;
}
