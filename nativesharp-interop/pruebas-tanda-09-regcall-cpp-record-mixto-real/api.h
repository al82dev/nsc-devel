#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09Wide {
struct PairDouble { float x; double y; };
using SumFn = double (__attribute__((regcall)) *)(PairDouble, int);
API double __attribute__((regcall)) sum(PairDouble value, int bias);
API SumFn fetch();
}

