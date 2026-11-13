extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Other p) { if (p.w!=40 || p.v!=50) { Small fail{-99,-99,-99}; return fail; } Small r{(float)(p.x+1),(float)p.y,p.z+3}; p.x=999; p.w=999; return r; }
Callback fetchCallback() { return most; }
}
