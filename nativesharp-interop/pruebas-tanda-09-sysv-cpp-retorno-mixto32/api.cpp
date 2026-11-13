extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p) { p.x+=1; p.z+=3; return p; }
Callback fetchCallback() { return most; }
}
