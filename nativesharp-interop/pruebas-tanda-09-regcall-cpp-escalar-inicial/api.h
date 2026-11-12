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
using NestedFn = Nested (__attribute__((regcall)) *)(int, Nested, OtherNested);
using ArrayFn = Array (__attribute__((regcall)) *)(int, Array, OtherArray);
API Nested __attribute__((regcall)) nested(int bias, Nested value, OtherNested other);
API Array __attribute__((regcall)) array(int bias, Array value, OtherArray other);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
