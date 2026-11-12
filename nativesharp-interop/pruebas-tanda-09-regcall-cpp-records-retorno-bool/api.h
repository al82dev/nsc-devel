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
using NestedFn = bool (__attribute__((regcall)) *)(Nested, int, OtherNested);
using ArrayFn = bool (__attribute__((regcall)) *)(Array, int, OtherArray);
API bool __attribute__((regcall)) nested(Nested value, int bias, OtherNested other);
API bool __attribute__((regcall)) array(Array value, int bias, OtherArray other);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
