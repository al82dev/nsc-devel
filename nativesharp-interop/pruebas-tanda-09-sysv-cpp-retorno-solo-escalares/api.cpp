extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(int bias, float extra) { Small r{(float)bias,extra,bias+22}; return r; }
Callback fetchCallback() { return most; }
}
