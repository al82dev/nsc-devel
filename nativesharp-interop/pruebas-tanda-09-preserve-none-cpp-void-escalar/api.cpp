#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { static int state=0; void __attribute__((preserve_none)) scalar(float x,int y) { state = x == -3.5f ? y*10 : y*100; } Callback fetchCallback() { return scalar; } int read() { return state; } }
