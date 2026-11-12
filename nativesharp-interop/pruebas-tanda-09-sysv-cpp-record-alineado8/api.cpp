#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, int bias, Other q) { p.x+=q.x; p.y+=bias+(q.x>4294967296LL?10:0); p.z+=q.z; q.x=999; p.w+=4; p.v+=5; p.u+=6; return p; }
Callback fetchCallback() { return most; }
}
