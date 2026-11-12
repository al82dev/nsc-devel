#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(int leading, Small p, int bias) { p.x+=1; p.y+=bias+leading*2; p.z+=3; p.w+=4; p.v+=5; p.u+=6; return p; }
Callback fetchCallback() { return most; }
}
