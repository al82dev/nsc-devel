#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09Wide {
struct PairDouble { float x; double y; };
using SumFn = int* (__attribute__((regcall)) *)(PairDouble, int*);
API int* __attribute__((regcall)) sum(PairDouble value, int* out);
API SumFn fetch();
}
