extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Other p) { if (p.y!=40) { Small fail{-99,-99,-99}; return fail; } Small r{(float)(p.x[0]+1),(float)p.x[1],p.x[2]+3}; p.x[0]=999; return r; }
Callback fetchCallback() { return most; }
}
