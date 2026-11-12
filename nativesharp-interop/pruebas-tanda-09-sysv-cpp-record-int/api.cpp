#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, int bias) { p.x+=1; p.y+=bias; p.z+=3; p.w+=4; p.v+=5; p.u+=6; return p; }
Callback fetchCallback() { return most; }
}
