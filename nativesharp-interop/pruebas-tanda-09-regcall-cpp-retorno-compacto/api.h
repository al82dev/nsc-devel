#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09Compact {
struct Pair { float x, y; };
struct Triple { Pair pair; float z; };
struct Result { int x, y; };
using CombineFn = Result (__attribute__((regcall)) *)(Triple, int);
API Result __attribute__((regcall)) combine(Triple value, int bias);
API CombineFn fetch();
}
