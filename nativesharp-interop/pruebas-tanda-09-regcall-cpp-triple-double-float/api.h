#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09Wide {
struct PairDouble { double x, y, z; };
using SumFn = float (__attribute__((regcall)) *)(PairDouble, int);
API float __attribute__((regcall)) sum(PairDouble value, int bias);
API SumFn fetch();
}
