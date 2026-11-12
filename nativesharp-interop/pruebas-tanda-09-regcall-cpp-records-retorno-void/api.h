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
struct OtherNested { Inner pair; };
struct OtherArray { float values[2]; };
using NestedFn = void (__attribute__((regcall)) *)(Nested, int, OtherNested, int*);
using ArrayFn = void (__attribute__((regcall)) *)(Array, int, OtherArray, int*);
API void __attribute__((regcall)) nested(Nested value, int bias, OtherNested other, int* out);
API void __attribute__((regcall)) array(Array value, int bias, OtherArray other, int* out);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
