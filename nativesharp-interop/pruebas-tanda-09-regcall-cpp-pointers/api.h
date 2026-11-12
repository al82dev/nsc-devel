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
using NestedFn = Nested (__attribute__((regcall)) *)(Nested, unsigned char*, short*, double*, void*);
using ArrayFn = Array (__attribute__((regcall)) *)(Array, unsigned char*, short*, double*, void*);
API Nested __attribute__((regcall)) nested(Nested value, unsigned char* p, short* q, double* r, void* identity);
API Array __attribute__((regcall)) array(Array value, unsigned char* p, short* q, double* r, void* identity);
API NestedFn fetchNested();
API ArrayFn fetchArray();
}
