#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09Compact {
struct Pair { float x, y; };
struct Triple { Pair pair; float z; };
struct Result { double x, y; };
using CombineFn = Result (__attribute__((regcall)) *)(int);
API Result __attribute__((regcall)) combine(int bias);
API CombineFn fetch();
}


