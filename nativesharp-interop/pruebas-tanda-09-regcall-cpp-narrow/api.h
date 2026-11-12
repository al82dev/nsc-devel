#pragma once
#ifdef __cplusplus
typedef bool T09Bool;
#else
typedef _Bool T09Bool;
#endif
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09HfaCpp {
struct Inner { float x, y; };
struct Nested { Inner pair; float z; };
struct Array { float values[3]; };
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, signed char, unsigned char, short, unsigned short, T09Bool);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, signed char, unsigned char, short, unsigned short, T09Bool);
API Nested __attribute__((regcall)) nested(Nested value, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last);
API Array __attribute__((regcall)) array(Array value, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
