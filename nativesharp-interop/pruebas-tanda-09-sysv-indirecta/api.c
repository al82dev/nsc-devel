#include "api.h"
__declspec(selectany) int _fltused = 0;
static int __attribute__((sysv_abi)) weighted(int a, int b, int c, int d, int e, int f) {
    return a + 2*b + 3*c + 4*d + 5*e + 6*f;
}
T09SysvCallback t09_get_sysv_callback(void) { return weighted; }
static double __attribute__((sysv_abi)) float_mix(double a, float b, int c, double d) { return a + b + c + d; }
static float __attribute__((sysv_abi)) float_one(float x) { return x + 1.0f; }
static long long __attribute__((sysv_abi)) wide(long long x, unsigned int y) { return x + y; }
static unsigned int __attribute__((sysv_abi)) uint_one(unsigned int x) { return x + 7U; }
static unsigned long long __attribute__((sysv_abi)) ulong_one(unsigned long long x, unsigned int y) { return x + y; }
static int* __attribute__((sysv_abi)) pointer(int* x, int y) { *x += y; return x; }
static void __attribute__((sysv_abi)) set_pointer(int* x) { *x = 47; }
T09FloatCallback t09_get_float_callback(void) { return float_mix; }
T09FloatOneCallback t09_get_float_one_callback(void) { return float_one; }
T09WideCallback t09_get_wide_callback(void) { return wide; }
T09UIntCallback t09_get_uint_callback(void) { return uint_one; }
T09ULongCallback t09_get_ulong_callback(void) { return ulong_one; }
T09PointerCallback t09_get_pointer_callback(void) { return pointer; }
T09VoidCallback t09_get_void_callback(void) { return set_pointer; }
int t09_normal(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
