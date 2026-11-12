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
using NestedFn = Nested (__attribute__((regcall)) *)(OtherNested, Nested, int);
using ArrayFn = Array (__attribute__((regcall)) *)(OtherArray, Array, int);
API Nested __attribute__((regcall)) nested(OtherNested other, Nested value, int bias);
API Array __attribute__((regcall)) array(OtherArray other, Array value, int bias);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
