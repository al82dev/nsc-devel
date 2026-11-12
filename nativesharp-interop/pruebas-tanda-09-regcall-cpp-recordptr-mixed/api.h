#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T09HfaCpp {
struct Box { int x; };
struct Inner { float x, y; };
struct Nested { Inner pair; float z; };
struct Array { float values[3]; };
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, Box*);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, Box*);
API Nested __attribute__((regcall)) nested(Nested value, Box* box);
API Array __attribute__((regcall)) array(Array value, Box* box);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
