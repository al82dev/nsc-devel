extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Other p) { Small r{p.x ? 11.0f : 12.0f,(float)p.y,p.z+3}; p.x=!p.x; return r; }
Callback fetchCallback() { return most; }
}
