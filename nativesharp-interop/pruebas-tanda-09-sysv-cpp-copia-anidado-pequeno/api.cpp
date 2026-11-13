extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Other p) { Small r{(float)(p.x.a+p.x.b),(float)p.x.b,p.z+3}; p.x.a=999; p.z=999; return r; }
Callback fetchCallback() { return most; }
}
