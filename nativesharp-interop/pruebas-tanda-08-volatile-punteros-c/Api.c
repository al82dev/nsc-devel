#include "Api.h"
static volatile signed char s8;
static volatile unsigned char u8;
static volatile unsigned short u16;
static volatile unsigned int u32;
static volatile unsigned long long u64;
static volatile float f32;
static volatile double f64;
static volatile _Bool bv;
volatile signed char* p_s8 = &s8;
volatile unsigned char* p_u8 = &u8;
volatile unsigned short* p_u16 = &u16;
volatile unsigned int* p_u32 = &u32;
volatile unsigned long long* p_u64 = &u64;
volatile float* p_f32 = &f32;
volatile double* p_f64 = &f64;
volatile _Bool* a_bool[2] = {&bv, &bv};
volatile signed char* a_s8[2] = {&s8, &s8};
volatile unsigned char* a_u8[2] = {&u8, &u8};
volatile unsigned short* a_u16[2] = {&u16, &u16};
volatile unsigned int* a_u32[2] = {&u32, &u32};
volatile unsigned long long* a_u64[2] = {&u64, &u64};
volatile float* a_f32[2] = {&f32, &f32};
volatile double* a_f64[2] = {&f64, &f64};
int c_check(void) { return s8 == 11 && u8 == 12 && u16 == 13 && u32 == 14 && u64 == 15 && f32 == 16 && f64 == 17; }
void c_probe(void) {
    *p_s8 = *p_s8; *p_u8 = *p_u8; *p_u16 = *p_u16; *p_u32 = *p_u32;
    *p_u64 = *p_u64; *p_f32 = *p_f32; *p_f64 = *p_f64;
    *a_bool[0] = *a_bool[0]; *a_s8[0] = *a_s8[0]; *a_u8[0] = *a_u8[0];
    *a_u16[0] = *a_u16[0]; *a_u32[0] = *a_u32[0]; *a_u64[0] = *a_u64[0];
    *a_f32[0] = *a_f32[0]; *a_f64[0] = *a_f64[0];
}
