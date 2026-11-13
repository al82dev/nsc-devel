#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { Small __attribute__((preserve_none)) scalar(float x,Small p) { p.value += x == -3.5f ? 7 : -2; return p; } Callback fetchCallback() { return scalar; } }
