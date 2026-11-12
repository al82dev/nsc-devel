#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09DoubleCpp {
using Callback = double (__attribute__((regcall)) *)(double, int*, double);
API double __attribute__((regcall)) transform(double a, int* b, double c);
API Callback fetch();
}
