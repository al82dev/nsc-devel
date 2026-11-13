#include "api.h"
namespace T09Cpp {
Small __attribute__((sysv_abi)) most(Small p, signed char a, unsigned char b, short c, unsigned short d, bool flag) {
 p.x+=(a==-7?1:0); p.y+=(b==250?2:0); p.z+=(c==-1000?3:0); p.w+=(d==65000?4:0); p.v+=5; p.u+=(flag?6:7); return p;
}
Callback fetchCallback() { return most; }
}