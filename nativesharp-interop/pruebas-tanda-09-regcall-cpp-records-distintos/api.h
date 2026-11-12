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
struct OtherNested { Inner pair; float z; };
struct OtherArray { float values[3]; };
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, OtherNested, int);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, OtherArray, int);
API Nested __attribute__((regcall)) nested(Nested value, OtherNested other, int bias);
API Array __attribute__((regcall)) array(Array value, OtherArray other, int bias);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
