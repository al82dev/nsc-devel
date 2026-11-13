extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Other p) { Small r{(float)(p.x+1),(float)p.y,p.z+3}; return r; }
Callback fetchCallback() { return most; }
}
