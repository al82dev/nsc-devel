#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { int __attribute__((preserve_none)) scalar(float x,float y,float z) { return x == -3.5f && y == 2.0f && z == 8.0f ? -71 : 42; } }
