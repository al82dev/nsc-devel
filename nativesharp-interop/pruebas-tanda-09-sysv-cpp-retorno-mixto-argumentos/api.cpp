extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, int bias, float extra) { p.x+=bias; p.z+=(int)extra; return p; }
Callback fetchCallback() { return most; }
}
