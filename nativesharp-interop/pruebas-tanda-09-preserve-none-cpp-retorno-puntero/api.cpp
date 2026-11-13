#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { int* __attribute__((preserve_none)) scalar(float x,int* p) { *p += x == -3.5f ? 7 : -2; return p; } Callback fetchCallback() { return scalar; } }
