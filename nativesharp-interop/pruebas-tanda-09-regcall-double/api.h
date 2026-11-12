#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef double (__attribute__((regcall)) *T09DoubleFn)(double, int*, double);
API double __attribute__((regcall)) t09_double_transform(double a, int* b, double c);
API T09DoubleFn t09_double_get(void);
