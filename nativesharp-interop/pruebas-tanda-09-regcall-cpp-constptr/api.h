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
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, const int*, int*);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, const int*, int*);
API Nested __attribute__((regcall)) nested(Nested value, const int* input, int* out);
API Array __attribute__((regcall)) array(Array value, const int* input, int* out);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
