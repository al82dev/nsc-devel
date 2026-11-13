extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, int bias, Small q) { if (q.y!=3) { p.z=-99; return p; } p.x+=q.x; p.z+=bias+q.z; q.x=999; q.y=999; q.z=999; return p; }
Callback fetchCallback() { return most; }
}
