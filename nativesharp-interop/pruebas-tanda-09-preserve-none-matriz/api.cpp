#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0;
float __attribute__((preserve_none)) t09_f(float x) { return x+1.0f; }
int __attribute__((preserve_none)) t09_i(int x) { return x-1; }
unsigned int __attribute__((preserve_none)) t09_u(unsigned int x) { return x+1u; }
long long __attribute__((preserve_none)) t09_l(long long x) { return x-1; }
unsigned long long __attribute__((preserve_none)) t09_ul(unsigned long long x) { return x+1ull; }
double __attribute__((preserve_none)) t09_d(double x) { return x+1.0; }
bool __attribute__((preserve_none)) t09_b(bool x) { return !x; }
void __attribute__((preserve_none)) t09_p(int* x) { *x=27; }
}
