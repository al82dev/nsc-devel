#include "api.h"
__declspec(selectany) int _fltused = 0;
float __attribute__((preserve_none)) t09_regcall(float x) { return x + 1; }
T09SC t09_get_sc(void) { return t09_regcall; }
unsigned int __attribute__((preserve_none)) t09_uc(unsigned int x) { return x + 1; }
T09UC t09_get_uc(void) { return t09_uc; }
unsigned long long __attribute__((preserve_none)) t09_sh(unsigned long long x) { return x + 1; }
T09SH t09_get_sh(void) { return t09_sh; }
double __attribute__((preserve_none)) t09_us(double x) { return x + 1; }
T09US t09_get_us(void) { return t09_us; }
