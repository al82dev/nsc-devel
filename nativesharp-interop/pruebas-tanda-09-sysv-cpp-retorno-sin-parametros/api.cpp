extern "C" { __declspec(selectany) int _fltused = 0; }
#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most() { Small r{11.0f,20.0f,33}; return r; }
Callback fetchCallback() { return most; }
}
