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
struct OtherNested { float x; int y; };
struct OtherArray { float x; int y; };
using NestedFn = int (__attribute__((regcall)) *)(Nested, int, OtherNested);
using ArrayFn = int (__attribute__((regcall)) *)(Array, int, OtherArray);
API int __attribute__((regcall)) nested(Nested value, int bias, OtherNested other);
API int __attribute__((regcall)) array(Array value, int bias, OtherArray other);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
