extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Other p) { if (p.y!=40 || !p.x[2]) { Small fail{-99,-99,-99}; return fail; } Small r{p.x[0] ? 11.0f : 12.0f,p.x[1] ? 21.0f : 20.0f,p.z+3}; p.x[0]=false; return r; }
Callback fetchCallback() { return most; }
}
