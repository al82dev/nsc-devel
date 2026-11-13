#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { int __attribute__((preserve_none)) scalar(float x) { return x == -3.5f ? -71 : 42; } Callback fetchCallback() { return scalar; } }
