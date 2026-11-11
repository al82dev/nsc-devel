#include "api.h"
__declspec(selectany) int _fltused = 0;
int sysv_value;
int __attribute__((sysv_abi)) sysv_mix(int a, int b, int c, int d, int e, int f) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6;
}
void __attribute__((sysv_abi)) sysv_set(int value) { sysv_value = value; }
double __attribute__((sysv_abi)) sysv_fp(double a, float b, int c, double d) { return a + b + c + d; }
float __attribute__((sysv_abi)) sysv_float(float value) { return value + 1.0f; }
int* __attribute__((sysv_abi)) sysv_pointer(int* value, int delta) { *value += delta; return value; }
double* __attribute__((sysv_abi)) sysv_double_pointer(double* value) { *value += 0.5; return value; }
void* __attribute__((sysv_abi)) sysv_void_pointer(void* value) { return value; }
int** __attribute__((sysv_abi)) sysv_pointer_pointer(int** value) { return value; }
long long __attribute__((sysv_abi)) sysv_wide(long long value, unsigned int delta) { return value + delta; }
unsigned int __attribute__((sysv_abi)) sysv_unsigned(unsigned int value) { return value + 7; }
unsigned long long __attribute__((sysv_abi)) sysv_unsigned64(unsigned long long value) { return value + 1; }
signed char __attribute__((sysv_abi)) sysv_s8(signed char value) { return value + 1; }
unsigned char __attribute__((sysv_abi)) sysv_u8(unsigned char value) { return value + 1; }
short __attribute__((sysv_abi)) sysv_s16(short value) { return value + 1; }
unsigned short __attribute__((sysv_abi)) sysv_u16(unsigned short value) { return value + 1; }
_Bool __attribute__((sysv_abi)) sysv_bool(_Bool value) { return !value; }
