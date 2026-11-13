#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { int __attribute__((preserve_none)) scalar(float f,Small a,Large b) { a.value+=2; b.x+=3; return a.value+b.x+b.y+b.z+(f<0 ? 100 : 200); } Callback fetchCallback() { return scalar; } }
