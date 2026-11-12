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
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, int, float, double);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, int, float, double);
API Nested __attribute__((regcall)) nested(Nested value, int delta, float factor, double extra);
API Array __attribute__((regcall)) array(Array value, int delta, float factor, double extra);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
