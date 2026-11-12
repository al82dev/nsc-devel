#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09HfaCpp {
struct Inner { float x, y; };
struct Nested { Inner pair; float z; };
struct Array { float values[3]; };
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, long long, unsigned int, unsigned long long, int*, double);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, long long, unsigned int, unsigned long long, int*, double);
API Nested __attribute__((regcall)) nested(Nested value, long long delta, unsigned int factor, unsigned long long extra, int* p, double more);
API Array __attribute__((regcall)) array(Array value, long long delta, unsigned int factor, unsigned long long extra, int* p, double more);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
