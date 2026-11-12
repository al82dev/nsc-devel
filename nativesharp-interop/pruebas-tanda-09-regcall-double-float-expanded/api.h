#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct PairDouble { double x, y, z; } PairDouble;
typedef struct PairFloat { float x, y; } PairFloat;
API int __attribute__((regcall)) sum_pair(PairDouble value, int bias, PairFloat extra);
typedef int (__attribute__((regcall)) *SumFn)(PairDouble, int, PairFloat);
API SumFn get_sum(void);
