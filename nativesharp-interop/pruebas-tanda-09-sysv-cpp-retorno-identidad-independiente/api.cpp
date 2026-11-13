extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Other p) { Small r{p.x+1,p.y,p.z+3}; return r; }
Callback fetchCallback() { return most; }
}
