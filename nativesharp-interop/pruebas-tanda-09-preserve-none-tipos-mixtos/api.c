#include "api.h"
__declspec(selectany) int _fltused = 0;
int __attribute__((preserve_none)) t09_regcall(short x) { return (int)x - 1000; }
T09SC t09_get_sc(void) { return t09_regcall; }
unsigned int __attribute__((preserve_none)) t09_uc(unsigned char x) { return (unsigned int)x + 4000000000u; }
T09UC t09_get_uc(void) { return t09_uc; }
long long __attribute__((preserve_none)) t09_sh(int x) { return (long long)x - 1099511627776LL; }
T09SH t09_get_sh(void) { return t09_sh; }
double __attribute__((preserve_none)) t09_us(float x) { return (double)x + 1.25; }
T09US t09_get_us(void) { return t09_us; }
