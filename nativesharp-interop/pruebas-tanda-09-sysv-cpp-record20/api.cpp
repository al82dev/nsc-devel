#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p) { p.x+=1; p.z+=3; p.w+=4; p.v+=5; return p; }
Callback fetchCallback() { return most; }
}
