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
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, long, unsigned long, double);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, long, unsigned long, double);
API Nested __attribute__((regcall)) nested(Nested value, long delta, unsigned long factor, double extra);
API Array __attribute__((regcall)) array(Array value, long delta, unsigned long factor, double extra);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
