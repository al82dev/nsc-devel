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
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, int,int,int,int,int,int,int,int);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, int,int,int,int,int,int,int,int);
API Nested __attribute__((regcall)) nested(Nested value, int b, int c, int d, int e, int f, int g, int h, int i);
API Array __attribute__((regcall)) array(Array value, int b, int c, int d, int e, int f, int g, int h, int i);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
