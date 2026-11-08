#include "Api.h"
static volatile signed char s8;
static volatile unsigned char u8;
static volatile unsigned short u16;
static volatile unsigned int u32;
static volatile unsigned long long u64;
static volatile float f32;
static volatile double f64;
volatile signed char* p_s8 = &s8;
volatile unsigned char* p_u8 = &u8;
volatile unsigned short* p_u16 = &u16;
volatile unsigned int* p_u32 = &u32;
volatile unsigned long long* p_u64 = &u64;
volatile float* p_f32 = &f32;
volatile double* p_f64 = &f64;
int c_check(void) { return s8 == 11 && u8 == 12 && u16 == 13 && u32 == 14 && u64 == 15 && f32 == 16 && f64 == 17; }
void c_probe(void) {
    *p_s8 = *p_s8; *p_u8 = *p_u8; *p_u16 = *p_u16; *p_u32 = *p_u32;
    *p_u64 = *p_u64; *p_f32 = *p_f32; *p_f64 = *p_f64;
}
