#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct PairDouble { float x; double y; } PairDouble;
API float __attribute__((regcall)) sum_pair(PairDouble value, int bias);
typedef float (__attribute__((regcall)) *SumFn)(PairDouble, int);
API SumFn get_sum(void);
