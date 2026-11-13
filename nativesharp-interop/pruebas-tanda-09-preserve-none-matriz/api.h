#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
extern "C" {
API_STORAGE float __attribute__((preserve_none)) t09_f(float x);
API_STORAGE int __attribute__((preserve_none)) t09_i(int x);
API_STORAGE unsigned int __attribute__((preserve_none)) t09_u(unsigned int x);
API_STORAGE long long __attribute__((preserve_none)) t09_l(long long x);
API_STORAGE unsigned long long __attribute__((preserve_none)) t09_ul(unsigned long long x);
API_STORAGE double __attribute__((preserve_none)) t09_d(double x);
API_STORAGE bool __attribute__((preserve_none)) t09_b(bool x);
API_STORAGE void __attribute__((preserve_none)) t09_p(int* x);
}
